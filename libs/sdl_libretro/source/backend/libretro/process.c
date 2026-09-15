#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "core.h"
#include "ipc.h"

extern char **environ;

typedef enum {
    PROC_IDLE = 0,
    PROC_PENDING,
    PROC_SPAWNED,
    PROC_CONNECTED,
    PROC_STOPPING,
    PROC_FAILED,
} proc_phase_t;

static struct {
    proc_phase_t phase;
    pid_t        pid;
    int          listen_fd;
    int          client_fd;
    bool         term_sent;
    bool         background;
    bool         preload;
    unsigned     spawn_count;
    uint16_t     win_w;
    uint16_t     win_h;
    uint64_t     deadline_ms;
    char         sock_path[108];
    char         exec_path[PATH_MAX];
    char         bin_path[PATH_MAX];
    bool         bin_on_path;
    char         ld_extra[PATH_MAX];
    char         shim_path[PATH_MAX];
    char         error[256];

    int          fb_fd;
    void        *fb_map;
    size_t       fb_size;
    unsigned     fb_w;
    unsigned     fb_h;
    unsigned     fb_bpp;
    bool         fb_new;
    bool         fb_geometry;
    char         fb_name[64];
} s = { .listen_fd = -1, .client_fd = -1, .preload = true, .fb_fd = -1 };

static void fb_release(void) {
    if (s.fb_map) munmap(s.fb_map, s.fb_size);
    if (s.fb_fd >= 0) close(s.fb_fd);
    s.fb_map = NULL;
    s.fb_fd = -1;
    s.fb_size = 0;
    s.fb_w = s.fb_h = s.fb_bpp = 0;
    s.fb_new = false;
    s.fb_name[0] = 0;
}

static void fb_attach(const char *name, unsigned w, unsigned h, unsigned bpp) {
    fb_release();
    if (!name || !name[0] || !w || !h) return;

    s.fb_fd = shm_open(name, O_RDONLY, 0);
    if (s.fb_fd < 0) {
        fprintf(stderr, "[sdl2] shm_open %s failed: %s\n", name, strerror(errno));
        return;
    }

    s.fb_size = (size_t)w * h * bpp;
    s.fb_map  = mmap(NULL, s.fb_size, PROT_READ, MAP_SHARED, s.fb_fd, 0);
    if (s.fb_map == MAP_FAILED) {
        s.fb_map = NULL;
        fb_release();
        return;
    }

    s.fb_w = w;
    s.fb_h = h;
    s.fb_bpp = bpp;
    s.fb_geometry = true;
    snprintf(s.fb_name, sizeof(s.fb_name), "%s", name);
    fprintf(stderr, "[sdl2] framebuffer %ux%u %ubpp via %s\n", w, h, bpp * 8, name);
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

const char *process_error(void) {
    return s.error;
}

void process_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s.error, sizeof(s.error), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[sdl2] %s\n", s.error);
}

static void sockets_close(void) {
    if (s.client_fd >= 0) close(s.client_fd);
    if (s.listen_fd >= 0) close(s.listen_fd);
    s.client_fd = -1;
    s.listen_fd = -1;
    if (s.sock_path[0]) unlink(s.sock_path);
    s.sock_path[0] = '\0';
}

static bool listen_open(void) {
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(s.sock_path, sizeof(s.sock_path), DOPO_SOCK_FMT,
             (int)getpid(), ++s.spawn_count);
    unlink(s.sock_path);
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", s.sock_path);

    s.listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s.listen_fd < 0) {
        process_set_error("socket failed: %s", strerror(errno));
        return false;
    }
    if (bind(s.listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        process_set_error("bind %s failed: %s", s.sock_path, strerror(errno));
        sockets_close();
        return false;
    }
    if (listen(s.listen_fd, 1) != 0) {
        process_set_error("listen failed: %s", strerror(errno));
        sockets_close();
        return false;
    }
    return true;
}

static bool send_pkt(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg) {
    if (s.client_fd < 0) return false;
    dopo_ipc_pkt_t pkt = { type, flag, code, arg };
    ssize_t n = send(s.client_fd, &pkt, sizeof(pkt), MSG_NOSIGNAL | MSG_DONTWAIT);
    return n == (ssize_t)sizeof(pkt);
}

