#define _GNU_SOURCE
#include "x11_shim.h"

/*
 * A fake Xlib. There is no X server on these devices; the game gets a display
 * that reports one screen, hands out a single window, and takes its input from
 * the core over IPC.
 *
 * Nothing is drawn through here. The game holds its own EGL context and puts
 * frames on the panel itself, so there is no swap for the core to hook and no
 * capture either: an X11 game always runs the way sdl_format=egl describes.
 */

#define ATOM_MAX   128
#define ATOM_FIRST 1000

static struct {
    x11_state_t state;
    bool        ready;

    union {
        Display display;
        char    padding[4096];  /* Xlib keeps private fields past the public part */
    } dpy;

    Screen       screen;
    Visual       visual;
    Depth        depth;
    ScreenFormat format;
    char         vendor[32];
    char         display_name[32];

    x11_native_window_t native;

    char *atoms[ATOM_MAX];
    int   atom_count;
} x;

x11_state_t *x11_state(void) {
    return &x.state;
}

static void screen_size(int *w, int *h) {
    const char *ew = getenv(DOPO_ENV_WIDTH);
    const char *eh = getenv(DOPO_ENV_HEIGHT);
    *w = ew ? atoi(ew) : 1920;
    *h = eh ? atoi(eh) : 1080;
    if (*w <= 0) *w = 1920;
    if (*h <= 0) *h = 1080;
}

static Atom atom_intern(const char *name, Bool only_if_exists) {
    for (int i = 0; i < x.atom_count; i++) {
        if (strcmp(x.atoms[i], name) == 0) return (Atom)(ATOM_FIRST + i);
    }
    if (only_if_exists || x.atom_count >= ATOM_MAX) return None;

    x.atoms[x.atom_count] = strdup(name);
    if (!x.atoms[x.atom_count]) return None;
    return (Atom)(ATOM_FIRST + x.atom_count++);
}

Display *XOpenDisplay(const char *display_name) {
    if (x.ready) return &x.dpy.display;

    int width, height;
    screen_size(&width, &height);

    snprintf(x.vendor, sizeof(x.vendor), "%s", DOPO_NAME);
    snprintf(x.display_name, sizeof(x.display_name), "%s",
             display_name && display_name[0] ? display_name : ":0");

    x.visual.ext_data     = NULL;
    x.visual.visualid     = 0x21;
    x.visual.class        = TrueColor;
    x.visual.red_mask     = 0x00FF0000;
    x.visual.green_mask   = 0x0000FF00;
    x.visual.blue_mask    = 0x000000FF;
    x.visual.bits_per_rgb = 8;
    x.visual.map_entries  = 256;

    x.depth.depth      = 24;
    x.depth.nvisuals   = 1;
    x.depth.visuals    = &x.visual;

    x.format.depth          = 24;
    x.format.bits_per_pixel = 32;
    x.format.scanline_pad   = 32;

    x.screen.display     = &x.dpy.display;
    x.screen.root        = X11_ROOT_ID;
    x.screen.width       = width;
    x.screen.height      = height;
    x.screen.mwidth      = width  * 254 / 960;   /* about 96 dpi */
    x.screen.mheight     = height * 254 / 960;
    x.screen.ndepths     = 1;
    x.screen.depths      = &x.depth;
    x.screen.root_depth  = 24;
    x.screen.root_visual = &x.visual;
    x.screen.default_gc  = NULL;
    x.screen.cmap        = 0x20;
    x.screen.white_pixel = 0x00FFFFFF;
    x.screen.black_pixel = 0x00000000;
    x.screen.max_maps    = 1;
    x.screen.min_maps    = 1;
    x.screen.backing_store = NotUseful;
    x.screen.save_unders   = False;
    x.screen.root_input_mask = 0;

    Display *d = &x.dpy.display;
    d->fd                  = -1;
    d->proto_major_version = 11;
    d->proto_minor_version = 0;
    d->vendor              = x.vendor;
    d->byte_order          = LSBFirst;
    d->bitmap_unit         = 32;
    d->bitmap_pad          = 32;
    d->bitmap_bit_order    = LSBFirst;
    d->nformats            = 1;
    d->pixmap_format       = &x.format;
    d->release             = 12101000;
    d->qlen                = 0;
    d->max_request_size    = 65535;
    d->display_name        = x.display_name;
    d->default_screen      = 0;
    d->nscreens            = 1;
    d->screens             = &x.screen;
    d->motion_buffer       = 0;
    d->min_keycode         = X11_MIN_KEYCODE;
    d->max_keycode         = X11_MAX_KEYCODE;
    d->xdefaults           = NULL;

    x.state.display = d;
    x.state.root    = X11_ROOT_ID;
    x.state.width   = width;
    x.state.height  = height;
    x.ready         = true;

    x11_events_init();
    shim_ipc_connect();

    const char *format = getenv(DOPO_ENV_FORMAT);
    if (format && strcmp(format, "egl") != 0) {
        fprintf(stderr, SHIM_TAG " %s=%s ignored, an X11 game presents through the"
                        " real driver and the core never sees its frames\n",
                DOPO_ENV_FORMAT, format);
    }

    x.state.wm_protocols = atom_intern("WM_PROTOCOLS", False);
    x.state.wm_delete    = atom_intern("WM_DELETE_WINDOW", False);

    fprintf(stderr, SHIM_TAG " display %s ready, screen %dx%d\n",
            x.display_name, width, height);
    return d;
}

