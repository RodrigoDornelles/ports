#define _GNU_SOURCE
#include <wchar.h>

#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "x11_shim.h"

/* wire scancode (SDL 2 numbering) to keysym, unshifted and shifted */
typedef struct {
    unsigned char scancode;
    KeySym        plain;
    KeySym        shifted;
} keymap_t;

static const keymap_t k_keys[] = {
    { 40, XK_Return,       XK_Return       },
    { 41, XK_Escape,       XK_Escape       },
    { 42, XK_BackSpace,    XK_BackSpace    },
    { 43, XK_Tab,          XK_Tab          },
    { 44, XK_space,        XK_space        },
    { 45, XK_minus,        XK_underscore   },
    { 46, XK_equal,        XK_plus         },
    { 47, XK_bracketleft,  XK_braceleft    },
    { 48, XK_bracketright, XK_braceright   },
    { 49, XK_backslash,    XK_bar          },
    { 51, XK_semicolon,    XK_colon        },
    { 52, XK_apostrophe,   XK_quotedbl     },
    { 53, XK_grave,        XK_asciitilde   },
    { 54, XK_comma,        XK_less         },
    { 55, XK_period,       XK_greater      },
    { 56, XK_slash,        XK_question     },
    { 57, XK_Caps_Lock,    XK_Caps_Lock    },
    { 73, XK_Insert,       XK_Insert       },
    { 74, XK_Home,         XK_Home         },
    { 75, XK_Prior,        XK_Prior        },
    { 76, XK_Delete,       XK_Delete       },
    { 77, XK_End,          XK_End          },
    { 78, XK_Next,         XK_Next         },
    { 79, XK_Right,        XK_Right        },
    { 80, XK_Left,         XK_Left         },
    { 81, XK_Down,         XK_Down         },
    { 82, XK_Up,           XK_Up           },
    { 83, XK_Num_Lock,     XK_Num_Lock     },
    { 84, XK_KP_Divide,    XK_KP_Divide    },
    { 85, XK_KP_Multiply,  XK_KP_Multiply  },
    { 86, XK_KP_Subtract,  XK_KP_Subtract  },
    { 87, XK_KP_Add,       XK_KP_Add       },
    { 88, XK_KP_Enter,     XK_KP_Enter     },
    { 99, XK_KP_Decimal,   XK_KP_Decimal   },
    { 224, XK_Control_L,   XK_Control_L    },
    { 225, XK_Shift_L,     XK_Shift_L      },
    { 226, XK_Alt_L,       XK_Alt_L        },
    { 227, XK_Super_L,     XK_Super_L      },
    { 228, XK_Control_R,   XK_Control_R    },
    { 229, XK_Shift_R,     XK_Shift_R      },
    { 230, XK_Alt_R,       XK_Alt_R        },
    { 231, XK_Super_R,     XK_Super_R      },
};

static const KeySym k_digit_shifted[] = {
    XK_parenright, XK_exclam, XK_at, XK_numbersign, XK_dollar,
    XK_percent, XK_asciicircum, XK_ampersand, XK_asterisk, XK_parenleft,
};

KeySym x11_keysym_for(unsigned keycode, int shifted) {
    if (keycode < X11_KEYCODE_OFFSET) return NoSymbol;
    unsigned scancode = keycode - X11_KEYCODE_OFFSET;

    if (scancode >= 4 && scancode <= 29) {
        return (shifted ? XK_A : XK_a) + (KeySym)(scancode - 4);
    }
    if (scancode >= 30 && scancode <= 38) {
        return shifted ? k_digit_shifted[scancode - 29] : XK_1 + (KeySym)(scancode - 30);
    }
    if (scancode == 39) {
        return shifted ? k_digit_shifted[0] : XK_0;
    }
    if (scancode >= 58 && scancode <= 69) {
        return XK_F1 + (KeySym)(scancode - 58);
    }
    if (scancode >= 89 && scancode <= 97) {
        return XK_KP_1 + (KeySym)(scancode - 89);
    }
    if (scancode == 98) return XK_KP_0;

    for (size_t i = 0; i < sizeof(k_keys) / sizeof(*k_keys); i++) {
        if (k_keys[i].scancode != scancode) continue;
        return shifted ? k_keys[i].shifted : k_keys[i].plain;
    }
    return NoSymbol;
}

