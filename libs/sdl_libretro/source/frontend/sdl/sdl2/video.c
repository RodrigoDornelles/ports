#define _GNU_SOURCE
#include "sdl2.h"
#include "capture.h"

struct SDL_Window {
    Uint32      id;
    Uint32      flags;
    int         x, y, w, h;
    int         min_w, min_h;
    char        title[256];
    win_window *win;
};

static SDL_Window *s_window;
static Uint32      s_next_id = 1;
static bool        s_grabbed;
static int         s_extra_attrs[32];

uint32_t shim_window_id(void) {
    return s_window ? s_window->id : 0;
}

void win_on_close(void) {
    shim_events_window(SDL_WINDOWEVENT_CLOSE, 0, 0);
    shim_events_quit_request();
}

void win_on_resize(int w, int h) {
    if (!s_window) return;
    s_window->w = w;
    s_window->h = h;
    shim_events_window(SDL_WINDOWEVENT_SIZE_CHANGED, w, h);
    shim_events_window(SDL_WINDOWEVENT_RESIZED, w, h);
}

void win_on_move(int x, int y) {
    if (!s_window) return;
    s_window->x = x;
    s_window->y = y;
    shim_events_window(SDL_WINDOWEVENT_MOVED, x, y);
}

void win_on_expose(void) {
    shim_events_window(SDL_WINDOWEVENT_EXPOSED, 0, 0);
}

void win_on_focus(bool gained) {
    if (!s_window) return;
    if (gained) s_window->flags |= SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
    else        s_window->flags &= ~(Uint32)(SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
    shim_events_window(gained ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
}

void win_on_map(bool mapped) {
    if (!s_window) return;
    if (mapped) s_window->flags |= SDL_WINDOW_SHOWN;
    shim_events_window(mapped ? SDL_WINDOWEVENT_SHOWN : SDL_WINDOWEVENT_HIDDEN, 0, 0);
}

static int gl_attr_map(SDL_GLattr attr) {
    if ((int)attr >= SDL_GL_RED_SIZE && (int)attr <= SDL_GL_ACCELERATED_VISUAL) return (int)attr;
    switch (attr) {
        case SDL_GL_CONTEXT_MAJOR_VERSION:       return WIN_GL_CONTEXT_MAJOR_VERSION;
        case SDL_GL_CONTEXT_MINOR_VERSION:       return WIN_GL_CONTEXT_MINOR_VERSION;
        case SDL_GL_CONTEXT_FLAGS:               return WIN_GL_CONTEXT_FLAGS;
        case SDL_GL_CONTEXT_PROFILE_MASK:        return WIN_GL_CONTEXT_PROFILE_MASK;
        case SDL_GL_SHARE_WITH_CURRENT_CONTEXT:  return WIN_GL_SHARE_WITH_CURRENT_CONTEXT;
        default:                                 return -1;
    }
}

int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
    int mapped = gl_attr_map(attr);
    if (mapped < 0) {
        s_extra_attrs[(int)attr & 31] = value;
        return 0;
    }
    return win_gl_set_attribute((win_gl_attr_t)mapped, value);
}

int SDL_GL_GetAttribute(SDL_GLattr attr, int *value) {
    if (!value) return -1;
    int mapped = gl_attr_map(attr);
    if (mapped < 0) {
        *value = s_extra_attrs[(int)attr & 31];
        return 0;
    }
    return win_gl_get_attribute((win_gl_attr_t)mapped, value);
}

void SDL_GL_ResetAttributes(void) {
    memset(s_extra_attrs, 0, sizeof(s_extra_attrs));
    win_gl_reset_attributes();
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window) {
    if (!window) {
        shim_set_error("passed a NULL window");
        return NULL;
    }
    return (SDL_GLContext)win_gl_create_context(window->win);
}

int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context) {
    return win_gl_make_current(window ? window->win : NULL, context);
}

void SDL_GL_DeleteContext(SDL_GLContext context) {
    win_gl_delete_context(context);
}

