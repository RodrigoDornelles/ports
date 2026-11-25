#pragma once

#include "chavo8/platform.h"

#define DX_WIDTH 400
#define DX_HEIGHT 300

/* colors are TColor, as RGB() builds them: 0x00BBGGRR */
static inline uint16_t dx_565(uint32_t color)
{
    return (uint16_t)(((color & 0xF8) << 8) | ((color >> 5) & 0x7E0) | ((color >> 19) & 0x1F));
}

/* a DirectDraw surface of the 16-bit display mode: pictures reach it as
 * GDI turns 24 bits into 5:6:5, and its color key is the transparent color
 * turned alike, so near-white pixels went transparent too */
typedef struct {
    int w, h;
    uint16_t *px;
    bool transparent;
    uint16_t key;
} surface_t;

bool surface_new(surface_t *s, int w, int h);
bool surface_from_image(surface_t *s, const image_t *img, bool transparent, uint32_t color);
void surface_free(surface_t *s);
void dx_fill(surface_t *dst, uint32_t color);
void dx_draw(surface_t *dst, int x, int y, const surface_t *src);

/* TCanvas.Pixels: CLR_INVALID out of the bitmap, which splits into white */
uint32_t canvas_pixel(const image_t *img, int x, int y);

/* TCanvas.CopyRect, that is StretchBlt: mirrored when the widths' signs differ */
void canvas_copy_rect(image_t *dst, int dl, int dt, int dr, int db,
    const image_t *src, int sl, int st, int sr, int sb);

/* TextOut in Font.Name 'Comic Sans MS' of Font.Size points, the text in
 * the game's code page (1252) */
int canvas_text_width(int size, const char *text);
void canvas_text_out(surface_t *dst, int x, int y, uint32_t color, int size, const char *text);

/* TDXSound: the waves mixed into 44100 Hz stereo */
#define DX_RATE 44100
int dx_play(const wave_t *wave, bool loop);
void dx_stop_all(void);
void dx_mix(int16_t *out, size_t frames);
