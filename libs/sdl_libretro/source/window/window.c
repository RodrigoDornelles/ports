#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shim.h"
#include "window.h"
#include "capture.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef void         *XDisplay;
typedef unsigned long XWindow;
typedef unsigned long XAtom;
typedef unsigned long XVisualID;
typedef unsigned long XKeySym;
typedef unsigned long XCursor;
typedef unsigned long XPixmap;

typedef struct {
    unsigned long  pixel;
    unsigned short red, green, blue;
    char           flags;
    char           pad;
} XColorRaw;

typedef union {
    int  type;
    long pad[24];
} XEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow event; XWindow window;
    int x, y; int width, height; int border_width;
    XWindow above; int override_redirect;
} XConfigureEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow window; XAtom message_type; int format;
    union { char b[20]; short s[10]; long l[5]; } data;
} XClientMessageEventRaw;

typedef struct {
    int type; unsigned long serial; int send_event; XDisplay display;
    XWindow window; XWindow root; XWindow subwindow; unsigned long time;
    int x, y; int x_root, y_root; unsigned int state; unsigned int keycode; int same_screen;
} XKeyEventRaw;

typedef struct {
    long flags; int input; int initial_state;
    unsigned long icon_pixmap; XWindow icon_window; int icon_x, icon_y;
    unsigned long icon_mask; unsigned long window_group;
} XWMHintsRaw;

#define X_KeyPress          2
#define X_KeyRelease        3
#define X_FocusIn           9
#define X_FocusOut          10
#define X_Expose            12
#define X_UnmapNotify       18
#define X_MapNotify         19
#define X_ConfigureNotify   22
#define X_ClientMessage     33

#define X_KeyPressMask            (1L << 0)
#define X_KeyReleaseMask          (1L << 1)
#define X_ExposureMask            (1L << 15)
#define X_StructureNotifyMask     (1L << 17)
#define X_SubstructureNotifyMask  (1L << 19)
#define X_SubstructureRedirectMask (1L << 20)
#define X_FocusChangeMask         (1L << 21)

#define X_InputHint        (1L << 0)
#define X_StateHint        (1L << 1)
#define X_NormalState      1
#define X_PropModeReplace  0
#define X_XA_ATOM          4UL
#define X_NET_WM_STATE_REMOVE 0
#define X_None             0UL
#define X_CurrentTime      0UL
#define X_GrabModeAsync    1
#define X_GrabSuccess      0
#define X_PointerMotionMask (1L << 6)
#define X_ButtonPressMask   (1L << 2)
#define X_ButtonReleaseMask (1L << 3)
#define X_NET_WM_STATE_ADD    1

#define X_FOREACH(X) \
    X(XDisplay,      XOpenDisplay,        (const char *)) \
    X(int,           XCloseDisplay,       (XDisplay)) \
    X(int,           XDefaultScreen,      (XDisplay)) \
    X(XWindow,       XDefaultRootWindow,  (XDisplay)) \
    X(void *,        XDefaultVisual,      (XDisplay, int)) \
    X(XVisualID,     XVisualIDFromVisual, (void *)) \
    X(unsigned long, XBlackPixel,         (XDisplay, int)) \
    X(int,           XDisplayWidth,       (XDisplay, int)) \
    X(int,           XDisplayHeight,      (XDisplay, int)) \
    X(int,           XDisplayWidthMM,     (XDisplay, int)) \
    X(XWindow,       XCreateSimpleWindow, (XDisplay, XWindow, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long)) \
    X(int,           XDestroyWindow,      (XDisplay, XWindow)) \
    X(int,           XMapRaised,          (XDisplay, XWindow)) \
    X(int,           XUnmapWindow,        (XDisplay, XWindow)) \
    X(int,           XRaiseWindow,        (XDisplay, XWindow)) \
    X(int,           XStoreName,          (XDisplay, XWindow, const char *)) \
    X(int,           XSelectInput,        (XDisplay, XWindow, long)) \
    X(int,           XFlush,              (XDisplay)) \
    X(int,           XSync,               (XDisplay, int)) \
    X(int,           XPending,            (XDisplay)) \
    X(int,           XNextEvent,          (XDisplay, XEventRaw *)) \
    X(XAtom,         XInternAtom,         (XDisplay, const char *, int)) \
    X(int,           XSetWMProtocols,     (XDisplay, XWindow, XAtom *, int)) \
    X(int,           XSetWMHints,         (XDisplay, XWindow, XWMHintsRaw *)) \
    X(int,           XResizeWindow,       (XDisplay, XWindow, unsigned, unsigned)) \
    X(int,           XMoveWindow,         (XDisplay, XWindow, int, int)) \
    X(int,           XSendEvent,          (XDisplay, XWindow, int, long, XEventRaw *)) \
    X(int,           XChangeProperty,     (XDisplay, XWindow, XAtom, XAtom, int, int, const unsigned char *, int)) \
    X(XKeySym,       XLookupKeysym,       (XKeyEventRaw *, int)) \
    X(XCursor,       XCreateFontCursor,   (XDisplay, unsigned)) \
    X(XCursor,       XCreatePixmapCursor, (XDisplay, XPixmap, XPixmap, XColorRaw *, XColorRaw *, unsigned, unsigned)) \
    X(XPixmap,       XCreateBitmapFromData, (XDisplay, XWindow, const char *, unsigned, unsigned)) \
    X(int,           XFreePixmap,         (XDisplay, XPixmap)) \
    X(int,           XDefineCursor,       (XDisplay, XWindow, XCursor)) \
    X(int,           XUndefineCursor,     (XDisplay, XWindow)) \
    X(int,           XFreeCursor,         (XDisplay, XCursor)) \
    X(int,           XGrabPointer,        (XDisplay, XWindow, int, unsigned, int, int, XWindow, XCursor, unsigned long)) \
    X(int,           XUngrabPointer,      (XDisplay, unsigned long))

