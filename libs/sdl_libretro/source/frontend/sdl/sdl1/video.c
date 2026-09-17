#define _GNU_SOURCE
#include "sdl1.h"

static SDL_Surface *s_screen;
static win_window  *s_win;
static void        *s_gl_context;
static bool         s_opengl;
static bool         s_fullscreen;
static int          s_swap_control = 1;
static SDL_GrabMode s_grab = SDL_GRAB_OFF;
static char         s_title[256];
static char         s_icon_title[256];
static uint32_t     s_palette_cache[256];

SDL_Surface *shim_screen(void) {
    return s_screen;
}

void win_on_close(void) {
    shim_events_quit_request();
}

void win_on_resize(int w, int h) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type     = SDL_VIDEORESIZE;
    ev.resize.w = w;
    ev.resize.h = h;
    shim_events_push(&ev);
}

void win_on_move(int x, int y) {
    (void)x; (void)y;
}

void win_on_expose(void) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = SDL_VIDEOEXPOSE;
    shim_events_push(&ev);
}

void win_on_focus(bool gained) {
    shim_events_active(SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS, gained);
}

void win_on_map(bool mapped) {
    shim_events_active(SDL_APPACTIVE, mapped);
}

static void screen_masks(int bpp, Uint32 *r, Uint32 *g, Uint32 *b, Uint32 *a) {
    *a = 0;
    switch (bpp) {
        case 8:
            *r = *g = *b = 0;
            break;
        case 15:
            *r = 0x7C00; *g = 0x03E0; *b = 0x001F;
            break;
        case 16:
            *r = 0xF800; *g = 0x07E0; *b = 0x001F;
            break;
        default:
            *r = 0x00FF0000; *g = 0x0000FF00; *b = 0x000000FF;
            break;
    }
}

static void screen_free(void) {
    if (!s_screen) return;
    if (s_screen->flags & SDL_OPENGL) s_screen->flags &= ~(Uint32)SDL_PREALLOC;
    SDL_FreeSurface(s_screen);
    s_screen = NULL;
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) {
    if (!win_init()) return NULL;

    if (width  <= 0) width  = 640;
    if (height <= 0) height = 480;
    if (bpp <= 0) bpp = capture_format() == DOPO_IPC_FORMAT_RGB565 ? 16 : 32;

    const bool opengl = (flags & SDL_OPENGL) != 0;

    if (s_win && opengl != s_opengl) {
        if (s_gl_context) win_gl_delete_context(s_gl_context);
        s_gl_context = NULL;
        win_destroy(s_win);
        s_win = NULL;
    }

    if (!s_win) {
        win_config_t cfg = {
            .title      = s_title,
            .x          = 0,
            .y          = 0,
            .w          = width,
            .h          = height,
            .fullscreen = (flags & SDL_FULLSCREEN) != 0,
            .hidden     = false,
            .opengl     = opengl,
        };
        int dw, dh;
        win_desktop_size(&dw, &dh);
        cfg.x = (dw - width) / 2  > 0 ? (dw - width) / 2  : 0;
        cfg.y = (dh - height) / 2 > 0 ? (dh - height) / 2 : 0;

        s_win = win_create(&cfg);
        if (!s_win) return NULL;
        s_opengl = opengl;

        if (opengl) {
            s_gl_context = win_gl_create_context(s_win);
            if (!s_gl_context) {
                win_destroy(s_win);
                s_win = NULL;
                return NULL;
            }
            win_gl_set_swap_interval(s_swap_control);
        }

        if (win_synthetic_focus()) {
            shim_events_active(SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS, true);
        }
    } else {
        win_set_size(s_win, width, height);
        win_set_fullscreen(s_win, (flags & SDL_FULLSCREEN) != 0);
        capture_resize(width, height);
        shim_ipc_send(DOPO_IPC_PKT_WINDOW, 0, (uint16_t)width, (uint32_t)height);
    }

    s_fullscreen = (flags & SDL_FULLSCREEN) != 0;

    Uint32 rmask, gmask, bmask, amask;
    screen_masks(bpp, &rmask, &gmask, &bmask, &amask);

    screen_free();
    s_screen = shim_surface_new(NULL, width, height, bpp, 0, rmask, gmask, bmask, amask);
    if (!s_screen) return NULL;

    s_screen->flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;
    if (s_fullscreen) s_screen->flags |= SDL_FULLSCREEN;
    if (flags & SDL_RESIZABLE) s_screen->flags |= SDL_RESIZABLE;
    if (opengl) {
        free(s_screen->pixels);
        s_screen->pixels = NULL;
        s_screen->flags |= SDL_OPENGL | SDL_PREALLOC;
    }

    fprintf(stderr, SHIM_TAG " video mode %dx%d %dbpp%s\n", width, height, bpp,
            opengl ? " opengl" : "");
    return s_screen;
}