static void accept_client(void) {
    if (s.listen_fd < 0 || s.client_fd >= 0) return;
    int fd = accept4(s.listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd >= 0) s.client_fd = fd;
}

static void read_packets(void) {
    if (s.client_fd < 0) return;

    static uint8_t buf[DOPO_IPC_PKT_MAX];
    for (;;) {
        ssize_t n = recv(s.client_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n >= (ssize_t)sizeof(dopo_ipc_pkt_t)) {
            dopo_ipc_pkt_t pkt;
            memcpy(&pkt, buf, sizeof(pkt));
            const uint8_t *payload = buf + sizeof(pkt);
            size_t         paylen  = (size_t)n - sizeof(pkt);
            switch (pkt.type) {
                case DOPO_IPC_PKT_HELLO:
                    if (s.phase == PROC_SPAWNED) {
                        s.phase      = PROC_CONNECTED;
                        s.background = true;

                    }
                    fprintf(stderr, "[sdl2] %s connected (pid %u)\n", DOPO_SDL2_SHIM_NAME, pkt.arg);
                    break;
                case DOPO_IPC_PKT_WINDOW:
                    s.win_w = pkt.code;
                    s.win_h = (uint16_t)pkt.arg;
                    break;
                case DOPO_IPC_PKT_AUDIO_CFG:
                    audio_configure(pkt.arg, pkt.code);
                    break;
                case DOPO_IPC_PKT_AUDIO: {
                    size_t want = (size_t)pkt.code * pkt.flag * sizeof(int16_t);
                    if (pkt.flag && want && want <= paylen) {
                        audio_push((const int16_t *)(const void *)payload, pkt.code);
                    }
                    break;
                }
                case DOPO_IPC_PKT_AUDIO_STOP:
                    audio_stop();
                    break;
                case DOPO_IPC_PKT_FB_INIT: {
                    char name[64];
                    size_t n = paylen < sizeof(name) ? paylen : sizeof(name) - 1;
                    memcpy(name, payload, n);
                    name[n] = '\0';
                    fb_attach(name, pkt.code, pkt.arg,
                              pkt.flag == DOPO_IPC_FORMAT_RGB565 ? 2 : 4);
                    break;
                }
                case DOPO_IPC_PKT_FB_FRAME:
                    if (s.fb_map) s.fb_new = true;
                    break;
                case DOPO_IPC_PKT_BYE:
                    fb_release();
                    audio_reset();
                    close(s.client_fd);
                    s.client_fd = -1;
                    return;
                default:
                    break;
            }
            continue;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            close(s.client_fd);
            s.client_fd = -1;
        }
        return;
    }
}

static bool env_is(const char *entry, const char *key) {
    size_t len = strlen(key);
    return strncmp(entry, key, len) == 0 && entry[len] == '=';
}

static char *env_join(const char *key, const char *head, const char *tail) {
    size_t cap = strlen(key) + strlen(head) + (tail ? strlen(tail) : 0) + 3;
    char  *out = malloc(cap);
    if (!out) return NULL;
    if (tail && tail[0]) {
        snprintf(out, cap, "%s=%s:%s", key, head, tail);
    } else {
        snprintf(out, cap, "%s=%s", key, head);
    }
    return out;
}

static char **env_build(void) {
    size_t count = 0;
    while (environ[count]) count++;

    char **env = calloc(count + 4, sizeof(char *));
    if (!env) return NULL;

    const char *old_ld_path = NULL;
    const char *old_preload = NULL;
    size_t      n           = 0;

    for (size_t i = 0; i < count; i++) {
        const char *e = environ[i];
        if (env_is(e, "LD_LIBRARY_PATH")) {
            old_ld_path = strchr(e, '=') + 1;
            continue;
        }
        if (env_is(e, "LD_PRELOAD")) {
            old_preload = strchr(e, '=') + 1;
            continue;
        }
        if (env_is(e, DOPO_IPC_ENV_SOCKET)) continue;
        env[n++] = strdup(e);
    }

    char shim_dir[PATH_MAX];
    snprintf(shim_dir, sizeof(shim_dir), "%s", s.shim_path);
    char *slash = strrchr(shim_dir, '/');
    if (slash) *slash = '\0';

    if (s.ld_extra[0]) {
        char head[PATH_MAX * 2];
        snprintf(head, sizeof(head), "%s:%s", shim_dir, s.ld_extra);
        env[n++] = env_join("LD_LIBRARY_PATH", head, old_ld_path);
    } else {
        env[n++] = env_join("LD_LIBRARY_PATH", shim_dir, old_ld_path);
    }
    if (s.preload) {
        env[n++] = env_join("LD_PRELOAD", s.shim_path, old_preload);
    } else if (old_preload) {
        env[n++] = env_join("LD_PRELOAD", old_preload, NULL);
    }
    env[n++] = env_join(DOPO_IPC_ENV_SOCKET, s.sock_path, NULL);
    env[n]   = NULL;
    return env;
}

