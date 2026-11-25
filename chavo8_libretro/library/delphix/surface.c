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
