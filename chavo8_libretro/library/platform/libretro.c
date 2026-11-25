#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "libretro.h"

#include "chavo8/game.h"
#include "chavo8/viewer.h"

/* DXTimer1.Interval: the game's tick, the frame here */
#define INTERVAL_MS 28

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_log_printf_t log_cb;

static surface_t screen;
static uint8_t *exe;
static unsigned audio_rest;

static bool contains(const blob_t *b, const char *s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i + n <= b->size; i++)
        if (!memcmp(b->data + i, s, n))
            return true;
    return false;
}

void chavo8_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "[chavo8] %s", line);
    else
        fprintf(stderr, "[chavo8] %s", line);
}

uint32_t clock_day_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec % 86400) * 1000 + (uint32_t)(tv.tv_usec / 1000);
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;
    struct retro_log_callback log;
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void)
{
    chavo8_NimMain();
}

void retro_deinit(void) { }

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof *info);
    info->library_name = "Chavo8";
    info->library_version = "0.1";
    info->valid_extensions = "exe|zip";
    info->need_fullpath = true;
    info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof *info);
    info->geometry.base_width = info->geometry.max_width = DX_WIDTH;
    info->geometry.base_height = info->geometry.max_height = DX_HEIGHT;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps = 1000.0 / INTERVAL_MS;
    info->timing.sample_rate = DX_RATE;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }

bool retro_load_game(const struct retro_game_info *game)
{
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
    if (!game || !game->path || !environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
        return false;
    if (!vfs_open(game->path)) {
        chavo8_log("no Chaves.exe in %s\n", game->path);
        return false;
    }
    size_t size;
    exe = vfs_load(vfs_exe(), &size);
    if (!exe) {
        chavo8_log("cannot read %s\n", vfs_exe());
        vfs_close();
        return false;
    }
    blob_t units;
    if (!pe_resource(exe, size, 10, "PACKAGEINFO", &units)
        || !contains(&units, "ChavesGolpes"))
        chavo8_log("%s does not look like Street Chaves (no ChavesGolpes unit)\n", vfs_exe());
    if (!texts_load(exe, size))
        chavo8_log("%s: the texts were not found in its code\n", vfs_exe());
    if (!form_create(exe, size))
        chavo8_log("%s: no form in it\n", vfs_exe());
    const char *save_dir = NULL;
    environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);
    config_open(save_dir);
    chavo8_v1_create();
    surface_new(&screen, DX_WIDTH, DX_HEIGHT);
    return viewer_init(exe, size);
}

void retro_unload_game(void)
{
    viewer_deinit();
    form_destroy();
    config_close();
    texts_free();
    surface_free(&screen);
    free(exe);
    exe = NULL;
    vfs_close();
}

void retro_run(void)
{
    input_poll_cb();
    uint16_t buttons = 0;
    for (unsigned id = 0; id < 16; id++)
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id))
            buttons |= (uint16_t)(1 << id);
    viewer_run(buttons, &screen);
    video_cb(screen.px, DX_WIDTH, DX_HEIGHT, DX_WIDTH * sizeof *screen.px);

    static int16_t audio[2 * (DX_RATE * INTERVAL_MS / 1000 + 1)];
    audio_rest += DX_RATE * INTERVAL_MS;
    size_t frames = audio_rest / 1000;
    audio_rest %= 1000;
    dx_mix(audio, frames);
    audio_batch_cb(audio, frames);
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)type; (void)info; (void)num;
    return false;
}

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