static void env_free(char **env) {
    if (!env) return;
    for (size_t i = 0; env[i]; i++) free(env[i]);
    free(env);
}

static bool spawn(void) {
    if (!listen_open()) return false;

    char **env = env_build();
    if (!env) {
        process_set_error("out of memory building environment");
        sockets_close();
        return false;
    }

    bool  executable    = access(s.exec_path, X_OK) == 0;
    char *argv_direct[] = { s.exec_path, NULL };
    char *argv_shell[]  = { "/bin/sh", s.exec_path, NULL };

    char *argv_bin[]    = { s.bin_path, s.exec_path, NULL };

    pid_t pid = fork();
    if (pid < 0) {
        process_set_error("fork failed: %s", strerror(errno));
        env_free(env);
        sockets_close();
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (s.bin_path[0]) {
            if (s.bin_on_path) execvpe(s.bin_path, argv_bin, env);
            else               execve(s.bin_path, argv_bin, env);
            _exit(127);
        }
        if (executable) execve(s.exec_path, argv_direct, env);
        execve("/bin/sh", argv_shell, env);
        _exit(127);
    }

    env_free(env);
    s.pid         = pid;
    s.phase       = PROC_SPAWNED;
    s.term_sent   = false;
    s.deadline_ms = now_ms() + 20000;
    fprintf(stderr, "[sdl2] spawned pid %d: %s\n", (int)pid, s.exec_path);
    return true;
}

static void core_foreground(void) {
    if (!s.background) return;
    s.background = false;

}

static void on_exit_status(int status) {
    bool clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    int  code  = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);

    if (s.phase == PROC_SPAWNED) {
        process_set_error("%s exited before connecting (status %d)", s.exec_path, code);
        s.phase = PROC_FAILED;
    } else if (!clean && s.phase != PROC_STOPPING) {
        process_set_error("%s exited with status %d", s.exec_path, code);
        s.phase = PROC_FAILED;
    } else {
        fprintf(stderr, "[sdl2] pid %d exited (status %d)\n", (int)s.pid, code);
        s.phase = PROC_IDLE;
    }
    s.pid = 0;
    sockets_close();
    core_foreground();
}

bool process_request(const char *path, const char *shim_dir) {
    struct stat st;
    s.error[0] = '\0';

    if (!path || stat(path, &st) != 0) {
        process_set_error("not found: %s", path ? path : "(null)");
        return false;
    }
    if (!shim_dir || !shim_dir[0]) {
        process_set_error("could not resolve %s directory", DOPO_SDL2_SHIM_NAME);
        return false;
    }
    snprintf(s.shim_path, sizeof(s.shim_path), "%s/%s", shim_dir, DOPO_SDL2_SHIM_NAME);
    if (stat(s.shim_path, &st) != 0) {
        process_set_error("shim not found: %s", s.shim_path);
        return false;
    }

    const char *preload = option_get("sdl_preload");
    s.preload = !(preload && (preload[0] == '0' || preload[0] == 'n' || preload[0] == 'f'));

    s.bin_path[0]  = '\0';
    s.bin_on_path  = false;
    const char *bin = option_get("sdl_bin");
    if (bin && bin[0]) {
        if (!strchr(bin, '/')) {

            snprintf(s.bin_path, sizeof(s.bin_path), "%s", bin);
            s.bin_on_path = true;
        } else {
            path_resolve_rel(bin, path, s.bin_path, sizeof(s.bin_path));
            if (access(s.bin_path, X_OK) != 0) {
                process_set_error("bin not executable: %s", s.bin_path);
                return false;
            }
        }
    }

    s.ld_extra[0] = '\0';
    const char *ld = option_get("sdl_ld");
    if (ld && ld[0]) {
        char   list[PATH_MAX];
        size_t used = 0;
        snprintf(list, sizeof(list), "%s", ld);

        for (char *tok = strtok(list, ":"); tok; tok = strtok(NULL, ":")) {
            if (!tok[0]) continue;
            char one[PATH_MAX];
            path_resolve_rel(tok, path, one, sizeof(one));

            int w = snprintf(s.ld_extra + used, sizeof(s.ld_extra) - used,
                             "%s%s", used ? ":" : "", one);
            if (w < 0 || (size_t)w >= sizeof(s.ld_extra) - used) {
                process_set_error("ld path too long");
                return false;
            }
            used += (size_t)w;

            struct stat lst;
            if (stat(one, &lst) != 0) {
                fprintf(stderr, "[sdl2] warning: ld path does not exist: %s\n", one);
            }
        }
    }

    snprintf(s.exec_path, sizeof(s.exec_path), "%s", path);
    s.win_w = s.win_h = 0;
    s.phase = PROC_PENDING;
    return true;
}