#define X_DECL(ret, name, args) static ret (*p_##name) args;
X_FOREACH(X_DECL)
#undef X_DECL

typedef EGLDisplay (*PFN_eglGetPlatformDisplay_t)(EGLenum, void *, const EGLAttrib *);
typedef EGLDisplay (*PFN_eglGetPlatformDisplayEXT_t)(EGLenum, void *, const EGLint *);

#define EGL_FOREACH(X) \
    X(EGLDisplay,   eglGetDisplay,          (EGLNativeDisplayType)) \
    X(EGLBoolean,   eglInitialize,          (EGLDisplay, EGLint *, EGLint *)) \
    X(EGLBoolean,   eglTerminate,           (EGLDisplay)) \
    X(EGLBoolean,   eglChooseConfig,        (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *)) \
    X(EGLBoolean,   eglGetConfigAttrib,     (EGLDisplay, EGLConfig, EGLint, EGLint *)) \
    X(EGLBoolean,   eglBindAPI,             (EGLenum)) \
    X(EGLSurface,   eglCreateWindowSurface, (EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *)) \
    X(EGLBoolean,   eglDestroySurface,      (EGLDisplay, EGLSurface)) \
    X(EGLContext,   eglCreateContext,       (EGLDisplay, EGLConfig, EGLContext, const EGLint *)) \
    X(EGLBoolean,   eglDestroyContext,      (EGLDisplay, EGLContext)) \
    X(EGLBoolean,   eglMakeCurrent,         (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
    X(EGLBoolean,   eglSwapBuffers,         (EGLDisplay, EGLSurface)) \
    X(EGLBoolean,   eglSwapInterval,        (EGLDisplay, EGLint)) \
    X(EGLint,       eglGetError,            (void)) \
    X(const char *, eglQueryString,         (EGLDisplay, EGLint)) \
    X(EGLBoolean,   eglQuerySurface,        (EGLDisplay, EGLSurface, EGLint, EGLint *)) \
    X(void *,       eglGetProcAddress,      (const char *))

#define EGL_DECL(ret, name, args) static ret (*p_##name) args;
EGL_FOREACH(EGL_DECL)
#undef EGL_DECL

struct win_cursor {
    XCursor xcursor;
    bool    owned;
};

struct win_window {
    int        x, y, w, h;
    XWindow    xwin;
    EGLConfig  config;
    EGLSurface surface;
    bool       mapped;
    bool       opengl;
};

static struct {
    bool        ready;
    bool        failed;
    bool        egl_ready;
    bool        egl_failed;
    bool        has_x;
    bool        x11_keys;
    bool        create_context_ext;
    bool        attrs_set;
    bool        sync_after_swap;
    void      (*gl_finish)(void);
    bool        synthetic_focus;
    bool        cursor_shown;
    bool        grabbed;
    void       *lib_xcursor;
    XCursor     invisible;
    win_cursor *cursor;
    win_cursor  default_cursor;
    void       *lib_x11;
    void       *lib_egl;
    void       *lib_gl;
    XDisplay    dpy;
    int         screen;
    XAtom       wm_delete;
    XAtom       net_wm_state;
    XAtom       net_wm_fullscreen;
    EGLDisplay  edpy;
    EGLContext  current;
    win_window *window;
    int         swap_interval;
    int         attrs[WIN_GL_ATTR_COUNT];
} v = { .swap_interval = 1, .cursor_shown = true };

static void cursor_apply(void);

static void attrs_reset(void) {
    memset(v.attrs, 0, sizeof(v.attrs));
    v.attrs[WIN_GL_RED_SIZE]              = 8;
    v.attrs[WIN_GL_GREEN_SIZE]            = 8;
    v.attrs[WIN_GL_BLUE_SIZE]             = 8;
    v.attrs[WIN_GL_ALPHA_SIZE]            = 0;
    v.attrs[WIN_GL_DEPTH_SIZE]            = 16;
    v.attrs[WIN_GL_STENCIL_SIZE]          = 0;
    v.attrs[WIN_GL_DOUBLEBUFFER]          = 1;
    v.attrs[WIN_GL_CONTEXT_MAJOR_VERSION] = 2;
    v.attrs[WIN_GL_CONTEXT_MINOR_VERSION] = 1;
    v.attrs[WIN_GL_CONTEXT_PROFILE_MASK]  = 0;
}

static bool x11_load(void) {
    v.lib_x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (!v.lib_x11) return false;

    /* our own fake libX11 means there is no server to talk to, and letting two
       shims share one IPC socket would only end in a fight over it */
    if (dlsym(v.lib_x11, "x11_state")) {
        dlclose(v.lib_x11);
        v.lib_x11 = NULL;
        return false;
    }

#define X_LOAD(ret, name, args) \
    p_##name = (ret (*) args)dlsym(v.lib_x11, #name); \
    if (!p_##name) { shim_set_error("libX11 missing %s", #name); return false; }
    X_FOREACH(X_LOAD)
#undef X_LOAD
    return true;
}

static bool egl_load(void) {
    v.lib_egl = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!v.lib_egl) v.lib_egl = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!v.lib_egl) {
        shim_set_error("libEGL not available: %s", dlerror());
        return false;
    }
#define EGL_LOAD(ret, name, args) \
    p_##name = (ret (*) args)dlsym(v.lib_egl, #name); \
    if (!p_##name) { shim_set_error("libEGL missing %s", #name); return false; }
    EGL_FOREACH(EGL_LOAD)
#undef EGL_LOAD
    return true;
}

static EGLDisplay egl_display_open(void) {
    if (v.has_x) {
        PFN_eglGetPlatformDisplay_t get = (PFN_eglGetPlatformDisplay_t)dlsym(v.lib_egl, "eglGetPlatformDisplay");
        if (get) {
            EGLDisplay d = get(EGL_PLATFORM_X11_KHR, v.dpy, NULL);
            if (d != EGL_NO_DISPLAY) return d;
        }
        PFN_eglGetPlatformDisplayEXT_t get_ext = (PFN_eglGetPlatformDisplayEXT_t)p_eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (get_ext) {
            EGLDisplay d = get_ext(EGL_PLATFORM_X11_KHR, v.dpy, NULL);
            if (d != EGL_NO_DISPLAY) return d;
        }
        return p_eglGetDisplay((EGLNativeDisplayType)v.dpy);
    }
    return p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

bool win_init(void) {
    if (v.ready) return true;
    if (v.failed) return false;

    if (!v.attrs_set) {
        attrs_reset();
        v.attrs_set = true;
    }

    v.sync_after_swap = getenv(DOPO_ENV_SYNC) != NULL;

    const char *native = getenv(DOPO_ENV_NATIVE);
    const char *keys   = getenv(DOPO_ENV_X11KEYS);
    bool want_x = native ? (strcmp(native, "x11") == 0) : (getenv("DISPLAY") != NULL);
    v.x11_keys = (keys && keys[0] == '1') || getenv(DOPO_IPC_ENV_SOCKET) == NULL;
    v.synthetic_focus = !v.x11_keys;

    if (want_x && x11_load()) {
        v.dpy = p_XOpenDisplay(NULL);
        if (v.dpy) {
            v.has_x             = true;
            v.screen            = p_XDefaultScreen(v.dpy);
            v.wm_delete         = p_XInternAtom(v.dpy, "WM_DELETE_WINDOW", 0);
            v.net_wm_state      = p_XInternAtom(v.dpy, "_NET_WM_STATE", 0);
            v.net_wm_fullscreen = p_XInternAtom(v.dpy, "_NET_WM_STATE_FULLSCREEN", 0);
        }
    }

    fprintf(stderr, SHIM_TAG " video ready: %s\n", v.has_x ? "x11" : "native");
    v.ready = true;
    return true;
}

/* EGL only gets loaded once something actually asks for OpenGL, so a software
   only frontend still runs on a device without any GL driver installed */
bool win_gl_init(void) {
    if (v.egl_ready) return true;
    if (v.egl_failed || !win_init()) return false;

    if (!egl_load()) {
        v.egl_failed = true;
        return false;
    }

    v.edpy = egl_display_open();
    if (v.edpy == EGL_NO_DISPLAY) {
        shim_set_error("eglGetDisplay failed (0x%x)", p_eglGetError());
        v.egl_failed = true;
        return false;
    }
    EGLint major = 0, minor = 0;
    if (!p_eglInitialize(v.edpy, &major, &minor)) {
        shim_set_error("eglInitialize failed (0x%x)", p_eglGetError());
        v.egl_failed = true;
        return false;
    }

    const char *exts = p_eglQueryString(v.edpy, EGL_EXTENSIONS);
    v.create_context_ext = (major > 1 || (major == 1 && minor >= 5))
                        || (exts && strstr(exts, "EGL_KHR_create_context"));

    fprintf(stderr, SHIM_TAG " EGL %d.%d ready, %s\n", major, minor,
            p_eglQueryString(v.edpy, EGL_VENDOR));
    v.egl_ready = true;
    return true;
}

bool win_has_x(void) {
    return v.has_x;
}

bool win_synthetic_focus(void) {
    return v.synthetic_focus;
}

const char *win_driver(void) {
    return v.has_x ? "x11" : DOPO_DRIVER;
}

void win_desktop_size(int *w, int *h) {
    if (v.has_x) {
        *w = p_XDisplayWidth(v.dpy, v.screen);
        *h = p_XDisplayHeight(v.dpy, v.screen);
        return;
    }
    const char *ew = getenv(DOPO_ENV_WIDTH);
    const char *eh = getenv(DOPO_ENV_HEIGHT);
    *w = ew ? atoi(ew) : 1920;
    *h = eh ? atoi(eh) : 1080;
    if (*w <= 0) *w = 1920;
    if (*h <= 0) *h = 1080;
}

int win_display_dpi(float *dpi) {
    if (!win_init()) return -1;
    float value = 96.0f;
    if (v.has_x) {
        int mm = p_XDisplayWidthMM(v.dpy, v.screen);
        int px = p_XDisplayWidth(v.dpy, v.screen);
        if (mm > 0 && px > 0) value = (float)px / ((float)mm / 25.4f);
    }
    if (dpi) *dpi = value;
    return 0;
}

/* SDL2 scancodes, the wire format the core speaks */
#define KEY_A          4
#define KEY_1         30
#define KEY_0         39
#define KEY_RETURN    40
#define KEY_ESCAPE    41
#define KEY_BACKSPACE 42
#define KEY_TAB       43
#define KEY_SPACE     44
#define KEY_F1        58
#define KEY_HOME      74
#define KEY_PAGEUP    75
#define KEY_DELETE    76
#define KEY_END       77
#define KEY_PAGEDOWN  78
#define KEY_RIGHT     79
#define KEY_LEFT      80
#define KEY_DOWN      81
#define KEY_UP        82
#define KEY_INSERT    73
#define KEY_LCTRL    224
#define KEY_LSHIFT   225
#define KEY_LALT     226
#define KEY_RCTRL    228
#define KEY_RSHIFT   229
#define KEY_RALT     230

/* SDL2 keycodes for the keys above the ASCII range */
#define SYM_SCANCODE_MASK (1 << 30)
#define SYM(scancode)     ((uint32_t)(scancode) | SYM_SCANCODE_MASK)

static bool keysym_translate(XKeySym ks, uint16_t *scancode, uint32_t *keycode) {
    if (ks >= 'a' && ks <= 'z') { *scancode = (uint16_t)(KEY_A + (ks - 'a')); *keycode = (uint32_t)ks; return true; }
    if (ks >= 'A' && ks <= 'Z') { *scancode = (uint16_t)(KEY_A + (ks - 'A')); *keycode = (uint32_t)(ks + 32); return true; }
    if (ks >= '1' && ks <= '9') { *scancode = (uint16_t)(KEY_1 + (ks - '1')); *keycode = (uint32_t)ks; return true; }
    if (ks == '0')              { *scancode = KEY_0; *keycode = '0'; return true; }
    if (ks >= 0xffbe && ks <= 0xffc9) {
        *scancode = (uint16_t)(KEY_F1 + (ks - 0xffbe));
        *keycode  = SYM(KEY_F1 + (ks - 0xffbe));
        return true;
    }
    switch (ks) {
        case 0x0020: *scancode = KEY_SPACE;     *keycode = ' ';                 return true;
        case 0xff0d: *scancode = KEY_RETURN;    *keycode = '\r';                return true;
        case 0xff1b: *scancode = KEY_ESCAPE;    *keycode = '\033';              return true;
        case 0xff08: *scancode = KEY_BACKSPACE; *keycode = '\b';                return true;
        case 0xff09: *scancode = KEY_TAB;       *keycode = '\t';                return true;
        case 0xff52: *scancode = KEY_UP;        *keycode = SYM(KEY_UP);         return true;
        case 0xff54: *scancode = KEY_DOWN;      *keycode = SYM(KEY_DOWN);       return true;
        case 0xff51: *scancode = KEY_LEFT;      *keycode = SYM(KEY_LEFT);       return true;
        case 0xff53: *scancode = KEY_RIGHT;     *keycode = SYM(KEY_RIGHT);      return true;
        case 0xff50: *scancode = KEY_HOME;      *keycode = SYM(KEY_HOME);       return true;
        case 0xff57: *scancode = KEY_END;       *keycode = SYM(KEY_END);        return true;
        case 0xff55: *scancode = KEY_PAGEUP;    *keycode = SYM(KEY_PAGEUP);     return true;
        case 0xff56: *scancode = KEY_PAGEDOWN;  *keycode = SYM(KEY_PAGEDOWN);   return true;
        case 0xff63: *scancode = KEY_INSERT;    *keycode = SYM(KEY_INSERT);     return true;
        case 0xffff: *scancode = KEY_DELETE;    *keycode = '\177';              return true;
        case 0xffe1: *scancode = KEY_LSHIFT;    *keycode = SYM(KEY_LSHIFT);     return true;
        case 0xffe2: *scancode = KEY_RSHIFT;    *keycode = SYM(KEY_RSHIFT);     return true;
        case 0xffe3: *scancode = KEY_LCTRL;     *keycode = SYM(KEY_LCTRL);      return true;
        case 0xffe4: *scancode = KEY_RCTRL;     *keycode = SYM(KEY_RCTRL);      return true;
        case 0xffe9: *scancode = KEY_LALT;      *keycode = SYM(KEY_LALT);       return true;
        case 0xffea: *scancode = KEY_RALT;      *keycode = SYM(KEY_RALT);       return true;
        default:     return false;
    }
}

void win_pump(void) {
    if (!v.has_x || !v.window) return;
    win_window *win = v.window;

    while (p_XPending(v.dpy) > 0) {
        XEventRaw ev;
        p_XNextEvent(v.dpy, &ev);
        switch (ev.type) {
            case X_ClientMessage: {
                XClientMessageEventRaw *cm = (XClientMessageEventRaw *)&ev;
                if ((XAtom)cm->data.l[0] == v.wm_delete) win_on_close();
                break;
            }
            case X_ConfigureNotify: {
                XConfigureEventRaw *ce = (XConfigureEventRaw *)&ev;
                if (ce->width != win->w || ce->height != win->h) {
                    win->w = ce->width;
                    win->h = ce->height;
                    shim_ipc_send(DOPO_IPC_PKT_WINDOW, 0, (uint16_t)win->w, (uint32_t)win->h);
                    capture_resize(win->w, win->h);
                    win_on_resize(win->w, win->h);
                }
                if (!ce->send_event && (ce->x != win->x || ce->y != win->y)) {
                    win->x = ce->x;
                    win->y = ce->y;
                    win_on_move(win->x, win->y);
                }
                break;
            }
            case X_Expose:
                win_on_expose();
                break;
            case X_FocusIn:
                if (!v.synthetic_focus) win_on_focus(true);
                break;
            case X_FocusOut:
                if (!v.synthetic_focus) win_on_focus(false);
                break;
            case X_MapNotify:
                win->mapped = true;
                win_on_map(true);
                break;
            case X_UnmapNotify:
                win->mapped = false;
                win_on_map(false);
                break;
            case X_KeyPress:
            case X_KeyRelease: {
                if (!v.x11_keys) break;
                XKeyEventRaw *ke = (XKeyEventRaw *)&ev;
                uint16_t scancode;
                uint32_t keycode;
                if (keysym_translate(p_XLookupKeysym(ke, 0), &scancode, &keycode)) {
                    shim_events_key(scancode, keycode, ev.type == X_KeyPress);
                }
                break;
            }
            default:
                break;
        }
    }
}

int win_gl_set_attribute(win_gl_attr_t attr, int value) {
    if (!v.attrs_set) {
        attrs_reset();
        v.attrs_set = true;
    }
    if ((int)attr < 0 || (int)attr >= WIN_GL_ATTR_COUNT) {
        shim_set_error("unknown GL attribute %d", (int)attr);
        return -1;
    }
    v.attrs[attr] = value;
    return 0;
}

int win_gl_get_attribute(win_gl_attr_t attr, int *value) {
    if ((int)attr < 0 || (int)attr >= WIN_GL_ATTR_COUNT || !value) return -1;
    *value = v.attrs[attr];
    if (v.egl_ready && v.window && v.window->config) {
        EGLint q = 0;
        switch (attr) {
            case WIN_GL_RED_SIZE:     p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_RED_SIZE,     &q); *value = q; break;
            case WIN_GL_GREEN_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_GREEN_SIZE,   &q); *value = q; break;
            case WIN_GL_BLUE_SIZE:    p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_BLUE_SIZE,    &q); *value = q; break;
            case WIN_GL_ALPHA_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_ALPHA_SIZE,   &q); *value = q; break;
            case WIN_GL_DEPTH_SIZE:   p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_DEPTH_SIZE,   &q); *value = q; break;
            case WIN_GL_STENCIL_SIZE: p_eglGetConfigAttrib(v.edpy, v.window->config, EGL_STENCIL_SIZE, &q); *value = q; break;
            default: break;
        }
    }
    return 0;
}

