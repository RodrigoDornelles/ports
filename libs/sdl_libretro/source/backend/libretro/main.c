#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "ipc.h"

#define FRAME_WIDTH  320
#define FRAME_HEIGHT 240
#define FRAME_RATE   60.0
#define SAMPLE_RATE  48000.0

static retro_environment_t  environ_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t   input_poll_cb;
static retro_input_state_t  input_state_cb;
static retro_log_printf_t   log_cb;

static char     s_shim_dir[PATH_MAX];
static char     s_content[PATH_MAX];
static uint16_t s_frame[FRAME_WIDTH * FRAME_HEIGHT];
static bool     s_held[DOPO_IPC_PAD_COUNT];
static bool     s_running;
static dopo_ipc_format_t s_format = DOPO_IPC_FORMAT_RGBA8888;
static unsigned s_fb_w = FRAME_WIDTH;
static unsigned s_fb_h = FRAME_HEIGHT;

static const struct {
    const char *name;
    unsigned    id;
} k_buttons[] = {
    { "up",    RETRO_DEVICE_ID_JOYPAD_UP    },
    { "down",  RETRO_DEVICE_ID_JOYPAD_DOWN  },
    { "left",  RETRO_DEVICE_ID_JOYPAD_LEFT  },
    { "right", RETRO_DEVICE_ID_JOYPAD_RIGHT },
    { "a",     RETRO_DEVICE_ID_JOYPAD_A     },
    { "b",     RETRO_DEVICE_ID_JOYPAD_B     },
    { "c",     RETRO_DEVICE_ID_JOYPAD_X     },
    { "d",     RETRO_DEVICE_ID_JOYPAD_Y     },
    { "e",     RETRO_DEVICE_ID_JOYPAD_L     },
    { "f",     RETRO_DEVICE_ID_JOYPAD_R     },
    { "menu",  RETRO_DEVICE_ID_JOYPAD_START },
};

#define BUTTON_COUNT (sizeof(k_buttons) / sizeof(*k_buttons))

static void log_line(enum retro_log_level level, const char *fmt, ...) {
    char    line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (log_cb) log_cb(level, "%s", line);
    else        fprintf(stderr, "[sdl_libretro] %s\n", line);
}

static const char *shim_dir(void) {
    if (s_shim_dir[0]) return s_shim_dir;

    const char *option = option_get("sdl_shim");
    if (option) {
        path_resolve_rel(option, s_content, s_shim_dir, sizeof(s_shim_dir));
        return s_shim_dir;
    }

    Dl_info info;
    char    resolved[PATH_MAX];
    if (dladdr((void *)shim_dir, &info) && info.dli_fname &&
        realpath(info.dli_fname, resolved)) {
        char *slash = strrchr(resolved, '/');
        if (slash) *slash = '\0';
        snprintf(s_shim_dir, sizeof(s_shim_dir), "%s", resolved);
    }
    return s_shim_dir;
}

static void dispatch(const char *name, bool pressed) {
    bind_t bind;
    if (!keymap_bind(name, &bind)) return;

    if (bind.kind == BIND_KEY) process_send_key(bind.scancode, bind.keycode, pressed);
    else                       process_send_pad(bind.pad, pressed);
}

static void poll_input(void) {
    input_poll_cb();

    for (size_t i = 0; i < BUTTON_COUNT; i++) {
        bool down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, k_buttons[i].id) != 0;
        if (down == s_held[i]) continue;
        s_held[i] = down;
        dispatch(k_buttons[i].name, down);
    }
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    bool no_rom = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

    struct retro_log_callback logging;
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) log_cb = logging.log;

    options_init(cb);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)       { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)     { input_state_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)   { (void)cb; }

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_init(cb); }

void retro_init(void) {
    memset(s_frame, 0, sizeof(s_frame));
    memset(s_held, 0, sizeof(s_held));
}

void retro_deinit(void) {
    process_stop(true);
    s_shim_dir[0] = '\0';
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = "sdl_libretro";
    info->library_version  = "0.1";
    info->valid_extensions = "love|sh|bin";
    info->need_fullpath    = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width   = FRAME_WIDTH;
    info->geometry.base_height  = FRAME_HEIGHT;
    info->geometry.max_width    = DOPO_IPC_FB_MAX_WIDTH;
    info->geometry.max_height   = DOPO_IPC_FB_MAX_HEIGHT;
    info->geometry.aspect_ratio = (float)FRAME_WIDTH / (float)FRAME_HEIGHT;
    info->timing.fps            = FRAME_RATE;
    info->timing.sample_rate    = SAMPLE_RATE;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port; (void)device;
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game || !game->path) {
        process_set_error("no content path");
        return false;
    }

    options_refresh();

    const char *want = option_get("sdl_format");
    if (!want || !want[0]) want = "rgba8888";

    if (!strcmp(want, "egl"))           s_format = DOPO_IPC_FORMAT_EGL;
    else if (!strcmp(want, "rgba8888")) s_format = DOPO_IPC_FORMAT_RGBA8888;
    else if (!strcmp(want, "rgb565"))   s_format = DOPO_IPC_FORMAT_RGB565;
    else {
        process_set_error("sdl_format=%s not supported", want);
        return false;
    }

    enum retro_pixel_format pixel = s_format == DOPO_IPC_FORMAT_RGB565
                                  ? RETRO_PIXEL_FORMAT_RGB565
                                  : RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel)) {
        process_set_error("pixel format not supported by frontend");
        return false;
    }

    snprintf(s_content, sizeof(s_content), "%s", game->path);
    s_shim_dir[0] = '\0';

    setenv(DOPO_ENV_FORMAT, want, 1);

    keymap_configure(s_content);

    if (!process_request(s_content, shim_dir())) return false;

    memset(s_held, 0, sizeof(s_held));
    s_running = true;
    log_line(RETRO_LOG_INFO, "loaded %s", s_content);
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    (void)type; (void)info; (void)num;
    return false;
}

void retro_unload_game(void) {
    process_stop(false);
    s_running = false;
}

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        options_refresh();
    }

    if (s_running) {
        poll_input();
        process_tick();

        if (process_is_done()) {
            s_running = false;
            if (process_has_failed()) log_line(RETRO_LOG_ERROR, "%s", process_error());
            else                      log_line(RETRO_LOG_INFO, "child exited, shutting down");
            environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
        }
    }

    unsigned    w = 0, h = 0, bpp = 0;
    const void *frame = process_frame(&w, &h, &bpp);

    if (frame) {
        if (process_frame_changed() || w != s_fb_w || h != s_fb_h) {
            s_fb_w = w;
            s_fb_h = h;
            struct retro_game_geometry geometry = {
                .base_width   = w,
                .base_height  = h,
                .max_width    = DOPO_IPC_FB_MAX_WIDTH,
                .max_height   = DOPO_IPC_FB_MAX_HEIGHT,
                .aspect_ratio = (float)w / (float)h,
            };
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
        }
        video_cb(frame, w, h, (size_t)w * bpp);
        return;
    }

    video_cb(NULL, s_fb_w, s_fb_h, 0);
}

void retro_reset(void) {
    if (!s_content[0]) return;
    process_stop(true);
    process_request(s_content, shim_dir());
    memset(s_held, 0, sizeof(s_held));
}

size_t retro_serialize_size(void) { return 0; }
bool   retro_serialize(void *data, size_t size)         { (void)data; (void)size; return false; }
bool   retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }

void  retro_cheat_reset(void) {}
void  retro_cheat_set(unsigned index, bool enabled, const char *code) {
    (void)index; (void)enabled; (void)code;
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void  *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