SDL_GLContext SDL_GL_GetCurrentContext(void) {
    return (SDL_GLContext)win_gl_current_context();
}

SDL_Window *SDL_GL_GetCurrentWindow(void) {
    return s_window;
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (window) win_gl_swap(window->win);
}

int SDL_GL_SetSwapInterval(int interval) {
    return win_gl_set_swap_interval(interval);
}

int SDL_GL_GetSwapInterval(void) {
    return win_gl_get_swap_interval();
}

void *SDL_GL_GetProcAddress(const char *proc) {
    return win_gl_proc_address(proc);
}

int SDL_GL_LoadLibrary(const char *path) {
    (void)path;
    return win_gl_init() ? 0 : -1;
}

void SDL_GL_UnloadLibrary(void) {
}

SDL_bool SDL_GL_ExtensionSupported(const char *extension) {
    (void)extension;
    return SDL_FALSE;
}

void SDL_GL_GetDrawableSize(SDL_Window *window, int *w, int *h) {
    win_gl_drawable_size(window ? window->win : NULL, w, h);
}

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    if (!win_init()) return NULL;
    if (s_window) {
        shim_set_error("only one window is supported");
        return NULL;
    }

    SDL_Window *win = calloc(1, sizeof(*win));
    if (!win) {
        shim_set_error("out of memory");
        return NULL;
    }

    int dw, dh;
    win_desktop_size(&dw, &dh);
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        w = dw;
        h = dh;
    }
    if (w <= 0) w = 640;
    if (h <= 0) h = 480;
    if (SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x)) x = (dw - w) / 2;
    if (SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y)) y = (dh - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    win->id    = s_next_id++;
    win->flags = flags | SDL_WINDOW_SHOWN;
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    snprintf(win->title, sizeof(win->title), "%s", title ? title : "");

    win_config_t cfg = {
        .title      = win->title,
        .x          = x,
        .y          = y,
        .w          = w,
        .h          = h,
        .fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0,
        .hidden     = (flags & SDL_WINDOW_HIDDEN) != 0,
        .opengl     = (flags & SDL_WINDOW_OPENGL) != 0,
    };
    win->win = win_create(&cfg);
    if (!win->win) {
        free(win);
        return NULL;
    }

    s_window = win;

    if (win_synthetic_focus()) {
        win->flags |= SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
        shim_events_window(SDL_WINDOWEVENT_SHOWN, 0, 0);
        shim_events_window(SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
    }
    return win;
}

void SDL_DestroyWindow(SDL_Window *window) {
    if (!window) return;
    win_destroy(window->win);
    if (s_window == window) s_window = NULL;
    free(window);
}

void shim_video_quit(void) {
    SDL_Window *window = s_window;
    s_window = NULL;
    free(window);
    capture_quit();
    win_quit();
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    if (w) *w = window ? window->w : 0;
    if (h) *h = window ? window->h : 0;
}

void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y) {
    if (x) *x = window ? window->x : 0;
    if (y) *y = window ? window->y : 0;
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h) {
    if (!window || w <= 0 || h <= 0) return;
    window->w = w;
    window->h = h;
    win_set_size(window->win, w, h);
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y) {
    if (!window) return;
    int dw, dh;
    win_desktop_size(&dw, &dh);
    if (SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x)) x = (dw - window->w) / 2;
    if (SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y)) y = (dh - window->h) / 2;
    window->x = x;
    window->y = y;
    win_set_position(window->win, x, y);
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title) {
    if (!window) return;
    snprintf(window->title, sizeof(window->title), "%s", title ? title : "");
    win_set_title(window->win, window->title);
}

const char *SDL_GetWindowTitle(SDL_Window *window) {
    return window ? window->title : "";
}

int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {
    if (!window) return -1;
    bool enable = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    window->flags &= ~(Uint32)SDL_WINDOW_FULLSCREEN_DESKTOP;
    window->flags |= flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
    win_set_fullscreen(window->win, enable);
    if (!win_has_x() && enable) win_geometry(window->win, NULL, NULL, &window->w, &window->h);
    return 0;
}