void win_gl_reset_attributes(void) {
    attrs_reset();
    v.attrs_set = true;
}

static bool is_es(void) {
    return (v.attrs[WIN_GL_CONTEXT_PROFILE_MASK] & WIN_GL_PROFILE_ES) != 0;
}

static EGLConfig config_choose(bool strict) {
    EGLint attribs[32];
    int    n = 0;

    attribs[n++] = EGL_RENDERABLE_TYPE;
    attribs[n++] = is_es() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT;
    attribs[n++] = EGL_SURFACE_TYPE;
    attribs[n++] = EGL_WINDOW_BIT;
    attribs[n++] = EGL_RED_SIZE;
    attribs[n++] = v.attrs[WIN_GL_RED_SIZE];
    attribs[n++] = EGL_GREEN_SIZE;
    attribs[n++] = v.attrs[WIN_GL_GREEN_SIZE];
    attribs[n++] = EGL_BLUE_SIZE;
    attribs[n++] = v.attrs[WIN_GL_BLUE_SIZE];
    if (strict) {
        attribs[n++] = EGL_ALPHA_SIZE;
        attribs[n++] = v.attrs[WIN_GL_ALPHA_SIZE];
        attribs[n++] = EGL_DEPTH_SIZE;
        attribs[n++] = v.attrs[WIN_GL_DEPTH_SIZE];
        attribs[n++] = EGL_STENCIL_SIZE;
        attribs[n++] = v.attrs[WIN_GL_STENCIL_SIZE];
        if (v.attrs[WIN_GL_MULTISAMPLEBUFFERS]) {
            attribs[n++] = EGL_SAMPLE_BUFFERS;
            attribs[n++] = 1;
            attribs[n++] = EGL_SAMPLES;
            attribs[n++] = v.attrs[WIN_GL_MULTISAMPLESAMPLES];
        }
    } else {
        attribs[n++] = EGL_DEPTH_SIZE;
        attribs[n++] = 1;
    }
    attribs[n++] = EGL_NONE;

    EGLConfig configs[64];
    EGLint    count = 0;
    if (!p_eglChooseConfig(v.edpy, attribs, configs, 64, &count) || count < 1) return NULL;

    if (v.has_x) {
        XVisualID want = p_XVisualIDFromVisual(p_XDefaultVisual(v.dpy, v.screen));
        for (EGLint i = 0; i < count; i++) {
            EGLint vid = 0;
            if (p_eglGetConfigAttrib(v.edpy, configs[i], EGL_NATIVE_VISUAL_ID, &vid) && (XVisualID)vid == want) {
                return configs[i];
            }
        }
    }
    return configs[0];
}

