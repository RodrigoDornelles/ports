#define _GNU_SOURCE
#include <time.h>

#include "x11_shim.h"

#define QUEUE_CAP 256

static XEvent   s_queue[QUEUE_CAP];
static int      s_head;
static int      s_tail;
static unsigned s_modifiers;
static unsigned long s_serial;

void x11_events_init(void) {
    s_head = s_tail = 0;
    s_modifiers = 0;
}

void x11_event_push(const XEvent *event) {
    int next = (s_head + 1) % QUEUE_CAP;
    if (next == s_tail) return;

    s_queue[s_head] = *event;
    s_queue[s_head].xany.serial  = ++s_serial;
    s_queue[s_head].xany.display = x11_state()->display;
    s_head = next;
}

bool x11_event_pop(XEvent *out) {
    if (s_tail == s_head) return false;
    *out   = s_queue[s_tail];
    s_tail = (s_tail + 1) % QUEUE_CAP;
    return true;
}

bool x11_event_peek(XEvent *out) {
    if (s_tail == s_head) return false;
    *out = s_queue[s_tail];
    return true;
}

int x11_event_count(void) {
    return (s_head - s_tail + QUEUE_CAP) % QUEUE_CAP;
}

/* pull the first event a caller is interested in, keeping the rest in order */
bool x11_event_take(XEvent *out, bool (*match)(const XEvent *, XPointer), XPointer arg) {
    for (int i = s_tail; i != s_head; i = (i + 1) % QUEUE_CAP) {
        if (!match(&s_queue[i], arg)) continue;

        *out = s_queue[i];
        for (int from = i; from != s_tail; ) {
            int to = from;
            from = (from - 1 + QUEUE_CAP) % QUEUE_CAP;
            s_queue[to] = s_queue[from];
        }
        s_tail = (s_tail + 1) % QUEUE_CAP;
        return true;
    }
    return false;
}

void x11_event_configure(int width, int height) {
    x11_state_t *s = x11_state();

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type                 = ConfigureNotify;
    ev.xconfigure.event     = s->window;
    ev.xconfigure.window    = s->window;
    ev.xconfigure.width     = width;
    ev.xconfigure.height    = height;
    x11_event_push(&ev);
}

void x11_event_focus(bool gained) {
    x11_state_t *s = x11_state();

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type          = gained ? FocusIn : FocusOut;
    ev.xfocus.window = s->window;
    ev.xfocus.mode   = NotifyNormal;
    ev.xfocus.detail = NotifyAncestor;
    x11_event_push(&ev);
}

static unsigned modifier_bit(KeySym keysym) {
    switch (keysym) {
        case XK_Shift_L:
        case XK_Shift_R:   return ShiftMask;
        case XK_Control_L:
        case XK_Control_R: return ControlMask;
        case XK_Alt_L:
        case XK_Alt_R:     return Mod1Mask;
        case XK_Super_L:
        case XK_Super_R:   return Mod4Mask;
        default:           return 0;
    }
}

void shim_events_key(uint16_t scancode, uint32_t keycode, bool pressed) {
    (void)keycode;
    x11_state_t *s = x11_state();
    if (scancode > X11_MAX_KEYCODE - X11_KEYCODE_OFFSET) return;

    unsigned kc = (unsigned)scancode + X11_KEYCODE_OFFSET;
    KeySym   ks = x11_keysym_for(kc, 0);
    if (ks == NoSymbol) return;

    unsigned bit = modifier_bit(ks);

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type             = pressed ? KeyPress : KeyRelease;
    ev.xkey.window      = s->window;
    ev.xkey.root        = s->root;
    ev.xkey.subwindow   = None;
    ev.xkey.time        = (Time)shim_ticks_ms();
    ev.xkey.state       = s_modifiers;
    ev.xkey.keycode     = kc;
    ev.xkey.same_screen = True;
    x11_event_push(&ev);

    if (bit) {
        if (pressed) s_modifiers |= bit;
        else         s_modifiers &= ~bit;
    }
}

void shim_joystick_input(uint8_t pad, bool pressed) {
    /* X11 has no gamepad; map pad buttons to keys with the core's key options */
    (void)pad; (void)pressed;
}

void shim_events_quit_request(void) {
    x11_state_t *s = x11_state();

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type                 = ClientMessage;
    ev.xclient.window       = s->window;
    ev.xclient.message_type = s->wm_protocols;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = (long)s->wm_delete;
    ev.xclient.data.l[1]    = (long)shim_ticks_ms();
    x11_event_push(&ev);
}

void x11_pump(void) {
    shim_ipc_pump();
}

int XPending(Display *display) {
    (void)display;
    x11_pump();
    return x11_event_count();
}

int XEventsQueued(Display *display, int mode) {
    (void)mode;
    return XPending(display);
}

int XNextEvent(Display *display, XEvent *event_return) {
    for (;;) {
        x11_pump();
        if (x11_event_pop(event_return)) return 0;

        struct timespec nap = { 0, 1000000L };
        nanosleep(&nap, NULL);
    }
    (void)display;
}

int XPeekEvent(Display *display, XEvent *event_return) {
    for (;;) {
        x11_pump();
        if (x11_event_peek(event_return)) return 0;

        struct timespec nap = { 0, 1000000L };
        nanosleep(&nap, NULL);
    }
    (void)display;
}

typedef struct {
    Display *display;
    Bool   (*predicate)(Display *, XEvent *, XPointer);
    XPointer arg;
} predicate_t;

static bool predicate_match(const XEvent *event, XPointer arg) {
    predicate_t *want = (predicate_t *)arg;
    XEvent       copy = *event;
    return want->predicate(want->display, &copy, want->arg) == True;
}

Bool XCheckIfEvent(Display *display, XEvent *event_return,
                   Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg) {
    x11_pump();
    if (!predicate) return False;

    predicate_t want = { display, predicate, arg };
    return x11_event_take(event_return, predicate_match, (XPointer)&want) ? True : False;
}

typedef struct {
    Window window;
    int    type;
} typed_t;

static bool typed_match(const XEvent *event, XPointer arg) {
    typed_t *want = (typed_t *)arg;
    return event->type == want->type && event->xany.window == want->window;
}

Bool XCheckTypedWindowEvent(Display *display, Window w, int event_type, XEvent *event_return) {
    (void)display;
    x11_pump();

    typed_t want = { w, event_type };
    return x11_event_take(event_return, typed_match, (XPointer)&want) ? True : False;
}

Bool XCheckTypedEvent(Display *display, int event_type, XEvent *event_return) {
    return XCheckTypedWindowEvent(display, x11_state()->window, event_type, event_return);
}

Status XSendEvent(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send) {
    (void)display; (void)propagate; (void)event_mask;
    if (!event_send) return 0;

    /* messages aimed at the root are window manager requests; there is none */
    if (w == x11_state()->window) x11_event_push(event_send);
    return 1;
}

Bool XFilterEvent(XEvent *event, Window w) {
    (void)event; (void)w;
    return False;
}

int XSelectInput(Display *display, Window w, long event_mask) {
    (void)display; (void)w;
    x11_state()->event_mask = event_mask;
    return 0;
}

int XSync(Display *display, Bool discard) {
    (void)display;
    if (discard) {
        XEvent drop;
        while (x11_event_pop(&drop)) {}
        return 0;
    }
    x11_pump();
    return 0;
}
