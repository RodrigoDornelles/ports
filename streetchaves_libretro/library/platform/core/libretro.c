#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "libretro.h"

#include "streetchaves/game.h"
#include "streetchaves/viewer.h"

/* DXTimer1.Interval: the game's tick, the frame here; 0 ran it as often as
 * the display allowed */
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
static bool viewer;
static int interval = INTERVAL_MS;
static uint16_t held[2];

/* streetchaves-control, both players' */
enum control { MODERN, ARCADE, SINGLE };
static enum control control = MODERN;
static unsigned picked[2];

static bool contains(const blob_t *b, const char *s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i + n <= b->size; i++)
        if (!memcmp(b->data + i, s, n))
            return true;
    return false;
}

void streetchaves_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "[streetchaves] %s", line);
    else
        fprintf(stderr, "[streetchaves] %s", line);
}

void streetchaves_notify(const char *text)
{
    struct retro_message msg = { text, 300 };
    if (environ_cb)
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
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
    static const struct retro_variable vars[] = {
        { "streetchaves-nivel", "Difficulty (the game's Nivel); 4|5|6|7|8|1|2|3" },
        { "streetchaves-control", "Controls; modern|arcade|single" },
        { "streetchaves-viewer", "Asset viewer instead of the game; disabled|enabled" },
        { NULL, NULL },
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
}

static double fps(void)
{
    return interval > 0 ? 1000.0 / interval : 60.0;
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void)
{
    streetchaves_NimMain();
}

void retro_deinit(void) { }

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof *info);
    info->library_name = "Street Chaves";
    info->library_version = "0.1";
    info->valid_extensions = "exe|zip|rar";
    info->need_fullpath = true;
    info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof *info);
    info->geometry.base_width = info->geometry.max_width = DX_WIDTH;
    info->geometry.base_height = info->geometry.max_height = DX_HEIGHT;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps = fps();
    info->timing.sample_rate = DX_RATE;
}

/* player 1 had the keyboard (arrows, Z X C V), player 2 the joystick (its
 * first four buttons): light punch, light kick, strong punch, strong kick;
 * the screens out of the fight read only the first, to confirm.
 * Modern: B on Z and A on X as RetroArch's keyboard has them, X and Y the
 * strong ones; out of the fight A confirms and B goes back as SELECT does
 * (streetchaves_v1_back); A or START puts a coin in and starts where a
 * player may enter (streetchaves_v1_start), SELECT is F1, the settings.
 * Arcade, as in 2004: the same blows, B (Z) confirming as Z did; SELECT
 * is 5, the coin, START 1 or 2, which needs one.
 * Single button: A is any of the four blows, picked when pressed, and
 * confirms out of the fight; B, X and Y do nothing. */
static const unsigned buttons[4] = {
    RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_Y,
};

#define PRESSED(b, id) ((b) >> RETRO_DEVICE_ID_JOYPAD_##id & 1)

static void describe(void)
{
    static const char *blow[4] = { "Soco fraco", "Chute fraco", "Soco forte", "Chute forte" };
    struct retro_input_descriptor desc[2 * 10 + 1], *d = desc;
    for (unsigned p = 0; p < 2; p++) {
#define DESC(id, text) (*d++ = (struct retro_input_descriptor){ p, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_##id, text })
        DESC(UP, "Cima");
        DESC(DOWN, "Baixo");
        DESC(LEFT, "Esquerda");
        DESC(RIGHT, "Direita");
        if (control == SINGLE) {
            DESC(A, "Golpe / Confirma");
        } else if (control == ARCADE) {
            for (unsigned b = 0; b < 4; b++)
                *d++ = (struct retro_input_descriptor){ p, RETRO_DEVICE_JOYPAD, 0, buttons[b], blow[b] };
        } else {
            DESC(B, "Soco fraco / Voltar");
            DESC(A, "Chute fraco / Confirma");
            DESC(X, "Soco forte");
            DESC(Y, "Chute forte");
        }
        DESC(START, control == ARCADE ? "Comecar (com ficha)" : "Comecar");
        DESC(SELECT, control == ARCADE ? "Ficha" : "Configuracoes");
#undef DESC
    }
    *d = (struct retro_input_descriptor){ 0 };
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

/* the difficulty in place of the one the game saved, at the start and
 * whenever the frontend changes it */
static void options(void)
{
    struct retro_variable var = { "streetchaves-nivel", NULL };
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        streetchaves_v1_nivel(atoi(var.value));
    var = (struct retro_variable){ "streetchaves-control", NULL };
    enum control was = control;
    control = MODERN;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        control = !strcmp(var.value, "arcade") ? ARCADE : !strcmp(var.value, "single") ? SINGLE : MODERN;
    if (control != was)
        describe();
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }

bool retro_load_game(const struct retro_game_info *game)
{
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
    if (!game || !game->path || !environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
        return false;
    if (!vfs_open(game->path)) {
        streetchaves_log("no Chaves.exe in %s\n", game->path);
        return false;
    }
    size_t size;
    exe = vfs_load(vfs_exe(), &size);
    if (!exe) {
        streetchaves_log("cannot read %s\n", vfs_exe());
        vfs_close();
        return false;
    }
    blob_t units;
    if (!pe_resource(exe, size, 10, "PACKAGEINFO", &units)
        || !contains(&units, "ChavesGolpes"))
        streetchaves_log("%s does not look like Street Chaves (no ChavesGolpes unit)\n", vfs_exe());
    if (!texts_load(exe, size))
        streetchaves_log("%s: the texts were not found in its code\n", vfs_exe());
    if (!form_create(exe, size))
        streetchaves_log("%s: no form in it\n", vfs_exe());
    const char *save_dir = NULL;
    environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);
    config_open(save_dir);
    streetchaves_v1_create();
    control = MODERN;
    options();
    struct retro_variable var = { "streetchaves-viewer", NULL };
    viewer = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "enabled");
    interval = INTERVAL_MS;
    describe();
    surface_new(&screen, DX_WIDTH, DX_HEIGHT);
    return !viewer || viewer_init(exe, size);
}