static bool surface_create(win_window *win) {
    win->config = config_choose(true);
    if (!win->config) win->config = config_choose(false);
    if (!win->config) {
        shim_set_error("no EGL config matches requested attributes");
        return false;
    }
    if (!p_eglBindAPI(is_es() ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
        shim_set_error("eglBindAPI failed (0x%x)", p_eglGetError());
        return false;
    }
    EGLNativeWindowType native = v.has_x ? (EGLNativeWindowType)win->xwin : (EGLNativeWindowType)0;
    win->surface = p_eglCreateWindowSurface(v.edpy, win->config, native, NULL);
    if (win->surface == EGL_NO_SURFACE) {
        shim_set_error("eglCreateWindowSurface failed (0x%x)", p_eglGetError());
        return false;
    }
    return true;
}

static void surface_destroy(win_window *win) {
    if (!v.egl_ready || win->surface == EGL_NO_SURFACE || !win->surface) return;
    p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    p_eglDestroySurface(v.edpy, win->surface);
    win->surface = EGL_NO_SURFACE;
}

void *win_gl_create_context(win_window *window) {
    if (!v.egl_ready || !window || !window->surface) {
        shim_set_error("window has no OpenGL surface");
        return NULL;
    }

    EGLint attribs[16];
    int    n     = 0;
    int    major = v.attrs[WIN_GL_CONTEXT_MAJOR_VERSION];
    int    minor = v.attrs[WIN_GL_CONTEXT_MINOR_VERSION];
    int    flags = v.attrs[WIN_GL_CONTEXT_FLAGS];

    if (is_es()) {
        attribs[n++] = EGL_CONTEXT_CLIENT_VERSION;
        attribs[n++] = major > 0 ? major : 2;
    } else if (v.create_context_ext) {
        attribs[n++] = EGL_CONTEXT_MAJOR_VERSION;
        attribs[n++] = major;
        attribs[n++] = EGL_CONTEXT_MINOR_VERSION;
        attribs[n++] = minor;
        if (major >= 3) {
            attribs[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
            attribs[n++] = (v.attrs[WIN_GL_CONTEXT_PROFILE_MASK] & WIN_GL_PROFILE_CORE)
                         ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
                         : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
        }
        if (flags & WIN_GL_CONTEXT_DEBUG) {
            attribs[n++] = EGL_CONTEXT_OPENGL_DEBUG;
            attribs[n++] = EGL_TRUE;
        }
        if (flags & WIN_GL_CONTEXT_FORWARD_COMPATIBLE) {
            attribs[n++] = EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE;
            attribs[n++] = EGL_TRUE;
        }
    }
    attribs[n++] = EGL_NONE;

    EGLContext share = v.attrs[WIN_GL_SHARE_WITH_CURRENT_CONTEXT] && v.current ? v.current : EGL_NO_CONTEXT;
    EGLContext ctx   = p_eglCreateContext(v.edpy, window->config, share, attribs);
    if (ctx == EGL_NO_CONTEXT) {
        shim_set_error("eglCreateContext %d.%d failed (0x%x)", major, minor, p_eglGetError());
        return NULL;
    }
    if (!p_eglMakeCurrent(v.edpy, window->surface, window->surface, ctx)) {
        shim_set_error("eglMakeCurrent failed (0x%x)", p_eglGetError());
        p_eglDestroyContext(v.edpy, ctx);
        return NULL;
    }
    v.current = ctx;

    if (!v.lib_gl) {
        v.lib_gl = dlopen(is_es() ? "libGLESv2.so.2" : "libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    }
    if (v.sync_after_swap && !v.gl_finish) {
        v.gl_finish = (void (*)(void))win_gl_proc_address("glFinish");
        fprintf(stderr, SHIM_TAG " sync after swap %s\n", v.gl_finish ? "on" : "unavailable");
    }
    fprintf(stderr, SHIM_TAG " GL context %d.%d created (%s)\n", major, minor, is_es() ? "GLES" : "GL");
    return (void *)ctx;
}

int win_gl_make_current(win_window *window, void *context) {
    if (!v.egl_ready) return -1;
    EGLSurface surf = (window && window->surface) ? window->surface : EGL_NO_SURFACE;
    if (!p_eglMakeCurrent(v.edpy, surf, surf, (EGLContext)context)) {
        shim_set_error("eglMakeCurrent failed (0x%x)", p_eglGetError());
        return -1;
    }
    v.current = (EGLContext)context;
    return 0;
}

void win_gl_delete_context(void *context) {
    if (!v.egl_ready || !context) return;
    if (v.current == (EGLContext)context) {
        p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        v.current = EGL_NO_CONTEXT;
    }
    p_eglDestroyContext(v.edpy, (EGLContext)context);
}

void *win_gl_current_context(void) {
    return (void *)v.current;
}

void win_gl_swap(win_window *window) {
    if (!v.egl_ready || !window || !window->surface) return;
    capture_frame();
    p_eglSwapBuffers(v.edpy, window->surface);

    if (v.sync_after_swap && v.gl_finish) v.gl_finish();
}

int win_gl_set_swap_interval(int interval) {
    if (!v.egl_ready) return -1;
    if (interval < 0) interval = 1;
    if (!p_eglSwapInterval(v.edpy, interval)) {
        fprintf(stderr, SHIM_TAG " eglSwapInterval(%d) failed (0x%x); frames may queue up\n",
                interval, p_eglGetError());
        shim_set_error("eglSwapInterval failed (0x%x)", p_eglGetError());
        return -1;
    }
    fprintf(stderr, SHIM_TAG " swap interval %d\n", interval);
    v.swap_interval = interval;
    return 0;
}

int win_gl_get_swap_interval(void) {
    return v.swap_interval;
}

void *win_gl_proc_address(const char *proc) {
    if (!proc) return NULL;
    void *p = NULL;
    if (p_eglGetProcAddress) p = p_eglGetProcAddress(proc);

    if (!p) p = dlsym(RTLD_DEFAULT, proc);
    if (!p) {
        static const char *const libs[] = {
            "libGLESv2.so.2", "libGLESv2.so", "libGL.so.1", "libGL.so",
        };
        for (size_t i = 0; !p && i < sizeof(libs) / sizeof(*libs); i++) {
            if (!v.lib_gl) v.lib_gl = dlopen(libs[i], RTLD_LAZY | RTLD_GLOBAL);
            if (v.lib_gl) p = dlsym(v.lib_gl, proc);
            if (!p && v.lib_gl) {
                dlclose(v.lib_gl);
                v.lib_gl = NULL;
            }
        }
    }
    return p;
}

void win_gl_drawable_size(win_window *window, int *w, int *h) {
    if (!window) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    EGLint sw = window->w, sh = window->h;
    if (v.egl_ready && window->surface) {
        p_eglQuerySurface(v.edpy, window->surface, EGL_WIDTH,  &sw);
        p_eglQuerySurface(v.edpy, window->surface, EGL_HEIGHT, &sh);
    }
    if (w) *w = sw;
    if (h) *h = sh;
}

static void x11_fullscreen(win_window *win, bool enable) {
    if (!v.has_x || !win->xwin) return;
    if (!win->mapped) {
        if (enable) {
            p_XChangeProperty(v.dpy, win->xwin, v.net_wm_state, X_XA_ATOM, 32, X_PropModeReplace,
                              (const unsigned char *)&v.net_wm_fullscreen, 1);
        }
        return;
    }
    XEventRaw ev;
    memset(&ev, 0, sizeof(ev));
    XClientMessageEventRaw *cm = (XClientMessageEventRaw *)&ev;
    cm->type         = X_ClientMessage;
    cm->display      = v.dpy;
    cm->window       = win->xwin;
    cm->message_type = v.net_wm_state;
    cm->format       = 32;
    cm->data.l[0]    = enable ? X_NET_WM_STATE_ADD : X_NET_WM_STATE_REMOVE;
    cm->data.l[1]    = (long)v.net_wm_fullscreen;
    cm->data.l[2]    = 0;
    cm->data.l[3]    = 1;
    p_XSendEvent(v.dpy, p_XDefaultRootWindow(v.dpy), 0,
                 X_SubstructureRedirectMask | X_SubstructureNotifyMask, &ev);
    p_XFlush(v.dpy);
}

win_window *win_create(const win_config_t *cfg) {
    if (!cfg || !win_init()) return NULL;
    if (v.window) {
        shim_set_error("only one window is supported");
        return NULL;
    }

    win_window *win = calloc(1, sizeof(*win));
    if (!win) {
        shim_set_error("out of memory");
        return NULL;
    }

    win->x      = cfg->x;
    win->y      = cfg->y;
    win->w      = cfg->w;
    win->h      = cfg->h;
    win->opengl = cfg->opengl;

    if (v.has_x) {
        XWindow root  = p_XDefaultRootWindow(v.dpy);
        unsigned long black = p_XBlackPixel(v.dpy, v.screen);
        win->xwin = p_XCreateSimpleWindow(v.dpy, root, win->x, win->y,
                                          (unsigned)win->w, (unsigned)win->h, 0, black, black);
        if (!win->xwin) {
            shim_set_error("XCreateSimpleWindow failed");
            free(win);
            return NULL;
        }
        p_XStoreName(v.dpy, win->xwin, cfg->title ? cfg->title : "");
        p_XSelectInput(v.dpy, win->xwin, X_StructureNotifyMask | X_ExposureMask | X_FocusChangeMask
                                         | X_KeyPressMask | X_KeyReleaseMask);
        p_XSetWMProtocols(v.dpy, win->xwin, &v.wm_delete, 1);

        XWMHintsRaw hints;
        memset(&hints, 0, sizeof(hints));
        hints.flags         = X_InputHint | X_StateHint;
        hints.input         = v.x11_keys ? 1 : 0;
        hints.initial_state = X_NormalState;
        p_XSetWMHints(v.dpy, win->xwin, &hints);

        if (cfg->fullscreen) x11_fullscreen(win, true);
        if (!cfg->hidden) {
            p_XMapRaised(v.dpy, win->xwin);
            win->mapped = true;
        }
        p_XSync(v.dpy, 0);
    }

    if (cfg->opengl && (!win_gl_init() || !surface_create(win))) {
        if (win->xwin) p_XDestroyWindow(v.dpy, win->xwin);
        free(win);
        return NULL;
    }

    v.window = win;
    if (!v.cursor) v.cursor = &v.default_cursor;
    cursor_apply();

    shim_ipc_send(DOPO_IPC_PKT_WINDOW, 0, (uint16_t)win->w, (uint32_t)win->h);
    capture_resize(win->w, win->h);
    fprintf(stderr, SHIM_TAG " window %dx%d '%s'%s\n", win->w, win->h,
            cfg->title ? cfg->title : "", cfg->opengl ? " opengl" : "");
    return win;
}

void win_destroy(win_window *win) {
    if (!win) return;
    surface_destroy(win);
    if (v.has_x && win->xwin) {
        p_XDestroyWindow(v.dpy, win->xwin);
        p_XFlush(v.dpy);
    }
    if (v.window == win) v.window = NULL;
    free(win);
}

void win_quit(void) {
    if (v.window) win_destroy(v.window);
    if (v.has_x && v.invisible) p_XFreeCursor(v.dpy, v.invisible);
    v.invisible = X_None;
    v.cursor    = NULL;
    if (v.egl_ready && v.edpy != EGL_NO_DISPLAY) {
        p_eglMakeCurrent(v.edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        p_eglTerminate(v.edpy);
    }
    if (v.has_x && v.dpy) p_XCloseDisplay(v.dpy);
    v.edpy    = EGL_NO_DISPLAY;
    v.current = EGL_NO_CONTEXT;
    v.dpy     = NULL;
    v.has_x      = false;
    v.ready      = false;
    v.failed     = false;
    v.egl_ready  = false;
    v.egl_failed = false;
}

void win_geometry(const win_window *win, int *x, int *y, int *w, int *h) {
    if (x) *x = win ? win->x : 0;
    if (y) *y = win ? win->y : 0;
    if (w) *w = win ? win->w : 0;
    if (h) *h = win ? win->h : 0;
}

void win_set_title(win_window *win, const char *title) {
    if (!win || !v.has_x || !win->xwin) return;
    p_XStoreName(v.dpy, win->xwin, title ? title : "");
    p_XFlush(v.dpy);
}

void win_set_size(win_window *win, int w, int h) {
    if (!win || w <= 0 || h <= 0) return;
    win->w = w;
    win->h = h;
    if (v.has_x && win->xwin) {
        p_XResizeWindow(v.dpy, win->xwin, (unsigned)w, (unsigned)h);
        p_XFlush(v.dpy);
    }
}

void win_set_position(win_window *win, int x, int y) {
    if (!win) return;
    win->x = x;
    win->y = y;
    if (v.has_x && win->xwin) {
        p_XMoveWindow(v.dpy, win->xwin, x, y);
        p_XFlush(v.dpy);
    }
}

void win_set_fullscreen(win_window *win, bool enable) {
    if (!win) return;
    x11_fullscreen(win, enable);
    if (!v.has_x && enable) {
        int dw, dh;
        win_desktop_size(&dw, &dh);
        win->w = dw;
        win->h = dh;
    }
}

void win_set_visible(win_window *win, bool visible) {
    if (!win || !v.has_x || !win->xwin) return;
    if (visible) p_XMapRaised(v.dpy, win->xwin);
    else         p_XUnmapWindow(v.dpy, win->xwin);
    p_XFlush(v.dpy);
}

void win_raise(win_window *win) {
    if (!win || !v.has_x || !win->xwin) return;
    p_XRaiseWindow(v.dpy, win->xwin);
    p_XFlush(v.dpy);
}

void win_set_grab(win_window *win, bool grabbed) {
    if (!win) return;
    v.grabbed = grabbed;
    if (!v.has_x || !win->xwin || !v.x11_keys) return;
    if (grabbed) {
        p_XGrabPointer(v.dpy, win->xwin, 1,
                       X_PointerMotionMask | X_ButtonPressMask | X_ButtonReleaseMask,
                       X_GrabModeAsync, X_GrabModeAsync, win->xwin, X_None, X_CurrentTime);
    } else {
        p_XUngrabPointer(v.dpy, X_CurrentTime);
    }
    p_XFlush(v.dpy);
}

typedef struct {
    unsigned int version, size, width, height, xhot, yhot, delay;
    unsigned int *pixels;
} XcursorImageRaw;

static struct {
    XcursorImageRaw *(*create)(int, int);
    XCursor          (*load)(XDisplay, const XcursorImageRaw *);
    void             (*destroy)(XcursorImageRaw *);
} xcursor;

static bool xcursor_bind(void) {
    if (xcursor.create) return true;
    if (!v.lib_xcursor) v.lib_xcursor = dlopen("libXcursor.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!v.lib_xcursor) return false;
    xcursor.create  = (typeof(xcursor.create)) dlsym(v.lib_xcursor, "XcursorImageCreate");
    xcursor.load    = (typeof(xcursor.load))   dlsym(v.lib_xcursor, "XcursorImageLoadCursor");
    xcursor.destroy = (typeof(xcursor.destroy))dlsym(v.lib_xcursor, "XcursorImageDestroy");
    return xcursor.create && xcursor.load && xcursor.destroy;
}

static const unsigned k_cursor_fonts[WIN_CURSOR_COUNT] = {
    [WIN_CURSOR_ARROW]     = 68,
    [WIN_CURSOR_IBEAM]     = 152,
    [WIN_CURSOR_WAIT]      = 150,
    [WIN_CURSOR_CROSSHAIR] = 34,
    [WIN_CURSOR_SIZENWSE]  = 134,
    [WIN_CURSOR_SIZENESW]  = 136,
    [WIN_CURSOR_SIZEWE]    = 108,
    [WIN_CURSOR_SIZENS]    = 116,
    [WIN_CURSOR_SIZEALL]   = 52,
    [WIN_CURSOR_NO]        = 88,
    [WIN_CURSOR_HAND]      = 60,
};

static XCursor cursor_invisible(void) {
    if (v.invisible || !v.has_x) return v.invisible;
    static const char blank[8] = {0};
    XPixmap   pixmap = p_XCreateBitmapFromData(v.dpy, p_XDefaultRootWindow(v.dpy), blank, 8, 8);
    XColorRaw black  = {0};
    v.invisible = p_XCreatePixmapCursor(v.dpy, pixmap, pixmap, &black, &black, 0, 0);
    p_XFreePixmap(v.dpy, pixmap);
    return v.invisible;
}

static void cursor_apply(void) {
    if (!v.has_x || !v.window || !v.window->xwin) return;
    if (!v.cursor_shown) {
        p_XDefineCursor(v.dpy, v.window->xwin, cursor_invisible());
    } else if (v.cursor && v.cursor->xcursor) {
        p_XDefineCursor(v.dpy, v.window->xwin, v.cursor->xcursor);
    } else {
        p_XUndefineCursor(v.dpy, v.window->xwin);
    }
    p_XFlush(v.dpy);
}

win_cursor *win_cursor_system(win_cursor_shape_t shape) {
    if (!win_init()) return NULL;
    win_cursor *cursor = calloc(1, sizeof(*cursor));
    if (!cursor) {
        shim_set_error("out of memory");
        return NULL;
    }
    if (v.has_x && shape >= 0 && shape < WIN_CURSOR_COUNT) {
        cursor->xcursor = p_XCreateFontCursor(v.dpy, k_cursor_fonts[shape]);
        cursor->owned   = cursor->xcursor != X_None;
    }
    return cursor;
}

win_cursor *win_cursor_color(const uint32_t *argb, int w, int h, int hot_x, int hot_y) {
    if (!win_init()) return NULL;
    win_cursor *cursor = calloc(1, sizeof(*cursor));
    if (!cursor) {
        shim_set_error("out of memory");
        return NULL;
    }
    if (!v.has_x || !argb || w <= 0 || h <= 0 || !xcursor_bind()) return cursor;

    XcursorImageRaw *image = xcursor.create(w, h);
    if (!image) return cursor;
    image->xhot = (unsigned)(hot_x < 0 ? 0 : hot_x);
    image->yhot = (unsigned)(hot_y < 0 ? 0 : hot_y);
    memcpy(image->pixels, argb, (size_t)w * (size_t)h * sizeof(uint32_t));

    cursor->xcursor = xcursor.load(v.dpy, image);
    cursor->owned   = cursor->xcursor != X_None;
    xcursor.destroy(image);
    return cursor;
}

void win_cursor_free(win_cursor *cursor) {
    if (!cursor || cursor == &v.default_cursor) return;
    if (v.cursor == cursor) {
        v.cursor = &v.default_cursor;
        cursor_apply();
    }
    if (cursor->owned && v.has_x) p_XFreeCursor(v.dpy, cursor->xcursor);
    free(cursor);
}

void win_cursor_set(win_cursor *cursor) {
    if (cursor) v.cursor = cursor;
    cursor_apply();
}

win_cursor *win_cursor_default(void) {
    return &v.default_cursor;
}

win_cursor *win_cursor_current(void) {
    return v.cursor ? v.cursor : &v.default_cursor;
}

void win_cursor_show(bool shown) {
    v.cursor_shown = shown;
    cursor_apply();
}

bool win_cursor_shown(void) {
    return v.cursor_shown;
}