int XCloseDisplay(Display *display) {
    (void)display;
    shim_ipc_close();
    for (int i = 0; i < x.atom_count; i++) free(x.atoms[i]);
    x.atom_count = 0;
    x.ready      = false;
    return 0;
}

char *XDisplayName(const char *string) {
    if (string && string[0]) return (char *)string;
    return x.display_name[0] ? x.display_name : (char *)":0";
}

int XDisplayWidth(Display *display, int screen_number) {
    (void)display; (void)screen_number;
    return x.state.width;
}

int XDisplayHeight(Display *display, int screen_number) {
    (void)display; (void)screen_number;
    return x.state.height;
}

int XFlush(Display *display) {
    (void)display;
    x11_pump();
    return 0;
}

int XFree(void *data) {
    free(data);
    return 1;
}

Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    (void)display;
    if (!atom_name) return None;
    return atom_intern(atom_name, only_if_exists);
}

char *XGetAtomName(Display *display, Atom atom) {
    (void)display;
    int index = (int)atom - ATOM_FIRST;
    if (index < 0 || index >= x.atom_count) return NULL;
    return strdup(x.atoms[index]);
}

Window XCreateWindow(Display *display, Window parent, int x_pos, int y_pos,
                     unsigned int width, unsigned int height, unsigned int border_width,
                     int depth, unsigned int class, Visual *visual,
                     unsigned long valuemask, XSetWindowAttributes *attributes) {
    (void)display; (void)parent; (void)x_pos; (void)y_pos; (void)border_width;
    (void)depth; (void)class; (void)visual;

    if (x.state.window) {
        shim_set_error("only one window is supported");
        return None;
    }
    if (width == 0)  width  = (unsigned)x.state.width;
    if (height == 0) height = (unsigned)x.state.height;

    x.native.width  = (unsigned short)width;
    x.native.height = (unsigned short)height;

    const char *want = getenv(DOPO_ENV_NATIVE);
    bool        none = want && strcmp(want, "null") == 0;

    x.state.window = none ? (Window)1 : (Window)(uintptr_t)&x.native;
    x.state.width  = (int)width;
    x.state.height = (int)height;
    if (valuemask & CWEventMask) x.state.event_mask = attributes->event_mask;

    shim_ipc_send(DOPO_IPC_PKT_WINDOW, 0, (uint16_t)width, (uint32_t)height);

    fprintf(stderr, SHIM_TAG " window %ux%u, native handle %s\n", width, height,
            none ? "none" : "fbdev");
    return x.state.window;
}