void retro_unload_game(void)
{
    if (viewer)
        viewer_deinit();
    dx_stop_all();
    form_destroy();
    config_close();
    texts_free();
    surface_free(&screen);
    free(exe);
    exe = NULL;
    vfs_close();
}

static uint16_t pad(unsigned port)
{
    uint16_t buttons = 0;
    for (unsigned id = 0; id < 16; id++)
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id))
            buttons |= (uint16_t)(1 << id);
    return buttons;
}

/* isUp, isDown, isLeft, isRight, isButton1..4 */
static uint32_t states(uint16_t b, uint16_t press, unsigned port, int tela)
{
    bool menu = tela != 7;
    static const unsigned dpad[4] = {
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
    };
    uint32_t s = 0;
    for (unsigned i = 0; i < 4; i++)
        s |= (uint32_t)(b >> dpad[i] & 1) << i;
    if (menu && control != ARCADE)
        return s | (uint32_t)PRESSED(b, A) << 4;
    if (control == SINGLE) {
        if (PRESSED(press, A))
            picked[port] = (unsigned)rand() % 4;
        return s | (uint32_t)PRESSED(b, A) << (4 + picked[port]);
    }
    for (unsigned i = 0; i < 4; i++)
        s |= (uint32_t)(b >> buttons[i] & 1) << (4 + i);
    return s;
}

static void run_game(void)
{
    uint16_t keys[8];
    int count = 0;
    int tela = streetchaves_v1_tela();
    bool menu = tela != 7;
    for (unsigned p = 0; p < 2; p++) {
        uint16_t b = pad(p), press = b & ~held[p];
        held[p] = b;
        dx_input_set(p == 0 ? DX_KEYBOARD : DX_JOYSTICK, states(b, press, p, tela));
        if (control == ARCADE) {
            if (PRESSED(press, START))
                keys[count++] = p == 0 ? '1' : '2';
            if (PRESSED(press, SELECT))
                keys[count++] = '5';
            continue;
        }
        if (PRESSED(press, START) || PRESSED(press, A))
            count += streetchaves_v1_start((int)p + 1, keys + count);
        if (control == MODERN && menu && PRESSED(press, B) && streetchaves_v1_back())
            keys[count++] = (uint16_t)streetchaves_v1_back();
        if (PRESSED(press, SELECT))
            keys[count++] = 0x70;
    }
    int next = streetchaves_v1_frame(keys, count);
    if (dx_terminated())
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    if (next != interval) {
        interval = next;
        struct retro_system_av_info av;
        retro_get_system_av_info(&av);
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
    }
}

void retro_run(void)
{
    input_poll_cb();
    surface_t *out = &screen;
    if (viewer) {
        viewer_run(pad(0), &screen);
    } else {
        bool changed = false;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &changed) && changed)
            options();
        run_game();
        out = dx_screen();
    }
    video_cb(out->px, DX_WIDTH, DX_HEIGHT, DX_WIDTH * sizeof *out->px);

    static int16_t audio[2 * (DX_RATE / 10)];
    audio_rest += DX_RATE * (unsigned)(interval > 0 ? interval : 17);
    size_t frames = audio_rest / 1000;
    audio_rest %= 1000;
    if (frames > sizeof audio / sizeof *audio / 2)
        frames = sizeof audio / sizeof *audio / 2;
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
