#define _GNU_SOURCE
#include "sdl1.h"

static void mask_split(Uint32 mask, Uint8 *shift, Uint8 *loss) {
    if (mask == 0) {
        *shift = 0;
        *loss  = 8;
        return;
    }
    *shift = (Uint8)__builtin_ctz(mask);
    *loss  = (Uint8)(8 - __builtin_popcount(mask));
}

SDL_Surface *shim_surface_new(void *pixels, int width, int height, int depth, int pitch,
                              Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask) {
    if (width < 0 || height < 0 || depth <= 0 || depth > 32) {
        shim_set_error("invalid surface geometry %dx%d@%d", width, height, depth);
        return NULL;
    }

    SDL_Surface     *surface = calloc(1, sizeof(*surface));
    SDL_PixelFormat *format  = calloc(1, sizeof(*format));
    if (!surface || !format) {
        free(surface);
        free(format);
        shim_set_error("out of memory");
        return NULL;
    }

    format->BitsPerPixel  = (Uint8)depth;
    format->BytesPerPixel = (Uint8)((depth + 7) / 8);
    format->Rmask         = rmask;
    format->Gmask         = gmask;
    format->Bmask         = bmask;
    format->Amask         = amask;
    format->alpha         = SDL_ALPHA_OPAQUE;
    mask_split(rmask, &format->Rshift, &format->Rloss);
    mask_split(gmask, &format->Gshift, &format->Gloss);
    mask_split(bmask, &format->Bshift, &format->Bloss);
    mask_split(amask, &format->Ashift, &format->Aloss);

    if (depth <= 8) {
        SDL_Palette *palette = calloc(1, sizeof(*palette));
        int          ncolors = 1 << depth;
        if (palette) {
            palette->ncolors = ncolors;
            palette->colors  = calloc((size_t)ncolors, sizeof(SDL_Color));
        }
        if (!palette || !palette->colors) {
            free(palette);
            free(format);
            free(surface);
            shim_set_error("out of memory");
            return NULL;
        }
        for (int i = 0; i < ncolors; i++) {
            palette->colors[i].r = (Uint8)i;
            palette->colors[i].g = (Uint8)i;
            palette->colors[i].b = (Uint8)i;
        }
        format->palette = palette;
    }

    int stride = pitch > 0 ? pitch : width * format->BytesPerPixel;
    stride = (stride + 3) & ~3;

    surface->format    = format;
    surface->w         = width;
    surface->h         = height;
    surface->pitch     = (Uint16)stride;
    surface->clip_rect = (SDL_Rect){ 0, 0, (Uint16)width, (Uint16)height };
    surface->refcount  = 1;

    if (pixels) {
        surface->flags  = SDL_PREALLOC;
        surface->pixels = pixels;
        surface->pitch  = (Uint16)(pitch > 0 ? pitch : width * format->BytesPerPixel);
    } else {
        surface->pixels = calloc(1, (size_t)surface->pitch * (size_t)(height > 0 ? height : 1));
        if (!surface->pixels) {
            free(format->palette ? format->palette->colors : NULL);
            free(format->palette);
            free(format);
            free(surface);
            shim_set_error("out of memory");
            return NULL;
        }
    }
    return surface;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    SDL_Surface *surface = shim_surface_new(NULL, width, height, depth, 0,
                                            Rmask, Gmask, Bmask, Amask);
    if (surface && (flags & SDL_SRCALPHA)) surface->flags |= SDL_SRCALPHA;
    return surface;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
                                      Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    return shim_surface_new(pixels, width, height, depth, pitch, Rmask, Gmask, Bmask, Amask);
}

void SDL_FreeSurface(SDL_Surface *surface) {
    if (!surface) return;
    if (--surface->refcount > 0) return;
    if (!(surface->flags & SDL_PREALLOC)) free(surface->pixels);
    if (surface->format) {
        if (surface->format->palette) {
            free(surface->format->palette->colors);
            free(surface->format->palette);
        }
        free(surface->format);
    }
    free(surface);
}

int SDL_LockSurface(SDL_Surface *surface) {
    if (!surface) {
        shim_set_error("passed a NULL surface");
        return -1;
    }
    surface->locked++;
    return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface) {
    if (surface && surface->locked > 0) surface->locked--;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect) {
    if (!surface) return SDL_FALSE;
    if (rect) surface->clip_rect = *rect;
    else      surface->clip_rect = (SDL_Rect){ 0, 0, (Uint16)surface->w, (Uint16)surface->h };
    return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect) {
    if (surface && rect) *rect = surface->clip_rect;
}

int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key) {
    if (!surface) return -1;
    if (flag & SDL_SRCCOLORKEY) {
        surface->flags |= SDL_SRCCOLORKEY;
        surface->format->colorkey = key;
    } else {
        surface->flags &= ~(Uint32)SDL_SRCCOLORKEY;
    }
    return 0;
}