int x11_keysym_text(KeySym keysym, char *out, int cap) {
    if (!out || cap <= 0) return 0;

    char c = 0;
    if (keysym >= 0x20 && keysym <= 0x7e) {
        c = (char)keysym;
    } else if (keysym >= XK_KP_0 && keysym <= XK_KP_9) {
        c = (char)('0' + (keysym - XK_KP_0));
    } else {
        switch (keysym) {
            case XK_Return:
            case XK_KP_Enter:    c = '\r';   break;
            case XK_Tab:         c = '\t';   break;
            case XK_BackSpace:   c = '\b';   break;
            case XK_Escape:      c = '\033'; break;
            case XK_Delete:      c = '\177'; break;
            case XK_KP_Decimal:  c = '.';    break;
            case XK_KP_Divide:   c = '/';    break;
            case XK_KP_Multiply: c = '*';    break;
            case XK_KP_Subtract: c = '-';    break;
            case XK_KP_Add:      c = '+';    break;
            default: return 0;
        }
    }
    out[0] = c;
    return 1;
}

KeySym XkbKeycodeToKeysym(Display *display, KeyCode kc, int group, int level) {
    (void)display; (void)group;
    return x11_keysym_for(kc, level);
}

KeySym XKeycodeToKeysym(Display *display, KeyCode kc, int index) {
    return XkbKeycodeToKeysym(display, kc, 0, index);
}

KeySym XLookupKeysym(XKeyEvent *key_event, int index) {
    if (!key_event) return NoSymbol;
    return x11_keysym_for(key_event->keycode, index);
}

KeyCode XKeysymToKeycode(Display *display, KeySym keysym) {
    (void)display;
    for (unsigned kc = X11_MIN_KEYCODE; kc <= X11_MAX_KEYCODE; kc++) {
        if (x11_keysym_for(kc, 0) == keysym || x11_keysym_for(kc, 1) == keysym) {
            return (KeyCode)kc;
        }
    }
    return 0;
}

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, XComposeStatus *status_in_out) {
    (void)status_in_out;
    if (!event_struct) return 0;

    int    shifted = (event_struct->state & ShiftMask) ? 1 : 0;
    KeySym keysym  = x11_keysym_for(event_struct->keycode, shifted);
    if (keysym_return) *keysym_return = keysym;

    if (event_struct->type != KeyPress) return 0;
    return x11_keysym_text(keysym, buffer_return, bytes_buffer);
}

int XwcLookupString(XIC ic, XKeyPressedEvent *event, wchar_t *buffer_return,
                    int wchars_buffer, KeySym *keysym_return, Status *status_return) {
    (void)ic;
    char text[8];
    int  n = XLookupString((XKeyEvent *)event, text, (int)sizeof(text), keysym_return, NULL);

    if (n > 0 && buffer_return && wchars_buffer > 0) {
        buffer_return[0] = (wchar_t)(unsigned char)text[0];
        if (status_return) *status_return = XLookupBoth;
        return 1;
    }
    if (status_return) *status_return = (keysym_return && *keysym_return != NoSymbol)
                                      ? XLookupKeySym : XLookupNone;
    return 0;
}

int XRefreshKeyboardMapping(XMappingEvent *event_map) {
    (void)event_map;
    return 0;
}

