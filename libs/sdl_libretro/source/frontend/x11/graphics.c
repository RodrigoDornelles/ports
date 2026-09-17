#define _GNU_SOURCE
#include <strings.h>

#include "x11_shim.h"

/*
 * Drawing calls exist only so window and cursor setup code runs to completion;
 * the frames the core shows come from the EGL hook, never from here.
 */

static unsigned long s_next_id = 0x00800001UL;

static unsigned long id_new(void) {
    return s_next_id++;
}

GC XCreateGC(Display *display, Drawable d, unsigned long valuemask, XGCValues *values) {
    (void)display; (void)d; (void)valuemask; (void)values;
    return (GC)calloc(1, sizeof(XGCValues) + sizeof(void *));
}

int XFreeGC(Display *display, GC gc) {
    (void)display;
    free(gc);
    return 0;
}

int XSetForeground(Display *display, GC gc, unsigned long foreground) {
    (void)display; (void)gc; (void)foreground;
    return 0;
}

int XSetBackground(Display *display, GC gc, unsigned long background) {
    (void)display; (void)gc; (void)background;
    return 0;
}

int XFillRectangle(Display *display, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height) {
    (void)display; (void)d; (void)gc; (void)x; (void)y; (void)width; (void)height;
    return 0;
}

Pixmap XCreatePixmap(Display *display, Drawable d, unsigned int width, unsigned int height,
                     unsigned int depth) {
    (void)display; (void)d; (void)width; (void)height; (void)depth;
    return (Pixmap)id_new();
}

Pixmap XCreateBitmapFromData(Display *display, Drawable d, const char *data,
                             unsigned int width, unsigned int height) {
    (void)display; (void)d; (void)data; (void)width; (void)height;
    return (Pixmap)id_new();
}

int XFreePixmap(Display *display, Pixmap pixmap) {
    (void)display; (void)pixmap;
    return 0;
}

static int image_destroy(XImage *image) {
    if (!image) return 0;
    free(image->data);
    free(image);
    return 1;
}

static unsigned long image_get_pixel(XImage *image, int x, int y) {
    if (!image || !image->data || image->bits_per_pixel != 32) return 0;
    if (x < 0 || y < 0 || x >= image->width || y >= image->height) return 0;

    unsigned long pixel;
    memcpy(&pixel, image->data + (size_t)y * (size_t)image->bytes_per_line + (size_t)x * 4,
           sizeof(uint32_t));
    return pixel & 0xFFFFFFFFUL;
}

static int image_put_pixel(XImage *image, int x, int y, unsigned long pixel) {
    if (!image || !image->data || image->bits_per_pixel != 32) return 0;
    if (x < 0 || y < 0 || x >= image->width || y >= image->height) return 0;

    uint32_t value = (uint32_t)pixel;
    memcpy(image->data + (size_t)y * (size_t)image->bytes_per_line + (size_t)x * 4,
           &value, sizeof(value));
    return 1;
}

static XImage *image_sub(XImage *image, int x, int y, unsigned int width, unsigned int height) {
    (void)image; (void)x; (void)y; (void)width; (void)height;
    return NULL;
}

static int image_add_pixel(XImage *image, long value) {
    (void)image; (void)value;
    return 0;
}

XImage *XCreateImage(Display *display, Visual *visual, unsigned int depth, int format,
                     int offset, char *data, unsigned int width, unsigned int height,
                     int bitmap_pad, int bytes_per_line) {
    (void)display;

    XImage *image = calloc(1, sizeof(*image));
    if (!image) return NULL;

    int bits = depth > 16 ? 32 : (depth > 8 ? 16 : (depth > 1 ? 8 : 1));

    image->width            = (int)width;
    image->height           = (int)height;
    image->xoffset          = offset;
    image->format           = format;
    image->data             = data;
    image->byte_order       = LSBFirst;
    image->bitmap_unit      = 32;
    image->bitmap_bit_order = LSBFirst;
    image->bitmap_pad       = bitmap_pad > 0 ? bitmap_pad : 32;
    image->depth            = (int)depth;
    image->bits_per_pixel   = bits;
    image->bytes_per_line   = bytes_per_line > 0
                            ? bytes_per_line
                            : (int)(((width * (unsigned)bits + 31) / 32) * 4);
    image->red_mask         = visual ? visual->red_mask   : 0x00FF0000;
    image->green_mask       = visual ? visual->green_mask : 0x0000FF00;
    image->blue_mask        = visual ? visual->blue_mask  : 0x000000FF;

    image->f.destroy_image = image_destroy;
    image->f.get_pixel     = image_get_pixel;
    image->f.put_pixel     = image_put_pixel;
    image->f.sub_image     = image_sub;
    image->f.add_pixel     = image_add_pixel;
    return image;
}