int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha) {
    if (!surface) return -1;
    if (flag & SDL_SRCALPHA) {
        surface->flags |= SDL_SRCALPHA;
        surface->format->alpha = alpha;
    } else {
        surface->flags &= ~(Uint32)SDL_SRCALPHA;
        surface->format->alpha = SDL_ALPHA_OPAQUE;
    }
    return 0;
}

int SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors) {
    if (!surface || !surface->format->palette || !colors) return 0;
    SDL_Palette *palette = surface->format->palette;
    if (firstcolor < 0 || firstcolor + ncolors > palette->ncolors) return 0;
    memcpy(palette->colors + firstcolor, colors, (size_t)ncolors * sizeof(SDL_Color));
    return 1;
}

int SDL_SetPalette(SDL_Surface *surface, int flags, SDL_Color *colors,
                   int firstcolor, int ncolors) {
    (void)flags;
    return SDL_SetColors(surface, colors, firstcolor, ncolors);
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat *const format, const Uint8 r, const Uint8 g,
                   const Uint8 b, const Uint8 a) {
    if (!format) return 0;
    if (format->palette) {
        int best = 0;
        int best_delta = 1 << 30;
        for (int i = 0; i < format->palette->ncolors; i++) {
            const SDL_Color *c = &format->palette->colors[i];
            int dr = (int)c->r - r, dg = (int)c->g - g, db = (int)c->b - b;
            int delta = dr * dr + dg * dg + db * db;
            if (delta < best_delta) {
                best_delta = delta;
                best = i;
                if (!delta) break;
            }
        }
        return (Uint32)best;
    }
    return ((Uint32)(r >> format->Rloss) << format->Rshift)
         | ((Uint32)(g >> format->Gloss) << format->Gshift)
         | ((Uint32)(b >> format->Bloss) << format->Bshift)
         | (format->Amask ? (((Uint32)(a >> format->Aloss) << format->Ashift) & format->Amask) : 0);
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *const format, const Uint8 r, const Uint8 g, const Uint8 b) {
    return SDL_MapRGBA(format, r, g, b, SDL_ALPHA_OPAQUE);
}