static void screen_present(void) {
    if (!s_screen || !s_screen->pixels) return;
    capture_pixels_t fmt;
    shim_surface_pixels(s_screen, &fmt, s_palette_cache);
    capture_surface(s_screen->pixels, s_screen->pitch, &fmt);
}

int SDL_Flip(SDL_Surface *screen) {
    if (!screen) return -1;
    if (screen->flags & SDL_OPENGL) {
        win_gl_swap(s_win);
        return 0;
    }
    if (screen == s_screen) screen_present();
    return 0;
}

void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
    (void)numrects; (void)rects;
    if (screen == s_screen) screen_present();
}

void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h) {
    (void)x; (void)y; (void)w; (void)h;
    if (screen == s_screen) screen_present();
}

SDL_Surface *SDL_GetVideoSurface(void) {
    return s_screen;
}

const SDL_VideoInfo *SDL_GetVideoInfo(void) {
    static SDL_VideoInfo    info;
    static SDL_PixelFormat  format;

    int dw, dh;
    win_desktop_size(&dw, &dh);

    if (s_screen) {
        format = *s_screen->format;
    } else {
        memset(&format, 0, sizeof(format));
        format.BitsPerPixel  = 32;
        format.BytesPerPixel = 4;
        format.Rmask = 0x00FF0000;
        format.Gmask = 0x0000FF00;
        format.Bmask = 0x000000FF;
        format.Rshift = 16;
        format.Gshift = 8;
        format.alpha  = SDL_ALPHA_OPAQUE;
    }
    format.palette = NULL;

    memset(&info, 0, sizeof(info));
    info.hw_available = 0;
    info.wm_available = win_has_x() ? 1 : 0;
    info.video_mem    = 0;
    info.vfmt         = &format;
    info.current_w    = s_screen ? s_screen->w : dw;
    info.current_h    = s_screen ? s_screen->h : dh;
    return &info;
}

int SDL_VideoModeOK(int width, int height, int bpp, Uint32 flags) {
    (void)flags;
    if (width <= 0 || height <= 0) return 0;
    return bpp > 0 ? bpp : 32;
}

static const int k_modes[][2] = {
    { 1920, 1080 }, { 1600, 900 }, { 1366, 768 }, { 1280, 720 },
    { 1024, 768 },  { 800, 600 },  { 640, 480 },  { 320, 240 },
};

SDL_Rect **SDL_ListModes(SDL_PixelFormat *format, Uint32 flags) {
    (void)format; (void)flags;
    static SDL_Rect  rects[1 + sizeof(k_modes) / sizeof(*k_modes)];
    static SDL_Rect *list[2 + sizeof(k_modes) / sizeof(*k_modes)];

    int dw, dh;
    win_desktop_size(&dw, &dh);

    int n = 0;
    rects[n] = (SDL_Rect){ 0, 0, (Uint16)dw, (Uint16)dh };
    list[n]  = &rects[n];
    n++;

    for (size_t i = 0; i < sizeof(k_modes) / sizeof(*k_modes); i++) {
        if (k_modes[i][0] > dw || k_modes[i][1] > dh) continue;
        if (k_modes[i][0] == dw && k_modes[i][1] == dh) continue;
        rects[n] = (SDL_Rect){ 0, 0, (Uint16)k_modes[i][0], (Uint16)k_modes[i][1] };
        list[n]  = &rects[n];
        n++;
    }
    list[n] = NULL;
    return list;
}

char *SDL_VideoDriverName(char *namebuf, int maxlen) {
    if (!namebuf || maxlen <= 0) return NULL;
    snprintf(namebuf, (size_t)maxlen, "%s", win_driver());
    return namebuf;
}

int SDL_VideoInit(const char *driver_name, Uint32 flags) {
    (void)driver_name; (void)flags;
    return win_init() ? 0 : -1;
}

void SDL_VideoQuit(void) {
    shim_video_quit();
}

void shim_video_quit(void) {
    screen_free();
    if (s_gl_context) win_gl_delete_context(s_gl_context);
    s_gl_context = NULL;
    s_win        = NULL;
    s_opengl     = false;
    capture_quit();
    win_quit();
}

int SDL_GL_LoadLibrary(const char *path) {
    (void)path;
    return win_gl_init() ? 0 : -1;
}

void *SDL_GL_GetProcAddress(const char *proc) {
    return win_gl_proc_address(proc);
}

