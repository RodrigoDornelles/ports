#ifndef SDL_LIBRETRO_CORE_H
#define SDL_LIBRETRO_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libretro.h>

typedef enum {
    BIND_PAD = 0,
    BIND_KEY,
} bind_kind_t;

typedef struct {
    bind_kind_t kind;
    uint8_t     pad;
    uint16_t    scancode;
    uint32_t    keycode;
} bind_t;

void        options_init(retro_environment_t cb);
void        options_refresh(void);
const char *option_get(const char *key);
bool        option_is(const char *key, const char *value);
void        path_resolve_rel(const char *given, const char *base_file, char *out, size_t cap);
bool        path_which(const char *name, char *out, size_t cap);

bool        linkage_needs(const char *path, const char *soname);

void        keymap_configure(const char *exec_path);
bool        keymap_bind(const char *name, bind_t *out);

bool        process_request(const char *path, const char *shim_dir);
void        process_stop(bool force);
void        process_tick(void);
bool        process_send_key(uint16_t scancode, uint32_t keycode, bool pressed);
bool        process_send_pad(uint8_t pad, bool pressed);
bool        process_is_running(void);
bool        process_is_done(void);
bool        process_has_failed(void);
const char *process_error(void);
void        process_set_error(const char *fmt, ...);

const void *process_frame(unsigned *w, unsigned *h, unsigned *bpp);
bool        process_frame_changed(void);

void        audio_init(retro_audio_sample_batch_t cb);
void        audio_configure(unsigned rate, unsigned channels);
void        audio_push(const int16_t *data, size_t frames);
void        audio_stop(void);
void        audio_reset(void);

#endif
