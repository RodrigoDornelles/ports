#define _GNU_SOURCE
#include "sdl1.h"

#define BMP_RGB       0
#define BMP_BITFIELDS 3

static bool read_exact(SDL_RWops *src, void *out, int bytes) {
    return SDL_RWread(src, out, 1, bytes) == bytes;
}

SDL_Surface *SDL_LoadBMP_RW(SDL_RWops *src, int freesrc) {
    SDL_Surface *surface = NULL;
    int          start   = 0;

    if (!src) {
        shim_set_error("passed a NULL data source");
        return NULL;
    }
    start = SDL_RWtell(src);

    char magic[2];
    if (!read_exact(src, magic, 2) || magic[0] != 'B' || magic[1] != 'M') {
        shim_set_error("not a bitmap file");
        goto done;
    }

    SDL_ReadLE32(src);                          /* file size  */
    SDL_ReadLE32(src);                          /* reserved   */
    Uint32 data_offset = SDL_ReadLE32(src);
    Uint32 header_size = SDL_ReadLE32(src);

    Sint32 width, height;
    Uint16 bitcount;
    Uint32 compression = BMP_RGB;
    Uint32 palette_used = 0;

    if (header_size == 12) {
        width    = (Sint16)SDL_ReadLE16(src);
        height   = (Sint16)SDL_ReadLE16(src);
        SDL_ReadLE16(src);                      /* planes */
        bitcount = SDL_ReadLE16(src);
    } else if (header_size >= 40) {
        width    = (Sint32)SDL_ReadLE32(src);
        height   = (Sint32)SDL_ReadLE32(src);
        SDL_ReadLE16(src);                      /* planes */
        bitcount = SDL_ReadLE16(src);
        compression = SDL_ReadLE32(src);
        SDL_ReadLE32(src);                      /* image size  */
        SDL_ReadLE32(src);                      /* x pixels/m  */
        SDL_ReadLE32(src);                      /* y pixels/m  */
        palette_used = SDL_ReadLE32(src);
        SDL_ReadLE32(src);                      /* colors used */
    } else {
        shim_set_error("unsupported bitmap header (%u bytes)", header_size);
        goto done;
    }

    if (compression != BMP_RGB && compression != BMP_BITFIELDS) {
        shim_set_error("compressed bitmaps are not supported");
        goto done;
    }
    if (bitcount != 8 && bitcount != 16 && bitcount != 24 && bitcount != 32) {
        shim_set_error("%u bit bitmaps are not supported", bitcount);
        goto done;
    }

    const bool top_down = height < 0;
    if (top_down) height = -height;
    if (width <= 0 || height <= 0) {
        shim_set_error("bad bitmap geometry %dx%d", width, height);
        goto done;
    }

    Uint32 rmask = 0, gmask = 0, bmask = 0, amask = 0;
    if (compression == BMP_BITFIELDS) {
        rmask = SDL_ReadLE32(src);
        gmask = SDL_ReadLE32(src);
        bmask = SDL_ReadLE32(src);
    } else if (bitcount == 16) {
        rmask = 0x7C00; gmask = 0x03E0; bmask = 0x001F;
    } else if (bitcount >= 24) {
        rmask = 0x00FF0000; gmask = 0x0000FF00; bmask = 0x000000FF;
    }

    surface = shim_surface_new(NULL, width, height, bitcount, 0, rmask, gmask, bmask, amask);
    if (!surface) goto done;

    if (bitcount == 8) {
        if (!palette_used) palette_used = 256;
        SDL_RWseek(src, start + 14 + (int)header_size, RW_SEEK_SET);
        const int entry = header_size == 12 ? 3 : 4;
        for (Uint32 i = 0; i < palette_used && i < 256; i++) {
            Uint8 bgr[4] = {0};
            if (!read_exact(src, bgr, entry)) break;
            surface->format->palette->colors[i].b = bgr[0];
            surface->format->palette->colors[i].g = bgr[1];
            surface->format->palette->colors[i].r = bgr[2];
        }
    }

    SDL_RWseek(src, start + (int)data_offset, RW_SEEK_SET);

    const int bpp    = surface->format->BytesPerPixel;
    const int stride = ((width * bpp) + 3) & ~3;
    Uint8    *row    = malloc((size_t)stride);
    if (!row) {
        shim_set_error("out of memory");
        SDL_FreeSurface(surface);
        surface = NULL;
        goto done;
    }

    for (int y = 0; y < height; y++) {
        if (!read_exact(src, row, stride)) {
            memset(row, 0, (size_t)stride);
        }
        Uint8 *out = (Uint8 *)surface->pixels
                   + (size_t)(top_down ? y : height - 1 - y) * surface->pitch;
        memcpy(out, row, (size_t)width * (size_t)bpp);
    }
    free(row);

done:
    if (freesrc && src) SDL_RWclose(src);
    return surface;
}

int SDL_SaveBMP_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst) {
    int rc = -1;

    if (!surface || !dst) {
        shim_set_error("passed a NULL surface or data source");
        goto done;
    }

    const int width  = surface->w;
    const int height = surface->h;
    const int stride = (width * 3 + 3) & ~3;
    const Uint32 image_bytes = (Uint32)stride * (Uint32)height;

    SDL_RWwrite(dst, "BM", 1, 2);
    SDL_WriteLE32(dst, 14 + 40 + image_bytes);
    SDL_WriteLE32(dst, 0);
    SDL_WriteLE32(dst, 14 + 40);

    SDL_WriteLE32(dst, 40);
    SDL_WriteLE32(dst, (Uint32)width);
    SDL_WriteLE32(dst, (Uint32)height);
    SDL_WriteLE16(dst, 1);
    SDL_WriteLE16(dst, 24);
    SDL_WriteLE32(dst, BMP_RGB);
    SDL_WriteLE32(dst, image_bytes);
    SDL_WriteLE32(dst, 2835);
    SDL_WriteLE32(dst, 2835);
    SDL_WriteLE32(dst, 0);
    SDL_WriteLE32(dst, 0);

    Uint8 *row = calloc(1, (size_t)stride);
    if (!row) {
        shim_set_error("out of memory");
        goto done;
    }

    const int bpp = surface->format->BytesPerPixel;
    for (int y = height - 1; y >= 0; y--) {
        const Uint8 *in = (const Uint8 *)surface->pixels + (size_t)y * surface->pitch;
        for (int x = 0; x < width; x++, in += bpp) {
            Uint32 pixel = 0;
            memcpy(&pixel, in, (size_t)bpp < sizeof(pixel) ? (size_t)bpp : sizeof(pixel));
            Uint8 r, g, b;
            SDL_GetRGB(pixel, surface->format, &r, &g, &b);
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        SDL_RWwrite(dst, row, 1, stride);
    }
    free(row);
    rc = 0;

done:
    if (freedst && dst) SDL_RWclose(dst);
    return rc;
}