Window XCreateSimpleWindow(Display *display, Window parent, int x_pos, int y_pos,
                           unsigned int width, unsigned int height, unsigned int border_width,
                           unsigned long border, unsigned long background) {
    (void)border; (void)background;
    return XCreateWindow(display, parent, x_pos, y_pos, width, height, border_width,
                         24, InputOutput, &x.visual, 0, NULL);
}

int XDestroyWindow(Display *display, Window w) {
    (void)display;
    if (w == x.state.window) {
        x.state.window = None;
        x.state.mapped = false;
    }
    return 0;
}

static void window_map(bool mapped) {
    if (x.state.mapped == mapped) return;
    x.state.mapped = mapped;

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type            = mapped ? MapNotify : UnmapNotify;
    ev.xmap.display    = x.state.display;
    ev.xmap.event      = x.state.window;
    ev.xmap.window     = x.state.window;
    x11_event_push(&ev);

    if (mapped) {
        x11_event_configure(x.state.width, x.state.height);
        x11_event_focus(true);

        memset(&ev, 0, sizeof(ev));
        ev.type             = Expose;
        ev.xexpose.display  = x.state.display;
        ev.xexpose.window   = x.state.window;
        ev.xexpose.width    = x.state.width;
        ev.xexpose.height   = x.state.height;
        x11_event_push(&ev);
    }
}

int XMapWindow(Display *display, Window w) {
    (void)display; (void)w;
    window_map(true);
    return 0;
}

int XMapRaised(Display *display, Window w) {
    return XMapWindow(display, w);
}

int XUnmapWindow(Display *display, Window w) {
    (void)display; (void)w;
    window_map(false);
    return 0;
}

int XRaiseWindow(Display *display, Window w) {
    (void)display; (void)w;
    return 0;
}

int XIconifyWindow(Display *display, Window w, int screen_number) {
    (void)display; (void)w; (void)screen_number;
    return 1;
}

int XMoveWindow(Display *display, Window w, int x_pos, int y_pos) {
    (void)display; (void)w; (void)x_pos; (void)y_pos;
    return 0;
}

int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height) {
    (void)display; (void)w;
    if (width == 0 || height == 0) return 0;
    if ((int)width == x.state.width && (int)height == x.state.height) return 0;

    x.state.width   = (int)width;
    x.state.height  = (int)height;
    x.native.width  = (unsigned short)width;
    x.native.height = (unsigned short)height;
    shim_ipc_send(DOPO_IPC_PKT_WINDOW, 0, (uint16_t)width, (uint32_t)height);
    x11_event_configure((int)width, (int)height);
    return 0;
}

int XMoveResizeWindow(Display *display, Window w, int x_pos, int y_pos,
                      unsigned int width, unsigned int height) {
    (void)x_pos; (void)y_pos;
    return XResizeWindow(display, w, width, height);
}

Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                    int *x_return, int *y_return, unsigned int *width_return,
                    unsigned int *height_return, unsigned int *border_width_return,
                    unsigned int *depth_return) {
    (void)display; (void)d;
    if (root_return)         *root_return         = x.state.root;
    if (x_return)            *x_return            = 0;
    if (y_return)            *y_return            = 0;
    if (width_return)        *width_return        = (unsigned)x.state.width;
    if (height_return)       *height_return       = (unsigned)x.state.height;
    if (border_width_return) *border_width_return = 0;
    if (depth_return)        *depth_return        = 24;
    return 1;
}

Status XGetWindowAttributes(Display *display, Window w, XWindowAttributes *window_attributes_return) {
    (void)display; (void)w;
    if (!window_attributes_return) return 0;

    memset(window_attributes_return, 0, sizeof(*window_attributes_return));
    window_attributes_return->x             = 0;
    window_attributes_return->y             = 0;
    window_attributes_return->width         = x.state.width;
    window_attributes_return->height        = x.state.height;
    window_attributes_return->depth         = 24;
    window_attributes_return->visual        = &x.visual;
    window_attributes_return->root          = x.state.root;
    window_attributes_return->class         = InputOutput;
    window_attributes_return->colormap      = x.screen.cmap;
    window_attributes_return->map_state     = x.state.mapped ? IsViewable : IsUnmapped;
    window_attributes_return->your_event_mask = x.state.event_mask;
    window_attributes_return->all_event_masks = x.state.event_mask;
    window_attributes_return->screen        = &x.screen;
    return 1;
}