XModifierKeymap *XGetModifierMapping(Display *display) {
    (void)display;
    const int per_mod = 2;

    XModifierKeymap *map = calloc(1, sizeof(*map));
    if (!map) return NULL;
    map->max_keypermod = per_mod;
    map->modifiermap   = calloc((size_t)(8 * per_mod), sizeof(KeyCode));
    if (!map->modifiermap) {
        free(map);
        return NULL;
    }

    /* slots run Shift, Lock, Control, Mod1 .. Mod5 */
    map->modifiermap[ShiftMapIndex   * per_mod]     = XKeysymToKeycode(display, XK_Shift_L);
    map->modifiermap[ShiftMapIndex   * per_mod + 1] = XKeysymToKeycode(display, XK_Shift_R);
    map->modifiermap[LockMapIndex    * per_mod]     = XKeysymToKeycode(display, XK_Caps_Lock);
    map->modifiermap[ControlMapIndex * per_mod]     = XKeysymToKeycode(display, XK_Control_L);
    map->modifiermap[ControlMapIndex * per_mod + 1] = XKeysymToKeycode(display, XK_Control_R);
    map->modifiermap[Mod1MapIndex    * per_mod]     = XKeysymToKeycode(display, XK_Alt_L);
    map->modifiermap[Mod1MapIndex    * per_mod + 1] = XKeysymToKeycode(display, XK_Alt_R);
    map->modifiermap[Mod4MapIndex    * per_mod]     = XKeysymToKeycode(display, XK_Super_L);
    map->modifiermap[Mod4MapIndex    * per_mod + 1] = XKeysymToKeycode(display, XK_Super_R);
    return map;
}

int XFreeModifiermap(XModifierKeymap *modmap) {
    if (!modmap) return 0;
    free(modmap->modifiermap);
    free(modmap);
    return 0;
}

Bool XkbSetDetectableAutoRepeat(Display *display, Bool detectable, Bool *supported_return) {
    (void)display; (void)detectable;
    if (supported_return) *supported_return = True;
    return True;
}

/*
 * Input methods. There is no compose or dead key handling to do, so the
 * context is a token the game can hold on to and hand back.
 */

static char s_im_token;
static char s_ic_token;

XIM XOpenIM(Display *display, struct _XrmHashBucketRec *db, char *res_name, char *res_class) {
    (void)display; (void)db; (void)res_name; (void)res_class;
    return (XIM)&s_im_token;
}

Status XCloseIM(XIM im) {
    (void)im;
    return 1;
}

char *XGetIMValues(XIM im, ...) {
    (void)im;
    return NULL;
}

XIC XCreateIC(XIM im, ...) {
    (void)im;
    return (XIC)&s_ic_token;
}

void XDestroyIC(XIC ic) {
    (void)ic;
}

char *XSetICValues(XIC ic, ...) {
    (void)ic;
    return NULL;
}

char *XGetICValues(XIC ic, ...) {
    (void)ic;
    return NULL;
}

void XSetICFocus(XIC ic) {
    (void)ic;
}

void XUnsetICFocus(XIC ic) {
    (void)ic;
}

XVaNestedList XVaCreateNestedList(int unused, ...) {
    (void)unused;
    return NULL;
}

Bool XSupportsLocale(void) {
    return True;
}

char *XSetLocaleModifiers(const char *modifier_list) {
    return (char *)(modifier_list ? modifier_list : "");
}

int XwcTextListToTextProperty(Display *display, wchar_t **list, int count,
                              XICCEncodingStyle style, XTextProperty *text_prop_return) {
    (void)display; (void)style;
    if (!text_prop_return) return XNoMemory;

    memset(text_prop_return, 0, sizeof(*text_prop_return));
    if (!list || count <= 0 || !list[0]) return Success;

    size_t length = wcslen(list[0]);
    char  *text   = malloc(length + 1);
    if (!text) return XNoMemory;

    for (size_t i = 0; i < length; i++) {
        text[i] = list[0][i] < 0x80 ? (char)list[0][i] : '?';
    }
    text[length] = '\0';

    text_prop_return->value    = (unsigned char *)text;
    text_prop_return->encoding = XA_STRING;
    text_prop_return->format   = 8;
    text_prop_return->nitems   = length;
    return Success;
}

void XFreeFontSet(Display *display, XFontSet fontset) {
    (void)display; (void)fontset;
}