void process_stop(bool force) {
    if (s.phase == PROC_PENDING || s.phase == PROC_FAILED) {
        s.phase = PROC_IDLE;
    }
    if (s.pid <= 0) {
        sockets_close();
        s.phase = PROC_IDLE;
        return;
    }
    if (force) {
        kill(-s.pid, SIGKILL);
        kill(s.pid, SIGKILL);
        waitpid(s.pid, NULL, 0);
        s.pid = 0;
        sockets_close();
        s.phase = PROC_IDLE;
        core_foreground();
        return;
    }
    if (s.phase != PROC_STOPPING) {
        send_pkt(DOPO_IPC_PKT_QUIT, 0, 0, 0);
        s.phase       = PROC_STOPPING;
        s.term_sent   = false;
        s.deadline_ms = now_ms() + 1500;
    }
}

void process_tick(void) {
    if (s.phase == PROC_PENDING) {
        if (!spawn()) s.phase = PROC_FAILED;
        return;
    }
    if (s.pid <= 0) return;

    int   status = 0;
    pid_t r      = waitpid(s.pid, &status, WNOHANG);
    if (r == s.pid) {
        on_exit_status(status);
        return;
    }

    accept_client();
    read_packets();

    uint64_t now = now_ms();
    if (s.phase == PROC_SPAWNED && now > s.deadline_ms) {
        process_set_error("timeout waiting for %s to connect", DOPO_SDL2_SHIM_NAME);
        kill(-s.pid, SIGKILL);
        s.phase = PROC_STOPPING;
        return;
    }
    if (s.phase == PROC_STOPPING && now > s.deadline_ms) {
        if (!s.term_sent) {
            kill(-s.pid, SIGTERM);
            s.term_sent   = true;
            s.deadline_ms = now + 2000;
        } else {
            kill(-s.pid, SIGKILL);
        }
    }
}

bool process_send_key(uint16_t scancode, uint32_t keycode, bool pressed) {
    if (s.phase != PROC_CONNECTED) return false;
    return send_pkt(DOPO_IPC_PKT_KEY, pressed ? 1 : 0, scancode, keycode);
}

bool process_send_pad(uint8_t pad, bool pressed) {
    if (s.phase != PROC_CONNECTED) return false;
    return send_pkt(DOPO_IPC_PKT_PAD, pressed ? 1 : 0, pad, 0);
}

bool process_is_running(void) {
    return s.phase == PROC_CONNECTED;
}

const void *process_frame(unsigned *w, unsigned *h, unsigned *bpp) {
    if (!s.fb_map || !s.fb_new) return NULL;
    s.fb_new = false;
    if (w)   *w = s.fb_w;
    if (h)   *h = s.fb_h;
    if (bpp) *bpp = s.fb_bpp;
    return s.fb_map;
}

bool process_frame_changed(void) {
    bool changed = s.fb_geometry;
    s.fb_geometry = false;
    return changed;
}

bool process_is_done(void) {
    return s.phase == PROC_IDLE || s.phase == PROC_FAILED;
}

bool process_has_failed(void) {
    return s.phase == PROC_FAILED;
}

__attribute__((destructor))
static void process_destroy(void) {
    if (s.pid > 0) {
        kill(-s.pid, SIGKILL);
        kill(s.pid, SIGKILL);
    }
    sockets_close();
}
