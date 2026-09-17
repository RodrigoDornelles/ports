#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "capture.h"
#include "shim.h"

#define GL_RGBA           0x1908
#define GL_UNSIGNED_BYTE  0x1401

typedef void (*PFN_glReadPixels)(int, int, int, int, unsigned, unsigned, void *);

static struct {
    bool              resolved;
    dopo_ipc_format_t format;
    PFN_glReadPixels  read_pixels;
    char              name[64];
    int               fd;
    uint8_t          *map;
    size_t            size;
    int               w;
    int               h;
    uint8_t          *scratch;
} c = { .fd = -1 };

dopo_ipc_format_t capture_format(void) {
    if (!c.resolved) {
        const char *want = getenv(DOPO_ENV_FORMAT);
        c.format = DOPO_IPC_FORMAT_EGL;
        if (want) {
            if (!strcmp(want, "rgba8888")) c.format = DOPO_IPC_FORMAT_RGBA8888;
            else if (!strcmp(want, "rgb565")) c.format = DOPO_IPC_FORMAT_RGB565;
        }
        c.resolved = true;
    }
    return c.format;
}

bool capture_enabled(void) {
    return capture_format() != DOPO_IPC_FORMAT_EGL;
}

static size_t bytes_per_pixel(void) {
    return c.format == DOPO_IPC_FORMAT_RGB565 ? 2 : 4;
}

static void unmap(void) {
    if (c.map) munmap(c.map, c.size);
    if (c.fd >= 0) close(c.fd);
    if (c.name[0]) shm_unlink(c.name);
    c.map = NULL;
    c.fd = -1;
    c.size = 0;
    c.name[0] = '\0';
}

void capture_resize(int w, int h) {
    if (!capture_enabled()) {
        fprintf(stderr, SHIM_TAG " capture off (%s=%s)\n", DOPO_ENV_FORMAT,
                getenv(DOPO_ENV_FORMAT) ? getenv(DOPO_ENV_FORMAT) : "unset");
        return;
    }
    if (w <= 0 || h <= 0) return;
    if (w > DOPO_IPC_FB_MAX_WIDTH)  w = DOPO_IPC_FB_MAX_WIDTH;
    if (h > DOPO_IPC_FB_MAX_HEIGHT) h = DOPO_IPC_FB_MAX_HEIGHT;
    if (w == c.w && h == c.h && c.map) return;

    unmap();
    free(c.scratch);
    c.scratch = NULL;

    snprintf(c.name, sizeof(c.name), DOPO_SHM_FMT, (int)getpid());
    shm_unlink(c.name);

    c.fd = shm_open(c.name, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (c.fd < 0) {
        fprintf(stderr, SHIM_TAG " shm_open %s failed\n", c.name);
        c.name[0] = '\0';
        return;
    }

    c.size = (size_t)w * (size_t)h * bytes_per_pixel();
    if (ftruncate(c.fd, (off_t)c.size) != 0) {
        fprintf(stderr, SHIM_TAG " ftruncate %zu failed\n", c.size);
        unmap();
        return;
    }

    c.map = mmap(NULL, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, c.fd, 0);
    if (c.map == MAP_FAILED) {
        c.map = NULL;
        fprintf(stderr, SHIM_TAG " mmap %zu failed\n", c.size);
        unmap();
        return;
    }

    c.scratch = malloc((size_t)w * (size_t)h * 4);
    if (!c.scratch) {
        fprintf(stderr, SHIM_TAG " scratch alloc failed\n");
        unmap();
        return;
    }

    c.w = w;
    c.h = h;
    shim_ipc_send_blob(DOPO_IPC_PKT_FB_INIT, (uint8_t)c.format,
                       (uint16_t)w, (uint32_t)h, c.name, strlen(c.name) + 1);
    fprintf(stderr, SHIM_TAG " capture %dx%d %s via %s\n", w, h,
            c.format == DOPO_IPC_FORMAT_RGB565 ? "rgb565" : "rgba8888", c.name);
}

static bool resolve_gl(void) {
    if (c.read_pixels) return true;
    c.read_pixels = (PFN_glReadPixels)(uintptr_t)dlsym(RTLD_DEFAULT, "glReadPixels");
    if (!c.read_pixels) {
        fprintf(stderr, SHIM_TAG " glReadPixels not found, capture disabled\n");
        c.format   = DOPO_IPC_FORMAT_EGL;
        c.resolved = true;
    }
    return c.read_pixels != NULL;
}

static void flip_to_xrgb(const uint8_t *src, uint32_t *dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint32_t *in  = (const uint32_t *)(const void *)(src + (size_t)(h - 1 - y) * (size_t)w * 4);
        uint32_t       *out = dst + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            uint32_t p = in[x];
            out[x] = ((p & 0x000000FFu) << 16) | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu);
        }
    }
}

