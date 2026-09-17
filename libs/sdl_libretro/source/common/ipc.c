#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "shim.h"

static int      s_fd = -1;
static uint64_t s_orphan_deadline;

void shim_ipc_connect(void) {
    const char *path = getenv(DOPO_IPC_ENV_SOCKET);
    if (!path || !path[0] || s_fd >= 0) return;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) return;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, SHIM_TAG " connect %s failed: %s\n", path, strerror(errno));
        close(fd);
        return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    s_fd = fd;
    shim_ipc_send(DOPO_IPC_PKT_HELLO, SHIM_API, 0, (uint32_t)getpid());
}

void shim_ipc_send(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg) {
    if (s_fd < 0) return;
    dopo_ipc_pkt_t pkt = { type, flag, code, arg };
    send(s_fd, &pkt, sizeof(pkt), MSG_NOSIGNAL | MSG_DONTWAIT);
}

void shim_ipc_send_blob(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg,
                        const void *payload, size_t bytes) {
    if (s_fd < 0) return;
    if (bytes > DOPO_IPC_PKT_MAX - sizeof(dopo_ipc_pkt_t)) return;

    uint8_t buf[DOPO_IPC_PKT_MAX];
    dopo_ipc_pkt_t pkt = { type, flag, code, arg };
    memcpy(buf, &pkt, sizeof(pkt));
    if (bytes && payload) memcpy(buf + sizeof(pkt), payload, bytes);
    send(s_fd, buf, sizeof(pkt) + bytes, MSG_NOSIGNAL | MSG_DONTWAIT);
}

void shim_ipc_close(void) {
    if (s_fd < 0) return;
    shim_ipc_send(DOPO_IPC_PKT_BYE, 0, 0, 0);
    close(s_fd);
    s_fd = -1;
}

static void ipc_lost(void) {
    close(s_fd);
    s_fd = -1;
    s_orphan_deadline = shim_now_ms() + 5000;
    fprintf(stderr, SHIM_TAG " host connection lost, requesting quit\n");
    shim_events_quit_request();
}

void shim_ipc_pump(void) {
    if (s_fd < 0) {
        if (s_orphan_deadline && shim_now_ms() > s_orphan_deadline) _exit(0);
        return;
    }
    dopo_ipc_pkt_t pkt;
    for (;;) {
        ssize_t n = recv(s_fd, &pkt, sizeof(pkt), MSG_DONTWAIT);
        if (n == (ssize_t)sizeof(pkt)) {
            switch (pkt.type) {
                case DOPO_IPC_PKT_KEY:
                    if (getenv(DOPO_ENV_DEBUG)) {
                        fprintf(stderr, SHIM_TAG " key scancode=%u sym=%u press=%u\n",
                                pkt.code, pkt.arg, pkt.flag);
                    }
                    shim_events_key(pkt.code, pkt.arg, pkt.flag != 0);
                    break;
                case DOPO_IPC_PKT_PAD:
                    if (getenv(DOPO_ENV_DEBUG)) {
                        fprintf(stderr, SHIM_TAG " pad button=%u press=%u\n",
                                pkt.code, pkt.flag);
                    }
                    shim_joystick_input((uint8_t)pkt.code, pkt.flag != 0);
                    break;
                case DOPO_IPC_PKT_QUIT:
                    shim_events_quit_request();
                    break;
                default:
                    break;
            }
            continue;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            ipc_lost();
        }
        return;
    }
}