int XPutImage(Display *display, Drawable d, GC gc, XImage *image, int src_x, int src_y,
              int dest_x, int dest_y, unsigned int width, unsigned int height) {
    (void)display; (void)d; (void)gc; (void)image;
    (void)src_x; (void)src_y; (void)dest_x; (void)dest_y; (void)width; (void)height;
    return 0;
}

Status XQueryBestCursor(Display *display, Drawable d, unsigned int width, unsigned int height,
                        unsigned int *width_return, unsigned int *height_return) {
    (void)display; (void)d;
    if (width_return)  *width_return  = width  ? width  : 32;
    if (height_return) *height_return = height ? height : 32;
    return 1;
}

Cursor XCreatePixmapCursor(Display *display, Pixmap source, Pixmap mask,
                           XColor *foreground_color, XColor *background_color,
                           unsigned int x, unsigned int y) {
    (void)display; (void)source; (void)mask;
    (void)foreground_color; (void)background_color; (void)x; (void)y;
    return (Cursor)id_new();
}

Cursor XCreateFontCursor(Display *display, unsigned int shape) {
    (void)display; (void)shape;
    return (Cursor)id_new();
}

int XFreeCursor(Display *display, Cursor cursor) {
    (void)display; (void)cursor;
    return 0;
}

int XDefineCursor(Display *display, Window w, Cursor cursor) {
    (void)display; (void)w; (void)cursor;
    return 0;
}

int XUndefineCursor(Display *display, Window w) {
    (void)display; (void)w;
    return 0;
}

static bool color_parse(const char *name, unsigned short *r, unsigned short *g, unsigned short *b) {
    if (!name) return false;

    if (name[0] == '#') {
        unsigned value = 0;
        if (sscanf(name + 1, "%x", &value) != 1) return false;
        *r = (unsigned short)(((value >> 16) & 0xFF) * 257);
        *g = (unsigned short)(((value >> 8)  & 0xFF) * 257);
        *b = (unsigned short)((value & 0xFF) * 257);
        return true;
    }
    if (strcasecmp(name, "black") == 0) { *r = *g = *b = 0;      return true; }
    if (strcasecmp(name, "white") == 0) { *r = *g = *b = 0xFFFF; return true; }
    return false;
}

Status XAllocNamedColor(Display *display, Colormap colormap, const char *color_name,
                        XColor *screen_def_return, XColor *exact_def_return) {
    (void)display; (void)colormap;

    unsigned short r = 0, g = 0, b = 0;
    if (!color_parse(color_name, &r, &g, &b)) return 0;

    XColor color;
    memset(&color, 0, sizeof(color));
    color.red   = r;
    color.green = g;
    color.blue  = b;
    color.flags = DoRed | DoGreen | DoBlue;
    color.pixel = ((unsigned long)(r >> 8) << 16) | ((unsigned long)(g >> 8) << 8) | (b >> 8);

    if (screen_def_return) *screen_def_return = color;
    if (exact_def_return)  *exact_def_return  = color;
    return 1;
}

Status XAllocColor(Display *display, Colormap colormap, XColor *screen_in_out) {
    (void)display; (void)colormap;
    if (!screen_in_out) return 0;
    screen_in_out->pixel = ((unsigned long)(screen_in_out->red   >> 8) << 16)
                         | ((unsigned long)(screen_in_out->green >> 8) << 8)
                         | (screen_in_out->blue >> 8);
    return 1;
}
