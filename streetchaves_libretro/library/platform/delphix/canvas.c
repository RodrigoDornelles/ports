#include <stdlib.h>

#include "stb_truetype.h"

#include "streetchaves/delphix.h"
#include "streetchaves/font.h"

uint32_t canvas_pixel(const image_t *img, int x, int y)
{
    if (x < 0 || y < 0 || x >= img->w || y >= img->h)
        return 0xFFFFFFFF;
    const uint8_t *p = img->rgb + 3 * ((size_t)y * img->w + x);
    return p[0] | p[1] << 8 | (uint32_t)p[2] << 16;
}

/* nearest pixel, like StretchBlt's COLORONCOLOR; a negative extent covers
 * the pixels from its end up to before its start, which is how the game
 * mirrors fighters (CopyRect from right-1 to left: one column less than
 * the width, stretched over it), yet to check against Windows */
void canvas_copy_rect(image_t *dst, int dl, int dt, int dr, int db,
    const image_t *src, int sl, int st, int sr, int sb)
{
    int dw = dr - dl, dh = db - dt, sw = sr - sl, sh = sb - st;
    if (!dw || !dh || !sw || !sh)
        return;
    bool mx = (dw < 0) != (sw < 0), my = (dh < 0) != (sh < 0);
    int ax = abs(dw), ay = abs(dh), bx = abs(sw), by = abs(sh);
    int dx0 = dw < 0 ? dr : dl, dy0 = dh < 0 ? db : dt;
    int sx0 = sw < 0 ? sr : sl, sy0 = sh < 0 ? sb : st;
    for (int j = 0; j < ay; j++) {
        int v = (int)((int64_t)j * by / ay);
        int sy = sy0 + (my ? by - 1 - v : v), dy = dy0 + j;
        if (sy < 0 || sy >= src->h || dy < 0 || dy >= dst->h)
            continue;
        for (int i = 0; i < ax; i++) {
            int u = (int)((int64_t)i * bx / ax);
            int sx = sx0 + (mx ? bx - 1 - u : u), dx = dx0 + i;
            if (sx < 0 || sx >= src->w || dx < 0 || dx >= dst->w)
                continue;
            const uint8_t *s = src->rgb + 3 * ((size_t)sy * src->w + sx);
            uint8_t *d = dst->rgb + 3 * ((size_t)dy * dst->w + dx);
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
}

/* GDI asks for -MulDiv(Size, 96, 72) pixels of em; glyphs are kept by
 * size and drawn without smoothing, as on the 16-bit display. A pixel is
 * set from 96/255 of coverage: half would lose the thin strokes (a hyphen at
 * 8 points) that the hinted, dropout-controlled GDI glyphs keep */
#define MAX_SIZE 128

typedef struct {
    uint8_t *bitmap;
    int w, h, xoff, yoff, advance;
    bool ready;
} glyph_t;

static stbtt_fontinfo font;
static bool font_ready;
static glyph_t *glyphs[MAX_SIZE];
static int ascent[MAX_SIZE];
static float scale[MAX_SIZE];

static const uint16_t cp1252[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

static int clamp(int size)
{
    return size < 1 ? 1 : size >= MAX_SIZE ? MAX_SIZE - 1 : size;
}

static const glyph_t *glyph(int size, uint8_t c)
{
    if (!font_ready) {
        stbtt_InitFont(&font, ComicRelief_Regular, stbtt_GetFontOffsetForIndex(ComicRelief_Regular, 0));
        font_ready = true;
    }
    size = clamp(size);
    if (!glyphs[size]) {
        glyphs[size] = calloc(256, sizeof(glyph_t));
        scale[size] = stbtt_ScaleForMappingEmToPixels(&font, (float)((size * 96 + 36) / 72));
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&font, &asc, &desc, &gap);
        ascent[size] = (int)(asc * scale[size] + 0.5f);
    }
    glyph_t *g = &glyphs[size][c];
    if (!g->ready) {
        int cp = c >= 0x80 && c < 0xA0 ? cp1252[c - 0x80] : c;
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, cp, &advance, &lsb);
        g->advance = (int)(advance * scale[size] + 0.5f);
        g->bitmap = stbtt_GetCodepointBitmap(&font, 0, scale[size], cp, &g->w, &g->h, &g->xoff, &g->yoff);
        g->ready = true;
    }
    return g;
}

int canvas_text_width(int size, const char *text)
{
    int w = 0;
    for (const uint8_t *c = (const uint8_t *)text; *c; c++)
        w += glyph(size, *c)->advance;
    return w;
}

void canvas_text_out(surface_t *dst, int x, int y, uint32_t color, int size, const char *text)
{
    uint16_t c565 = dx_565(color);
    for (const uint8_t *c = (const uint8_t *)text; *c; c++) {
        const glyph_t *g = glyph(size, *c);
        int base = y + ascent[clamp(size)];
        for (int j = 0; j < g->h; j++) {
            int py = base + g->yoff + j;
            if (py < 0 || py >= dst->h)
                continue;
            for (int i = 0; i < g->w; i++) {
                int px = x + g->xoff + i;
                if (px >= 0 && px < dst->w && g->bitmap[j * g->w + i] >= 96)
                    dst->px[(size_t)py * dst->w + px] = c565;
            }
        }
        x += g->advance;
    }
}
