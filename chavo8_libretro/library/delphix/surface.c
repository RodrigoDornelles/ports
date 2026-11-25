#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "chavo8/delphix.h"

bool surface_new(surface_t *s, int w, int h)
{
    memset(s, 0, sizeof *s);
    s->w = w;
    s->h = h;
    s->px = calloc((size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1), sizeof *s->px);
    return s->px != NULL;
}

bool surface_from_image(surface_t *s, const image_t *img, bool transparent, uint32_t color)
{
    if (!surface_new(s, img->w, img->h))
        return false;
    const uint8_t *p = img->rgb;
    for (size_t i = 0, n = (size_t)img->w * img->h; i < n; i++, p += 3)
        s->px[i] = dx_565(p[0] | p[1] << 8 | (uint32_t)p[2] << 16);
    s->transparent = transparent;
    s->key = dx_565(color);
    return true;
}

void surface_free(surface_t *s)
{
    free(s->px);
    memset(s, 0, sizeof *s);
}

void dx_fill(surface_t *dst, uint32_t color)
{
    uint16_t c = dx_565(color);
    for (size_t i = 0, n = (size_t)dst->w * dst->h; i < n; i++)
        dst->px[i] = c;
}

void dx_draw(surface_t *dst, int x, int y, const surface_t *src)
{
    int x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0;
    int x1 = src->w, y1 = src->h;
    if (x + x1 > dst->w)
        x1 = dst->w - x;
    if (y + y1 > dst->h)
        y1 = dst->h - y;
    for (int sy = y0; sy < y1; sy++) {
        const uint16_t *s = src->px + (size_t)sy * src->w;
        uint16_t *d = dst->px + (size_t)(y + sy) * dst->w + x;
        if (!src->transparent) {
            if (x1 > x0)
                memcpy(d + x0, s + x0, (size_t)(x1 - x0) * sizeof *d);
            continue;
        }
        for (int sx = x0; sx < x1; sx++)
            if (s[sx] != src->key)
                d[sx] = s[sx];
    }
}

/* TDirectDrawSurface.DrawAlpha onto the rect, stretched: from 255 up DXRender
 * copies the source (DXR_BLEND_ONE1), the color key kept; below it mixes as
 * c1*a + c2*(1-a) on each 5:6:5 channel, yet to check against DXRender */
void dx_draw_alpha(surface_t *dst, int l, int t, int r, int b, const surface_t *src, int alpha)
{
    int w = r - l, h = b - t;
    if (alpha <= 0 || w <= 0 || h <= 0 || !src->w || !src->h)
        return;
    uint32_t xstep = (uint32_t)(((uint64_t)src->w << 16) / (uint32_t)w);
    uint32_t ystep = (uint32_t)(((uint64_t)src->h << 16) / (uint32_t)h);
    for (int j = 0; j < h; j++) {
        int y = t + j;
        if (y < 0 || y >= dst->h)
            continue;
        const uint16_t *s = src->px + (size_t)((j * (uint64_t)ystep) >> 16) * src->w;
        uint16_t *d = dst->px + (size_t)y * dst->w;
        for (int i = 0; i < w; i++) {
            int x = l + i;
            if (x < 0 || x >= dst->w)
                continue;
            uint16_t c = s[(i * (uint64_t)xstep) >> 16];
            if (src->transparent && c == src->key)
                continue;
            if (alpha < 255) {
                uint16_t o = d[x];
                int rr = (((c >> 11) * alpha) + ((o >> 11) * (255 - alpha))) / 255;
                int gg = ((((c >> 5) & 63) * alpha) + (((o >> 5) & 63) * (255 - alpha))) / 255;
                int bb = (((c & 31) * alpha) + ((o & 31) * (255 - alpha))) / 255;
                c = (uint16_t)(rr << 11 | gg << 5 | bb);
            }
            d[x] = c;
        }
    }
}

/* GDI FillRect: right and bottom left out */
void canvas_fill_rect(surface_t *dst, int l, int t, int r, int b, uint32_t color)
{
    uint16_t c = dx_565(color);
    for (int y = t < 0 ? 0 : t; y < b && y < dst->h; y++)
        for (int x = l < 0 ? 0 : l; x < r && x < dst->w; x++)
            dst->px[(size_t)y * dst->w + x] = c;
}

/* GDI FrameRect: a one pixel border inside the rect, nothing when empty */
void canvas_frame_rect(surface_t *dst, int l, int t, int r, int b, uint32_t color)
{
    if (r <= l || b <= t)
        return;
    canvas_fill_rect(dst, l, t, r, t + 1, color);
    canvas_fill_rect(dst, l, b - 1, r, b, color);
    canvas_fill_rect(dst, l, t, l + 1, b, color);
    canvas_fill_rect(dst, r - 1, t, r, b, color);
}

/* GDI RoundRect with a one pixel pen: corners are quarter ellipses of
 * w x h; the rasterization only approaches Windows' */
void canvas_round_rect(surface_t *dst, int l, int t, int r, int b, int w, int h, uint32_t pen, uint32_t brush)
{
    if (r - l < 2 || b - t < 2)
        return;
    double rx = w / 2.0, ry = h / 2.0;
    uint16_t cp = dx_565(pen), cb = dx_565(brush);
    for (int y = t; y < b && y < dst->h; y++) {
        double dy = y < t + ry ? t + ry - y - 0.5 : y > b - 1 - ry ? y - (b - 1 - ry) + 0.5 : 0;
        double k = ry > 0 ? 1 - (dy / ry) * (dy / ry) : 1;
        int inset = (int)(rx - rx * sqrt(k > 0 ? k : 0) + 0.5);
        int x0 = l + inset, x1 = r - 1 - inset;
        for (int x = x0; y >= 0 && x <= x1; x++)
            if (x >= 0 && x < dst->w)
                dst->px[(size_t)y * dst->w + x] = y == t || y == b - 1 || x == x0 || x == x1 ? cp : cb;
    }
}
