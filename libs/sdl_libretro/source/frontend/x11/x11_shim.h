#ifndef DOPO_X11_SHIM_H
#define DOPO_X11_SHIM_H

/* the fake display is ours to fill in, so take the public view of the struct */
#define XLIB_ILLEGAL_ACCESS 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "shim.h"

#define X11_ROOT_ID 0x00000100UL

/*
 * The game hands the Window we give it straight to eglCreateWindowSurface, and
 * with no X server the driver underneath is a native one that reads it as its
 * own window handle, not as an X resource id. So the id we hand out is a real
 * pointer to the struct those drivers expect. DOPO_X11_NATIVE=null asks for a
 * plain zero instead, for drivers that want no window at all.
 */
typedef struct {
    unsigned short width;
    unsigned short height;
} x11_native_window_t;

/* our keycodes are simply the wire scancode plus the usual evdev offset, and
   nothing outside this shim ever sees them, so they only need to agree with
   the lookup functions here */
#define X11_KEYCODE_OFFSET 8
#define X11_MIN_KEYCODE    8
#define X11_MAX_KEYCODE    255

typedef struct {
    Display *display;
    Window   window;
    Window   root;
    int      width;
    int      height;
    bool     mapped;
    long     event_mask;
    Atom     wm_delete;
    Atom     wm_protocols;
} x11_state_t;

x11_state_t *x11_state(void);
void         x11_pump(void);

/* events.c */
void   x11_events_init(void);
void   x11_event_push(const XEvent *event);
bool   x11_event_pop(XEvent *out);
bool   x11_event_peek(XEvent *out);
int    x11_event_count(void);
bool   x11_event_take(XEvent *out, bool (*match)(const XEvent *, XPointer), XPointer arg);
void   x11_event_configure(int width, int height);
void   x11_event_focus(bool gained);

/* keyboard.c */
KeySym x11_keysym_for(unsigned keycode, int shifted);
int    x11_keysym_text(KeySym keysym, char *out, int cap);

#endif
