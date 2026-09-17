#ifndef DOPO_CAPTURE_H
#define DOPO_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "ipc.h"

typedef struct {
    int             w, h;     /* size of the source image */
    int             bpp;      /* bytes per pixel: 1, 2, 3 or 4 */
    uint32_t        rmask;
    uint32_t        gmask;
    uint32_t        bmask;
    const uint32_t *palette;  /* 256 xrgb8888 entries, required when bpp == 1 */
} capture_pixels_t;

dopo_ipc_format_t capture_format(void);
bool              capture_enabled(void);
void              capture_resize(int w, int h);
void              capture_frame(void);
void              capture_surface(const void *pixels, int pitch, const capture_pixels_t *fmt);
void              capture_quit(void);

#endif
