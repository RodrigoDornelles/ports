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
void dx_draw_alpha(surface_t *dst, int l, int t, int r, int b, const surface_t *src, int alpha);
void canvas_fill_rect(surface_t *dst, int l, int t, int r, int b, uint32_t color);
void canvas_frame_rect(surface_t *dst, int l, int t, int r, int b, uint32_t color);
void canvas_round_rect(surface_t *dst, int l, int t, int r, int b, int w, int h, uint32_t pen, uint32_t brush);

/* TCanvas.Pixels: CLR_INVALID out of the bitmap, which splits into white */
uint32_t canvas_pixel(const image_t *img, int x, int y);

/* TCanvas.CopyRect, that is StretchBlt: mirrored when the widths' signs differ */
void canvas_copy_rect(image_t *dst, int dl, int dt, int dr, int db,
    const image_t *src, int sl, int st, int sr, int sb);

/* TextOut in Font.Name 'Comic Sans MS' of Font.Size points, the text in
 * the game's code page (1252) */
int canvas_text_width(int size, const char *text);
void canvas_text_out(surface_t *dst, int x, int y, uint32_t color, int size, const char *text);

/* the form's components: its TImages (24-bit bitmaps), TDXImageLists
 * (16-bit surfaces) and TDXWaveLists, with the items its DFM gives them
 * before FormCreate runs */
enum { IMAGE_LUTADOR, IMAGE_RETRATO, IMAGE_LUTADOR_FRAME, IMAGE_RETRATO_INVERT, FORM_IMAGES };
enum {
    LIST_RETRATO, LIST_EFEITOS, LIST_CENARIO, LIST_ICONE, LIST_LUTADOR, LIST_LUTADOR_INVERT,
    LIST_ABERTURA, LIST_FINAL, LIST_ANIMACAO, LIST_EFEITOS_INVERT, IMAGE_LISTS
};
enum { WAVES_SND, WAVES_MUSICA, WAVES_VITORIA, WAVE_LISTS };

/* TDXInput: the keyboard (player 1) and the joystick (player 2), each a set
 * of states as bits: isUp, isDown, isLeft, isRight, isButton1.. from 0 */
enum { DX_KEYBOARD, DX_JOYSTICK, DX_DEVICES };

bool form_create(const uint8_t *exe, size_t size);
surface_t *dx_screen(void);
uint32_t form_image_pixel(int image, int x, int y);
void dx_images_draw(int list, int index, int x, int y);
void dx_images_draw_alpha(int list, int index, int l, int t, int r, int b, int alpha);
void dx_images_stretch(int list, int index, int l, int t, int r, int b);
void dx_waves_play(int list, int index);
void dx_waves_stop(int list, int index);
void dx_terminate(void);
bool dx_terminated(void);
void dx_input_set(int device, uint32_t states);
void dx_input_update(void);
uint32_t dx_input_states(int device);
void form_destroy(void);
image_t *form_image(int image);
bool form_image_load(int image, const char *name);
void form_image_resize(int image, int w, int h);
int dx_images_add(int list);
int dx_images_count(int list);
void dx_images_assign(int list, int index, int image, bool transparent, uint32_t color);
bool dx_images_load(int list, int index, const char *name, bool transparent, uint32_t color);
const surface_t *dx_images_get(int list, int index);
int dx_waves_add(int list);
int dx_waves_count(int list);
bool dx_waves_load(int list, int index, const char *name);
const wave_t *dx_waves_get(int list, int index);

/* TDXSound: the waves mixed into 44100 Hz stereo */
#define DX_RATE 44100
int dx_play(const wave_t *wave, bool loop);
void dx_stop(const wave_t *wave);
void dx_stop_all(void);
void dx_mix(int16_t *out, size_t frames);