void SDL_SetWindowResizable(SDL_Window *window, SDL_bool resizable) {
    if (!window) return;
    if (resizable) window->flags |= SDL_WINDOW_RESIZABLE;
    else           window->flags &= ~(Uint32)SDL_WINDOW_RESIZABLE;
}

void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h) {
    if (!window) return;
    window->min_w = min_w;
    window->min_h = min_h;
}

void SDL_GetWindowMinimumSize(SDL_Window *window, int *w, int *h) {
    if (w) *w = window ? window->min_w : 0;
    if (h) *h = window ? window->min_h : 0;
}

void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered) {
    (void)window; (void)bordered;
}

void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
    (void)window; (void)icon;
}

void SDL_ShowWindow(SDL_Window *window) {
    if (!window) return;
    window->flags |= SDL_WINDOW_SHOWN;
    win_set_visible(window->win, true);
}

void SDL_HideWindow(SDL_Window *window) {
    if (!window) return;
    window->flags &= ~(Uint32)SDL_WINDOW_SHOWN;
    win_set_visible(window->win, false);
}

void SDL_RaiseWindow(SDL_Window *window) {
    if (window) win_raise(window->win);
}

void SDL_MinimizeWindow(SDL_Window *window) { (void)window; }
void SDL_MaximizeWindow(SDL_Window *window) { (void)window; }
void SDL_RestoreWindow(SDL_Window *window)  { (void)window; }

Uint32 SDL_GetWindowFlags(SDL_Window *window) {
    return window ? window->flags : 0;
}

Uint32 SDL_GetWindowID(SDL_Window *window) {
    return window ? window->id : 0;
}

SDL_Window *SDL_GetWindowFromID(Uint32 id) {
    return (s_window && s_window->id == id) ? s_window : NULL;
}

int SDL_GetWindowDisplayIndex(SDL_Window *window) {
    (void)window;
    return 0;
}

Uint32 SDL_GetWindowPixelFormat(SDL_Window *window) {
    (void)window;
    return SDL_PIXELFORMAT_RGB888;
}

static const win_cursor_shape_t k_cursor_shapes[SDL_NUM_SYSTEM_CURSORS] = {
    [SDL_SYSTEM_CURSOR_ARROW]     = WIN_CURSOR_ARROW,
    [SDL_SYSTEM_CURSOR_IBEAM]     = WIN_CURSOR_IBEAM,
    [SDL_SYSTEM_CURSOR_WAIT]      = WIN_CURSOR_WAIT,
    [SDL_SYSTEM_CURSOR_CROSSHAIR] = WIN_CURSOR_CROSSHAIR,
    [SDL_SYSTEM_CURSOR_WAITARROW] = WIN_CURSOR_WAIT,
    [SDL_SYSTEM_CURSOR_SIZENWSE]  = WIN_CURSOR_SIZENWSE,
    [SDL_SYSTEM_CURSOR_SIZENESW]  = WIN_CURSOR_SIZENESW,
    [SDL_SYSTEM_CURSOR_SIZEWE]    = WIN_CURSOR_SIZEWE,
    [SDL_SYSTEM_CURSOR_SIZENS]    = WIN_CURSOR_SIZENS,
    [SDL_SYSTEM_CURSOR_SIZEALL]   = WIN_CURSOR_SIZEALL,
    [SDL_SYSTEM_CURSOR_NO]        = WIN_CURSOR_NO,
    [SDL_SYSTEM_CURSOR_HAND]      = WIN_CURSOR_HAND,
};

SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id) {
    win_cursor_shape_t shape = WIN_CURSOR_ARROW;
    if (id >= 0 && id < SDL_NUM_SYSTEM_CURSORS) shape = k_cursor_shapes[id];
    return (SDL_Cursor *)win_cursor_system(shape);
}

SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *surface, int hot_x, int hot_y) {
    if (!surface || !surface->pixels || surface->format->BytesPerPixel != 4) {
        return (SDL_Cursor *)win_cursor_color(NULL, 0, 0, 0, 0);
    }

    uint32_t *argb = malloc((size_t)surface->w * (size_t)surface->h * sizeof(uint32_t));
    if (!argb) {
        shim_set_error("out of memory");
        return NULL;
    }

    const SDL_PixelFormat *fmt = surface->format;
    for (int y = 0; y < surface->h; y++) {
        const Uint32 *row = (const Uint32 *)((const Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch);
        for (int x = 0; x < surface->w; x++) {
            Uint32 px = row[x];
            Uint32 r  = fmt->Rmask ? ((px & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss : 0;
            Uint32 g  = fmt->Gmask ? ((px & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss : 0;
            Uint32 b  = fmt->Bmask ? ((px & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss : 0;
            Uint32 a  = fmt->Amask ? ((px & fmt->Amask) >> fmt->Ashift) << fmt->Aloss : 255;
            r = r * a / 255;
            g = g * a / 255;
            b = b * a / 255;
            argb[(size_t)y * (size_t)surface->w + (size_t)x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    SDL_Cursor *cursor = (SDL_Cursor *)win_cursor_color(argb, surface->w, surface->h, hot_x, hot_y);
    free(argb);
    return cursor;
}

SDL_Cursor *SDL_GetDefaultCursor(void) {
    return (SDL_Cursor *)win_cursor_default();
}

SDL_Cursor *SDL_GetCursor(void) {
    return (SDL_Cursor *)win_cursor_current();
}

void SDL_SetCursor(SDL_Cursor *cursor) {
    win_cursor_set((win_cursor *)cursor);
}

void SDL_FreeCursor(SDL_Cursor *cursor) {
    win_cursor_free((win_cursor *)cursor);
}

int SDL_ShowCursor(int toggle) {
    int previous = win_cursor_shown() ? SDL_ENABLE : SDL_DISABLE;
    if (toggle < 0) return previous;
    win_cursor_show(toggle != SDL_DISABLE);
    return previous;
}

void SDL_SetWindowGrab(SDL_Window *window, SDL_bool grabbed) {
    if (!window) return;
    s_grabbed = grabbed == SDL_TRUE;
    if (grabbed) window->flags |= SDL_WINDOW_INPUT_GRABBED;
    else         window->flags &= ~(Uint32)SDL_WINDOW_INPUT_GRABBED;
    win_set_grab(window->win, s_grabbed);
}

SDL_bool SDL_GetWindowGrab(SDL_Window *window) {
    if (!window) return SDL_FALSE;
    return (window->flags & SDL_WINDOW_INPUT_GRABBED) ? SDL_TRUE : SDL_FALSE;
}

SDL_Window *SDL_GetGrabbedWindow(void) {
    return s_grabbed ? s_window : NULL;
}

SDL_Window *SDL_GetKeyboardFocus(void) {
    if (s_window && (s_window->flags & SDL_WINDOW_INPUT_FOCUS)) return s_window;
    return NULL;
}

SDL_Window *SDL_GetMouseFocus(void) {
    if (s_window && (s_window->flags & SDL_WINDOW_MOUSE_FOCUS)) return s_window;
    return NULL;
}

static const int k_modes[][2] = {
    { 3840, 2160 }, { 2560, 1440 }, { 1920, 1080 }, { 1600, 900 },
    { 1366, 768 },  { 1280, 720 },  { 1024, 768 },  { 800, 600 }, { 640, 480 },
};

static void mode_fill(SDL_DisplayMode *mode, int w, int h) {
    mode->format       = SDL_PIXELFORMAT_RGB888;
    mode->w            = w;
    mode->h            = h;
    mode->refresh_rate = 60;
    mode->driverdata   = NULL;
}

static int modes_build(SDL_DisplayMode *out, int cap) {
    int dw, dh;
    win_desktop_size(&dw, &dh);
    int n = 0;
    mode_fill(&out[n++], dw, dh);
    for (size_t i = 0; i < sizeof(k_modes) / sizeof(*k_modes) && n < cap; i++) {
        int w = k_modes[i][0], h = k_modes[i][1];
        if (w > dw || h > dh || (w == dw && h == dh)) continue;
        mode_fill(&out[n++], w, h);
    }
    return n;
}

int SDL_GetNumVideoDisplays(void) {
    return win_init() ? 1 : 0;
}

const char *SDL_GetDisplayName(int displayIndex) {
    (void)displayIndex;
    return DOPO_DRIVER;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    (void)displayIndex;
    if (!mode || !win_init()) return -1;
    int dw, dh;
    win_desktop_size(&dw, &dh);
    mode_fill(mode, dw, dh);
    return 0;
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    return SDL_GetDesktopDisplayMode(displayIndex, mode);
}

int SDL_GetNumDisplayModes(int displayIndex) {
    (void)displayIndex;
    if (!win_init()) return -1;
    SDL_DisplayMode modes[16];
    return modes_build(modes, 16);
}

int SDL_GetDisplayMode(int displayIndex, int modeIndex, SDL_DisplayMode *mode) {
    (void)displayIndex;
    if (!mode || !win_init()) return -1;
    SDL_DisplayMode modes[16];
    int n = modes_build(modes, 16);
    if (modeIndex < 0 || modeIndex >= n) {
        shim_set_error("display mode index %d out of range", modeIndex);
        return -1;
    }
    *mode = modes[modeIndex];
    return 0;
}

SDL_DisplayMode *SDL_GetClosestDisplayMode(int displayIndex, const SDL_DisplayMode *mode, SDL_DisplayMode *closest) {
    (void)displayIndex;
    if (!mode || !closest) return NULL;
    mode_fill(closest, mode->w, mode->h);
    return closest;
}

int SDL_GetDisplayDPI(int displayIndex, float *ddpi, float *hdpi, float *vdpi) {
    (void)displayIndex;
    float dpi = 96.0f;
    if (win_display_dpi(&dpi) != 0) return -1;
    if (ddpi) *ddpi = dpi;
    if (hdpi) *hdpi = dpi;
    if (vdpi) *vdpi = dpi;
    return 0;
}

int SDL_GetDisplayBounds(int displayIndex, SDL_Rect *rect) {
    (void)displayIndex;
    if (!rect || !win_init()) return -1;
    int dw, dh;
    win_desktop_size(&dw, &dh);
    rect->x = 0;
    rect->y = 0;
    rect->w = dw;
    rect->h = dh;
    return 0;
}

int SDL_GetDisplayUsableBounds(int displayIndex, SDL_Rect *rect) {
    return SDL_GetDisplayBounds(displayIndex, rect);
}

SDL_DisplayOrientation SDL_GetDisplayOrientation(int displayIndex) {
    (void)displayIndex;
    return SDL_ORIENTATION_UNKNOWN;
}

int SDL_GetWindowDisplayMode(SDL_Window *window, SDL_DisplayMode *mode) {
    if (!window || !mode) return -1;
    mode_fill(mode, window->w, window->h);
    return 0;
}

int SDL_SetWindowDisplayMode(SDL_Window *window, const SDL_DisplayMode *mode) {
    (void)window; (void)mode;
    return 0;
}

const char *SDL_GetCurrentVideoDriver(void) {
    return win_driver();
}

int SDL_GetNumVideoDrivers(void) {
    return 1;
}

const char *SDL_GetVideoDriver(int index) {
    (void)index;
    return SDL_GetCurrentVideoDriver();
}

int SDL_VideoInit(const char *driver_name) {
    (void)driver_name;
    return win_init() ? 0 : -1;
}

void SDL_VideoQuit(void) {
    shim_video_quit();
}
