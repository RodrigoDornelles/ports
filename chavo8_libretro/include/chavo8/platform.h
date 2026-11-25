#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* the game's files: the folder of the Chaves.exe given as content, or a .zip
 * holding it; names are relative to the exe's folder, '/' separated and
 * matched without case, as Windows did */
bool vfs_open(const char *content);
void vfs_close(void);
const char *vfs_exe(void);
uint8_t *vfs_load(const char *name, size_t *size);
size_t vfs_list(const char *dir, const char **names, size_t max);

typedef struct {
    const uint8_t *data;
    size_t size;
} blob_t;

bool pe_resource(const uint8_t *exe, size_t size, unsigned type, const char *name, blob_t *out);
const uint8_t *pe_va(const uint8_t *exe, size_t size, uint32_t va, size_t len);
bool pe_code(const uint8_t *exe, size_t size, blob_t *code, uint32_t *va);

/* the lines ChavesTexto puts in the form's memos, read from its code:
 * Portuguese is language 1, anything else Spanish, as the game decides */
enum { MEMO_NOMES, MEMO_TEXTO, MEMO_CONFIG, MEMO_FRASES, MEMO_DIALOGO, MEMO_FINAIS, MEMO_CENARIOS, MEMOS };
bool texts_load(const uint8_t *exe, size_t size);
void texts_free(void);
size_t texts_count(int language, int memo);
const char *texts_line(int language, int memo, size_t index);

/* the image and wave items of the form's TDXImageList and TDXWaveList
 * components, as the DFM stores them */
typedef struct {
    const char *component;
    const char *name;
    blob_t data;
    bool transparent;
    uint32_t transparent_color;
    bool looped;
} dfm_item_t;

typedef void (*dfm_item_fn)(void *ctx, const dfm_item_t *item);
bool dfm_items(const uint8_t *dfm, size_t size, dfm_item_fn fn, void *ctx);

/* 24-bit pictures as GDI read them: top-down, R G B */
typedef struct {
    int w, h;
    uint8_t *rgb;
} image_t;

bool image_bmp(const uint8_t *data, size_t size, image_t *out);
bool image_picture(const uint8_t *data, size_t size, image_t *out);
bool image_new(image_t *img, int w, int h);
void image_free(image_t *img);

typedef struct {
    const uint8_t *pcm;
    uint32_t frames;
    uint32_t rate;
    uint8_t channels, bits;
} wave_t;

bool wave_parse(const uint8_t *data, size_t size, wave_t *out);