void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *const fmt,
                 Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    if (!fmt) return;
    if (fmt->palette) {
        const SDL_Color *c = &fmt->palette->colors[pixel % (Uint32)fmt->palette->ncolors];
        if (r) *r = c->r;
        if (g) *g = c->g;
        if (b) *b = c->b;
        if (a) *a = SDL_ALPHA_OPAQUE;
        return;
    }
    if (r) *r = (Uint8)(((pixel & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss);
    if (g) *g = (Uint8)(((pixel & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss);
    if (b) *b = (Uint8)(((pixel & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss);
    if (a) *a = fmt->Amask ? (Uint8)(((pixel & fmt->Amask) >> fmt->Ashift) << fmt->Aloss)
                           : SDL_ALPHA_OPAQUE;
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *const fmt, Uint8 *r, Uint8 *g, Uint8 *b) {
    SDL_GetRGBA(pixel, fmt, r, g, b, NULL);
}

static Uint32 pixel_get(const Uint8 *at, int bpp) {
    switch (bpp) {
        case 1:  return at[0];
        case 2:  return *(const Uint16 *)(const void *)at;
        case 3:  return (Uint32)at[0] | ((Uint32)at[1] << 8) | ((Uint32)at[2] << 16);
        default: return *(const Uint32 *)(const void *)at;
    }
}

static void pixel_put(Uint8 *at, int bpp, Uint32 value) {
    switch (bpp) {
        case 1:  at[0] = (Uint8)value; break;
        case 2:  *(Uint16 *)(void *)at = (Uint16)value; break;
        case 3:
            at[0] = (Uint8)(value & 0xFF);
            at[1] = (Uint8)((value >> 8) & 0xFF);
            at[2] = (Uint8)((value >> 16) & 0xFF);
            break;
        default: *(Uint32 *)(void *)at = value; break;
    }
}

static bool format_same(const SDL_PixelFormat *a, const SDL_PixelFormat *b) {
    return a->BitsPerPixel == b->BitsPerPixel && a->Rmask == b->Rmask
        && a->Gmask == b->Gmask && a->Bmask == b->Bmask && a->Amask == b->Amask
        && (a->palette == NULL) == (b->palette == NULL);
}

int SDL_LowerBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);

int SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
    if (!src || !dst || !src->pixels || !dst->pixels) {
        shim_set_error("passed a NULL surface");
        return -1;
    }

    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int w  = srcrect ? srcrect->w : src->w;
    int h  = srcrect ? srcrect->h : src->h;
    int dx = dstrect ? dstrect->x : 0;
    int dy = dstrect ? dstrect->y : 0;

    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;

    const SDL_Rect *clip = &dst->clip_rect;
    if (dx < clip->x) { int d = clip->x - dx; w -= d; sx += d; dx = clip->x; }
    if (dy < clip->y) { int d = clip->y - dy; h -= d; sy += d; dy = clip->y; }
    if (dx + w > clip->x + clip->w) w = clip->x + clip->w - dx;
    if (dy + h > clip->y + clip->h) h = clip->y + clip->h - dy;

    if (dstrect) {
        dstrect->x = (Sint16)dx;
        dstrect->y = (Sint16)dy;
        dstrect->w = (Uint16)(w > 0 ? w : 0);
        dstrect->h = (Uint16)(h > 0 ? h : 0);
    }
    if (w <= 0 || h <= 0) return 0;

    SDL_Rect clipped_src = { (Sint16)sx, (Sint16)sy, (Uint16)w, (Uint16)h };
    SDL_Rect clipped_dst = { (Sint16)dx, (Sint16)dy, (Uint16)w, (Uint16)h };
    return SDL_LowerBlit(src, &clipped_src, dst, &clipped_dst);
}

int SDL_LowerBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
    if (!src || !dst || !srcrect || !dstrect) return -1;

    const int sbpp = src->format->BytesPerPixel;
    const int dbpp = dst->format->BytesPerPixel;
    const int w    = srcrect->w;
    const int h    = srcrect->h;

    const bool colorkey = (src->flags & SDL_SRCCOLORKEY) != 0;
    const bool alpha    = (src->flags & SDL_SRCALPHA) != 0;
    const bool per_pixel_alpha = alpha && src->format->Amask != 0;
    const Uint8 surface_alpha  = src->format->alpha;

    if (!colorkey && !alpha && format_same(src->format, dst->format)) {
        for (int y = 0; y < h; y++) {
            const Uint8 *in  = (const Uint8 *)src->pixels + (size_t)(srcrect->y + y) * src->pitch
                             + (size_t)srcrect->x * (size_t)sbpp;
            Uint8       *out = (Uint8 *)dst->pixels + (size_t)(dstrect->y + y) * dst->pitch
                             + (size_t)dstrect->x * (size_t)dbpp;
            memcpy(out, in, (size_t)w * (size_t)sbpp);
        }
        return 0;
    }

    for (int y = 0; y < h; y++) {
        const Uint8 *in  = (const Uint8 *)src->pixels + (size_t)(srcrect->y + y) * src->pitch
                         + (size_t)srcrect->x * (size_t)sbpp;
        Uint8       *out = (Uint8 *)dst->pixels + (size_t)(dstrect->y + y) * dst->pitch
                         + (size_t)dstrect->x * (size_t)dbpp;

        for (int x = 0; x < w; x++, in += sbpp, out += dbpp) {
            Uint32 pixel = pixel_get(in, sbpp);
            if (colorkey && pixel == src->format->colorkey) continue;

            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, src->format, &r, &g, &b, &a);
            if (!per_pixel_alpha) a = alpha ? surface_alpha : SDL_ALPHA_OPAQUE;

            if (alpha && a < SDL_ALPHA_OPAQUE) {
                if (!a) continue;
                Uint8 dr, dg, db, da;
                SDL_GetRGBA(pixel_get(out, dbpp), dst->format, &dr, &dg, &db, &da);
                r = (Uint8)((r * a + dr * (255 - a)) / 255);
                g = (Uint8)((g * a + dg * (255 - a)) / 255);
                b = (Uint8)((b * a + db * (255 - a)) / 255);
                a = da > a ? da : a;
            }
            pixel_put(out, dbpp, SDL_MapRGBA(dst->format, r, g, b, a));
        }
    }
    return 0;
}

int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color) {
    if (!dst || !dst->pixels) {
        shim_set_error("passed a NULL surface");
        return -1;
    }

    int x = dstrect ? dstrect->x : 0;
    int y = dstrect ? dstrect->y : 0;
    int w = dstrect ? dstrect->w : dst->w;
    int h = dstrect ? dstrect->h : dst->h;

    const SDL_Rect *clip = &dst->clip_rect;
    if (x < clip->x) { w -= clip->x - x; x = clip->x; }
    if (y < clip->y) { h -= clip->y - y; y = clip->y; }
    if (x + w > clip->x + clip->w) w = clip->x + clip->w - x;
    if (y + h > clip->y + clip->h) h = clip->y + clip->h - y;
    if (w <= 0 || h <= 0) return 0;

    const int bpp = dst->format->BytesPerPixel;
    for (int row = 0; row < h; row++) {
        Uint8 *out = (Uint8 *)dst->pixels + (size_t)(y + row) * dst->pitch + (size_t)x * (size_t)bpp;
        if (bpp == 1) {
            memset(out, (int)(color & 0xFF), (size_t)w);
            continue;
        }
        for (int col = 0; col < w; col++, out += bpp) pixel_put(out, bpp, color);
    }
    return 0;
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, Uint32 flags) {
    if (!src || !fmt) {
        shim_set_error("passed a NULL surface or format");
        return NULL;
    }

    SDL_Surface *out = shim_surface_new(NULL, src->w, src->h, fmt->BitsPerPixel, 0,
                                        fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    if (!out) return NULL;
    if (fmt->palette && out->format->palette) {
        int n = fmt->palette->ncolors < out->format->palette->ncolors
              ? fmt->palette->ncolors : out->format->palette->ncolors;
        memcpy(out->format->palette->colors, fmt->palette->colors, (size_t)n * sizeof(SDL_Color));
    }

    Uint32 saved = src->flags;
    src->flags &= ~(Uint32)SDL_SRCALPHA;
    SDL_UpperBlit(src, NULL, out, NULL);
    src->flags = saved;

    if (flags & SDL_SRCALPHA) {
        out->flags |= SDL_SRCALPHA;
        out->format->alpha = src->format->alpha;
    }
    if (saved & SDL_SRCCOLORKEY) {
        Uint8 r, g, b;
        SDL_GetRGB(src->format->colorkey, src->format, &r, &g, &b);
        SDL_SetColorKey(out, SDL_SRCCOLORKEY, SDL_MapRGB(out->format, r, g, b));
    }
    return out;
}

SDL_Surface *SDL_DisplayFormat(SDL_Surface *surface) {
    SDL_Surface *screen = shim_screen();
    if (!surface) return NULL;
    if (!screen) {
        shim_set_error("no video mode has been set");
        return NULL;
    }
    return SDL_ConvertSurface(surface, screen->format, surface->flags & SDL_SRCALPHA);
}

SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface) {
    if (!surface) return NULL;
    SDL_PixelFormat fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.BitsPerPixel  = 32;
    fmt.BytesPerPixel = 4;
    fmt.Rmask = 0x00FF0000;
    fmt.Gmask = 0x0000FF00;
    fmt.Bmask = 0x000000FF;
    fmt.Amask = 0xFF000000;
    fmt.alpha = SDL_ALPHA_OPAQUE;
    return SDL_ConvertSurface(surface, &fmt, SDL_SRCALPHA);
}

int SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
    if (!src || !dst || !src->pixels || !dst->pixels) return -1;
    if (src->format->BytesPerPixel != dst->format->BytesPerPixel) {
        shim_set_error("only the same pixel format can be stretched");
        return -1;
    }

    int sx = srcrect ? srcrect->x : 0, sy = srcrect ? srcrect->y : 0;
    int sw = srcrect ? srcrect->w : src->w, sh = srcrect ? srcrect->h : src->h;
    int dx = dstrect ? dstrect->x : 0, dy = dstrect ? dstrect->y : 0;
    int dw = dstrect ? dstrect->w : dst->w, dh = dstrect ? dstrect->h : dst->h;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return 0;

    const int bpp = src->format->BytesPerPixel;
    for (int y = 0; y < dh; y++) {
        const Uint8 *in  = (const Uint8 *)src->pixels + (size_t)(sy + y * sh / dh) * src->pitch;
        Uint8       *out = (Uint8 *)dst->pixels + (size_t)(dy + y) * dst->pitch + (size_t)dx * (size_t)bpp;
        for (int x = 0; x < dw; x++, out += bpp) {
            memcpy(out, in + (size_t)(sx + x * sw / dw) * (size_t)bpp, (size_t)bpp);
        }
    }
    return 0;
}

void shim_surface_pixels(const SDL_Surface *surface, capture_pixels_t *out, uint32_t *palette_cache) {
    memset(out, 0, sizeof(*out));
    if (!surface) return;

    const SDL_PixelFormat *fmt = surface->format;
    out->w     = surface->w;
    out->h     = surface->h;
    out->bpp   = fmt->BytesPerPixel;
    out->rmask = fmt->Rmask;
    out->gmask = fmt->Gmask;
    out->bmask = fmt->Bmask;

    if (fmt->palette && palette_cache) {
        for (int i = 0; i < 256; i++) {
            const SDL_Color *c = i < fmt->palette->ncolors ? &fmt->palette->colors[i] : NULL;
            palette_cache[i] = c ? ((uint32_t)c->r << 16 | (uint32_t)c->g << 8 | c->b) : 0;
        }
        out->palette = palette_cache;
    }
}