Status XQueryTree(Display *display, Window w, Window *root_return, Window *parent_return,
                  Window **children_return, unsigned int *nchildren_return) {
    (void)display; (void)w;
    if (root_return)      *root_return      = x.state.root;
    if (parent_return)    *parent_return    = x.state.root;
    if (children_return)  *children_return  = NULL;
    if (nchildren_return) *nchildren_return = 0;
    return 1;
}

int XChangeProperty(Display *display, Window w, Atom property, Atom type, int format,
                    int mode, const unsigned char *data, int nelements) {
    (void)display; (void)w; (void)property; (void)type; (void)format;
    (void)mode; (void)data; (void)nelements;
    return 0;
}

int XGetWindowProperty(Display *display, Window w, Atom property, long long_offset,
                       long long_length, Bool delete, Atom req_type, Atom *actual_type_return,
                       int *actual_format_return, unsigned long *nitems_return,
                       unsigned long *bytes_after_return, unsigned char **prop_return) {
    (void)display; (void)w; (void)property; (void)long_offset; (void)long_length;
    (void)delete; (void)req_type;
    if (actual_type_return)   *actual_type_return   = None;
    if (actual_format_return) *actual_format_return = 0;
    if (nitems_return)        *nitems_return        = 0;
    if (bytes_after_return)   *bytes_after_return   = 0;
    if (prop_return)          *prop_return          = NULL;
    return Success;
}

Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count) {
    (void)display; (void)w;
    for (int i = 0; i < count; i++) {
        if (protocols[i] == x.state.wm_delete) return 1;
    }
    return 1;
}

Colormap XCreateColormap(Display *display, Window w, Visual *visual, int alloc) {
    (void)display; (void)w; (void)visual; (void)alloc;
    return x.screen.cmap;
}

XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask, XVisualInfo *vinfo_template,
                            int *nitems_return) {
    (void)display; (void)vinfo_mask; (void)vinfo_template;

    XVisualInfo *info = calloc(1, sizeof(*info));
    if (!info) {
        if (nitems_return) *nitems_return = 0;
        return NULL;
    }
    info->visual        = &x.visual;
    info->visualid      = x.visual.visualid;
    info->screen        = 0;
    info->depth         = 24;
    info->class         = TrueColor;
    info->red_mask      = x.visual.red_mask;
    info->green_mask    = x.visual.green_mask;
    info->blue_mask     = x.visual.blue_mask;
    info->colormap_size = 256;
    info->bits_per_rgb  = 8;

    if (nitems_return) *nitems_return = 1;
    return info;
}

int XSetInputFocus(Display *display, Window focus, int revert_to, Time time) {
    (void)display; (void)focus; (void)revert_to; (void)time;
    return 0;
}

int XGrabPointer(Display *display, Window grab_window, Bool owner_events, unsigned int event_mask,
                 int pointer_mode, int keyboard_mode, Window confine_to, Cursor cursor, Time time) {
    (void)display; (void)grab_window; (void)owner_events; (void)event_mask;
    (void)pointer_mode; (void)keyboard_mode; (void)confine_to; (void)cursor; (void)time;
    return GrabSuccess;
}

int XUngrabPointer(Display *display, Time time) {
    (void)display; (void)time;
    return 0;
}

int XGrabKeyboard(Display *display, Window grab_window, Bool owner_events,
                  int pointer_mode, int keyboard_mode, Time time) {
    (void)display; (void)grab_window; (void)owner_events;
    (void)pointer_mode; (void)keyboard_mode; (void)time;
    return GrabSuccess;
}

int XUngrabKeyboard(Display *display, Time time) {
    (void)display; (void)time;
    return 0;
}