int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
    if (attr == SDL_GL_SWAP_CONTROL) {
        s_swap_control = value;
        return s_win ? win_gl_set_swap_interval(value) : 0;
    }
    return win_gl_set_attribute((win_gl_attr_t)attr, value);
}

int SDL_GL_GetAttribute(SDL_GLattr attr, int *value) {
    if (!value) return -1;
    if (attr == SDL_GL_SWAP_CONTROL) {
        *value = win_gl_get_swap_interval();
        return 0;
    }
    return win_gl_get_attribute((win_gl_attr_t)attr, value);
}

void SDL_GL_SwapBuffers(void) {
    win_gl_swap(s_win);
}

void SDL_GL_UpdateRects(int numrects, SDL_Rect *rects) {
    (void)numrects; (void)rects;
}

void SDL_GL_Lock(void) {
}

void SDL_GL_Unlock(void) {
}

void SDL_WM_SetCaption(const char *title, const char *icon) {
    if (title) snprintf(s_title, sizeof(s_title), "%s", title);
    if (icon)  snprintf(s_icon_title, sizeof(s_icon_title), "%s", icon);
    if (s_win) win_set_title(s_win, s_title);
}

void SDL_WM_GetCaption(char **title, char **icon) {
    if (title) *title = s_title;
    if (icon)  *icon  = s_icon_title;
}

void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask) {
    (void)icon; (void)mask;
}

int SDL_WM_IconifyWindow(void) {
    return 0;
}

int SDL_WM_ToggleFullScreen(SDL_Surface *surface) {
    if (!s_win) return 0;
    s_fullscreen = !s_fullscreen;
    win_set_fullscreen(s_win, s_fullscreen);
    if (surface) {
        if (s_fullscreen) surface->flags |= SDL_FULLSCREEN;
        else              surface->flags &= ~(Uint32)SDL_FULLSCREEN;
    }
    return 1;
}

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode) {
    if (mode == SDL_GRAB_QUERY) return s_grab;
    s_grab = mode;
    win_set_grab(s_win, mode != SDL_GRAB_OFF);
    return s_grab;
}

SDL_Cursor *SDL_CreateCursor(Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y) {
    if (!data || !mask || w <= 0 || h <= 0) {
        shim_set_error("invalid cursor data");
        return NULL;
    }

    SDL_Cursor *cursor = calloc(1, sizeof(*cursor));
    uint32_t   *argb   = calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!cursor || !argb) {
        free(cursor);
        free(argb);
        shim_set_error("out of memory");
        return NULL;
    }

    /* one bit per pixel, msb first; data selects black/white, mask opacity */
    const int stride = (w + 7) / 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int      bit   = 7 - (x % 8);
            unsigned index = (unsigned)(y * stride + x / 8);
            bool     black = (data[index] >> bit) & 1;
            bool     opaque = (mask[index] >> bit) & 1;
            uint32_t rgb   = black ? 0x000000u : 0xFFFFFFu;
            argb[(size_t)y * (size_t)w + (size_t)x] = opaque ? (0xFF000000u | rgb) : 0;
        }
    }

    cursor->area      = (SDL_Rect){ 0, 0, (Uint16)w, (Uint16)h };
    cursor->hot_x     = (Sint16)hot_x;
    cursor->hot_y     = (Sint16)hot_y;
    cursor->wm_cursor = (struct WMcursor *)win_cursor_color(argb, w, h, hot_x, hot_y);
    free(argb);
    return cursor;
}

void SDL_SetCursor(SDL_Cursor *cursor) {
    if (!cursor) return;
    win_cursor_set((win_cursor *)cursor->wm_cursor);
}

SDL_Cursor *SDL_GetCursor(void) {
    static SDL_Cursor current;
    current.wm_cursor = (struct WMcursor *)win_cursor_current();
    return &current;
}

void SDL_FreeCursor(SDL_Cursor *cursor) {
    if (!cursor) return;
    win_cursor_free((win_cursor *)cursor->wm_cursor);
    free(cursor);
}

int SDL_ShowCursor(int toggle) {
    int previous = win_cursor_shown() ? SDL_ENABLE : SDL_DISABLE;
    if (toggle < 0) return previous;
    win_cursor_show(toggle != SDL_DISABLE);
    return previous;
}

Uint8 SDL_GetMouseState(int *x, int *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

Uint8 SDL_GetRelativeMouseState(int *x, int *y) {
    return SDL_GetMouseState(x, y);
}

void SDL_WarpMouse(Uint16 x, Uint16 y) {
    (void)x; (void)y;
}