static void flip_to_565(const uint8_t *src, uint16_t *dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint8_t *in  = src + (size_t)(h - 1 - y) * (size_t)w * 4;
        uint16_t      *out = dst + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++, in += 4) {
            out[x] = (uint16_t)(((in[0] & 0xF8) << 8) | ((in[1] & 0xFC) << 3) | (in[2] >> 3));
        }
    }
}

void capture_frame(void) {
    if (!capture_enabled() || !c.map || !resolve_gl()) return;

    c.read_pixels(0, 0, c.w, c.h, GL_RGBA, GL_UNSIGNED_BYTE, c.scratch);

    if (c.format == DOPO_IPC_FORMAT_RGB565) {
        flip_to_565(c.scratch, (uint16_t *)(void *)c.map, c.w, c.h);
    } else {
        flip_to_xrgb(c.scratch, (uint32_t *)(void *)c.map, c.w, c.h);
    }

    shim_ipc_send(DOPO_IPC_PKT_FB_FRAME, (uint8_t)c.format,
                  (uint16_t)c.w, (uint32_t)c.h);
}

static void mask_shift_loss(uint32_t mask, unsigned *shift, unsigned *loss) {
    if (!mask) {
        *shift = 0;
        *loss  = 8;
        return;
    }
    *shift = (unsigned)__builtin_ctz(mask);
    *loss  = (unsigned)(8 - __builtin_popcount(mask));
}

static uint32_t pixel_read(const uint8_t *at, int bpp) {
    switch (bpp) {
        case 1:  return at[0];
        case 2:  return *(const uint16_t *)(const void *)at;
        case 3:  return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16);
        default: return *(const uint32_t *)(const void *)at;
    }
}

void capture_surface(const void *pixels, int pitch, const capture_pixels_t *fmt) {
    if (!capture_enabled() || !c.map || !pixels || !fmt || pitch <= 0) return;
    if (fmt->bpp == 1 && !fmt->palette) return;

    unsigned rshift, rloss, gshift, gloss, bshift, bloss;
    mask_shift_loss(fmt->rmask, &rshift, &rloss);
    mask_shift_loss(fmt->gmask, &gshift, &gloss);
    mask_shift_loss(fmt->bmask, &bshift, &bloss);

    const bool to565 = c.format == DOPO_IPC_FORMAT_RGB565;
    const int  rows  = fmt->h < c.h ? fmt->h : c.h;
    const int  cols  = fmt->w < c.w ? fmt->w : c.w;

    for (int y = 0; y < rows; y++) {
        const uint8_t *in   = (const uint8_t *)pixels + (size_t)y * (size_t)pitch;
        uint32_t      *out32 = (uint32_t *)(void *)c.map + (size_t)y * (size_t)c.w;
        uint16_t      *out16 = (uint16_t *)(void *)c.map + (size_t)y * (size_t)c.w;

        for (int x = 0; x < cols; x++, in += fmt->bpp) {
            uint32_t r, g, b;
            if (fmt->bpp == 1) {
                uint32_t argb = fmt->palette[in[0]];
                r = (argb >> 16) & 0xFF;
                g = (argb >> 8)  & 0xFF;
                b = argb & 0xFF;
            } else {
                uint32_t p = pixel_read(in, fmt->bpp);
                r = ((p & fmt->rmask) >> rshift) << rloss;
                g = ((p & fmt->gmask) >> gshift) << gloss;
                b = ((p & fmt->bmask) >> bshift) << bloss;
            }
            if (to565) out16[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
            else       out32[x] = (r << 16) | (g << 8) | b;
        }
    }

    shim_ipc_send(DOPO_IPC_PKT_FB_FRAME, (uint8_t)c.format,
                  (uint16_t)c.w, (uint32_t)c.h);
}

void capture_quit(void) {
    unmap();
    free(c.scratch);
    c.scratch = NULL;
    c.w = c.h = 0;
}