int XWarpPointer(Display *display, Window src_w, Window dest_w, int src_x, int src_y,
                 unsigned int src_width, unsigned int src_height, int dest_x, int dest_y) {
    (void)display; (void)src_w; (void)dest_w; (void)src_x; (void)src_y;
    (void)src_width; (void)src_height; (void)dest_x; (void)dest_y;
    return 0;
}

Bool XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return,
                   int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return,
                   unsigned int *mask_return) {
    (void)display; (void)w;
    if (root_return)   *root_return   = x.state.root;
    if (child_return)  *child_return  = None;
    if (root_x_return) *root_x_return = 0;
    if (root_y_return) *root_y_return = 0;
    if (win_x_return)  *win_x_return  = 0;
    if (win_y_return)  *win_y_return  = 0;
    if (mask_return)   *mask_return   = 0;
    return True;
}

int XResetScreenSaver(Display *display) {
    (void)display;
    return 0;
}

Window XGetSelectionOwner(Display *display, Atom selection) {
    (void)display; (void)selection;
    return None;
}

int XSetSelectionOwner(Display *display, Atom selection, Window owner, Time time) {
    (void)display; (void)selection; (void)owner; (void)time;
    return 0;
}

int XConvertSelection(Display *display, Atom selection, Atom target, Atom property,
                      Window requestor, Time time) {
    (void)display; (void)time;

    /* nothing owns a selection here, so answer straight away with a refusal */
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type                  = SelectionNotify;
    ev.xselection.display    = x.state.display;
    ev.xselection.requestor  = requestor;
    ev.xselection.selection  = selection;
    ev.xselection.target     = target;
    ev.xselection.property   = None;
    (void)property;
    x11_event_push(&ev);
    return 0;
}

static int error_handler_default(Display *display, XErrorEvent *event) {
    (void)display; (void)event;
    return 0;
}

static XErrorHandler s_error_handler = error_handler_default;

XErrorHandler XSetErrorHandler(XErrorHandler handler) {
    XErrorHandler previous = s_error_handler;
    s_error_handler = handler ? handler : error_handler_default;
    return previous;
}

int XGetErrorText(Display *display, int code, char *buffer_return, int length) {
    (void)display;
    if (buffer_return && length > 0) snprintf(buffer_return, (size_t)length, "error %d", code);
    return 0;
}

int XGetErrorDatabaseText(Display *display, const char *name, const char *message,
                          const char *default_string, char *buffer_return, int length) {
    (void)display; (void)name; (void)message;
    if (buffer_return && length > 0) {
        snprintf(buffer_return, (size_t)length, "%s", default_string ? default_string : "");
    }
    return 0;
}

XSizeHints *XAllocSizeHints(void) {
    return calloc(1, sizeof(XSizeHints));
}

void XSetWMNormalHints(Display *display, Window w, XSizeHints *hints) {
    (void)display; (void)w; (void)hints;
}

Status XGetWMNormalHints(Display *display, Window w, XSizeHints *hints_return, long *supplied_return) {
    (void)display; (void)w;
    if (hints_return) memset(hints_return, 0, sizeof(*hints_return));
    if (supplied_return) *supplied_return = 0;
    return 1;
}

XClassHint *XAllocClassHint(void) {
    return calloc(1, sizeof(XClassHint));
}

int XSetClassHint(Display *display, Window w, XClassHint *class_hints) {
    (void)display; (void)w; (void)class_hints;
    return 0;
}

void XSetWMName(Display *display, Window w, XTextProperty *text_prop) {
    (void)display; (void)w;
    if (text_prop && text_prop->value) {
        fprintf(stderr, SHIM_TAG " title: %s\n", (const char *)text_prop->value);
    }
}

void XSetWMIconName(Display *display, Window w, XTextProperty *text_prop) {
    (void)display; (void)w; (void)text_prop;
}

int XStoreName(Display *display, Window w, const char *window_name) {
    (void)display; (void)w;
    if (window_name) fprintf(stderr, SHIM_TAG " title: %s\n", window_name);
    return 0;
}
