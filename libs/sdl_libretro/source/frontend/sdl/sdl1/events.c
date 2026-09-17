#define _GNU_SOURCE
#include <pthread.h>

#include "sdl1.h"

#define QUEUE_CAP 256

/* SDL 2 scancodes, the wire format the core speaks */
#define WIRE_A          4
#define WIRE_1         30
#define WIRE_0         39
#define WIRE_F1        58
#define WIRE_KP_1      89
#define WIRE_KP_0      98

static SDL_Event       s_queue[QUEUE_CAP];
static int             s_head;
static int             s_tail;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static Uint8           s_keys[SDLK_LAST];
static SDLMod          s_mod;
static Uint8           s_state[SDL_NUMEVENTS];
static SDL_EventFilter s_filter;
static bool            s_unicode;
static int             s_repeat_delay;
static int             s_repeat_interval;
static Uint8           s_app_state = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;

void shim_events_init(void) {
    pthread_mutex_lock(&s_lock);
    s_head = s_tail = 0;
    memset(s_keys, 0, sizeof(s_keys));
    memset(s_state, SDL_ENABLE, sizeof(s_state));
    s_mod = KMOD_NONE;
    pthread_mutex_unlock(&s_lock);
}

void shim_events_quit(void) {
    shim_events_init();
    shim_joystick_quit();
}

void shim_events_push(const SDL_Event *ev) {
    pthread_mutex_lock(&s_lock);
    int next = (s_head + 1) % QUEUE_CAP;
    if (next != s_tail) {
        s_queue[s_head] = *ev;
        s_head = next;
    }
    pthread_mutex_unlock(&s_lock);
}

/* internal posting honours the event filter and the per type state */
static void events_post(const SDL_Event *ev) {
    if (ev->type < SDL_NUMEVENTS && s_state[ev->type] != SDL_ENABLE) return;
    if (s_filter && !s_filter(ev)) return;
    shim_events_push(ev);
}

static bool events_pop(SDL_Event *out) {
    bool ok = false;
    pthread_mutex_lock(&s_lock);
    if (s_tail != s_head) {
        *out   = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_CAP;
        ok     = true;
    }
    pthread_mutex_unlock(&s_lock);
    return ok;
}

static bool events_pending(void) {
    pthread_mutex_lock(&s_lock);
    bool any = s_tail != s_head;
    pthread_mutex_unlock(&s_lock);
    return any;
}

static SDLKey key_from_wire(uint16_t scancode) {
    if (scancode >= WIRE_A && scancode <= WIRE_A + 25) return (SDLKey)(SDLK_a + (scancode - WIRE_A));
    if (scancode >= WIRE_1 && scancode <= WIRE_1 + 8)  return (SDLKey)(SDLK_1 + (scancode - WIRE_1));
    if (scancode == WIRE_0)                            return SDLK_0;
    if (scancode >= WIRE_F1 && scancode <= WIRE_F1 + 11) return (SDLKey)(SDLK_F1 + (scancode - WIRE_F1));
    if (scancode >= WIRE_KP_1 && scancode <= WIRE_KP_1 + 8) return (SDLKey)(SDLK_KP1 + (scancode - WIRE_KP_1));

    switch (scancode) {
        case 40:  return SDLK_RETURN;
        case 41:  return SDLK_ESCAPE;
        case 42:  return SDLK_BACKSPACE;
        case 43:  return SDLK_TAB;
        case 44:  return SDLK_SPACE;
        case 45:  return SDLK_MINUS;
        case 46:  return SDLK_EQUALS;
        case 47:  return SDLK_LEFTBRACKET;
        case 48:  return SDLK_RIGHTBRACKET;
        case 49:  return SDLK_BACKSLASH;
        case 51:  return SDLK_SEMICOLON;
        case 52:  return SDLK_QUOTE;
        case 53:  return SDLK_BACKQUOTE;
        case 54:  return SDLK_COMMA;
        case 55:  return SDLK_PERIOD;
        case 56:  return SDLK_SLASH;
        case 57:  return SDLK_CAPSLOCK;
        case 73:  return SDLK_INSERT;
        case 74:  return SDLK_HOME;
        case 75:  return SDLK_PAGEUP;
        case 76:  return SDLK_DELETE;
        case 77:  return SDLK_END;
        case 78:  return SDLK_PAGEDOWN;
        case 79:  return SDLK_RIGHT;
        case 80:  return SDLK_LEFT;
        case 81:  return SDLK_DOWN;
        case 82:  return SDLK_UP;
        case 83:  return SDLK_NUMLOCK;
        case 84:  return SDLK_KP_DIVIDE;
        case 85:  return SDLK_KP_MULTIPLY;
        case 86:  return SDLK_KP_MINUS;
        case 87:  return SDLK_KP_PLUS;
        case 88:  return SDLK_KP_ENTER;
        case WIRE_KP_0: return SDLK_KP0;
        case 99:  return SDLK_KP_PERIOD;
        case 224: return SDLK_LCTRL;
        case 225: return SDLK_LSHIFT;
        case 226: return SDLK_LALT;
        case 227: return SDLK_LSUPER;
        case 228: return SDLK_RCTRL;
        case 229: return SDLK_RSHIFT;
        case 230: return SDLK_RALT;
        case 231: return SDLK_RSUPER;
        default:  return SDLK_UNKNOWN;
    }
}

static void mod_update(SDLKey sym, bool pressed) {
    SDLMod bit = KMOD_NONE;
    switch (sym) {
        case SDLK_LSHIFT: bit = KMOD_LSHIFT; break;
        case SDLK_RSHIFT: bit = KMOD_RSHIFT; break;
        case SDLK_LCTRL:  bit = KMOD_LCTRL;  break;
        case SDLK_RCTRL:  bit = KMOD_RCTRL;  break;
        case SDLK_LALT:   bit = KMOD_LALT;   break;
        case SDLK_RALT:   bit = KMOD_RALT;   break;
        case SDLK_LMETA:  bit = KMOD_LMETA;  break;
        case SDLK_RMETA:  bit = KMOD_RMETA;  break;
        default: return;
    }
    if (pressed) s_mod = (SDLMod)(s_mod | bit);
    else         s_mod = (SDLMod)(s_mod & ~bit);
}

static const char k_shifted[] = ")!@#$%^&*(";

static Uint16 unicode_of(SDLKey sym) {
    bool shift = (s_mod & KMOD_SHIFT) != 0;
    bool caps  = (s_mod & KMOD_CAPS) != 0;

    if (sym >= SDLK_a && sym <= SDLK_z) {
        return (Uint16)((shift != caps) ? (sym - 32) : sym);
    }
    if (sym >= SDLK_0 && sym <= SDLK_9) {
        return shift ? (Uint16)k_shifted[sym - SDLK_0] : (Uint16)sym;
    }
    if (!shift) {
        if (sym >= SDLK_SPACE && sym <= SDLK_DELETE) return (Uint16)sym;
        return 0;
    }
    switch (sym) {
        case SDLK_MINUS:        return '_';
        case SDLK_EQUALS:       return '+';
        case SDLK_LEFTBRACKET:  return '{';
        case SDLK_RIGHTBRACKET: return '}';
        case SDLK_BACKSLASH:    return '|';
        case SDLK_SEMICOLON:    return ':';
        case SDLK_QUOTE:        return '"';
        case SDLK_BACKQUOTE:    return '~';
        case SDLK_COMMA:        return '<';
        case SDLK_PERIOD:       return '>';
        case SDLK_SLASH:        return '?';
        default:                return (sym >= SDLK_SPACE && sym <= SDLK_DELETE) ? (Uint16)sym : 0;
    }
}

void shim_events_key(uint16_t scancode, uint32_t keycode, bool pressed) {
    (void)keycode;
    SDLKey sym = key_from_wire(scancode);
    if (sym == SDLK_UNKNOWN) return;

    mod_update(sym, pressed);
    if (sym < SDLK_LAST) s_keys[sym] = pressed ? 1 : 0;

    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type                = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state           = pressed ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.scancode = (Uint8)scancode;
    ev.key.keysym.sym      = sym;
    ev.key.keysym.mod      = s_mod;
    ev.key.keysym.unicode  = (s_unicode && pressed) ? unicode_of(sym) : 0;
    events_post(&ev);
}

void shim_events_quit_request(void) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = SDL_QUIT;
    events_post(&ev);
}

void shim_events_active(Uint8 state, bool gain) {
    if (gain) s_app_state |= state;
    else      s_app_state &= (Uint8)~state;

    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type         = SDL_ACTIVEEVENT;
    ev.active.gain  = gain ? 1 : 0;
    ev.active.state = state;
    events_post(&ev);
}

bool shim_events_unicode(void) {
    return s_unicode;
}

void shim_events_pump(void) {
    shim_joystick_announce();
    win_pump();
    shim_ipc_pump();
}

void SDL_PumpEvents(void) {
    shim_events_pump();
}

int SDL_PollEvent(SDL_Event *event) {
    SDL_PumpEvents();
    if (!event) return events_pending() ? 1 : 0;
    return events_pop(event) ? 1 : 0;
}

int SDL_WaitEvent(SDL_Event *event) {
    for (;;) {
        if (SDL_PollEvent(event)) return 1;
        SDL_Delay(1);
    }
}

int SDL_PushEvent(SDL_Event *event) {
    if (!event) return -1;
    shim_events_push(event);
    return 0;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action, Uint32 mask) {
    if (!events || numevents <= 0) return 0;

    if (action == SDL_ADDEVENT) {
        for (int i = 0; i < numevents; i++) shim_events_push(&events[i]);
        return numevents;
    }

    SDL_Event kept[QUEUE_CAP];
    int       taken = 0;
    int       n     = 0;

    pthread_mutex_lock(&s_lock);
    while (s_tail != s_head) {
        SDL_Event ev = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_CAP;

        bool match = (mask & SDL_EVENTMASK(ev.type)) != 0 && taken < numevents;
        if (match) {
            events[taken++] = ev;
            if (action == SDL_GETEVENT) continue;
        }
        kept[n++] = ev;
    }
    s_head = s_tail = 0;
    for (int i = 0; i < n; i++) s_queue[s_head++] = kept[i];
    pthread_mutex_unlock(&s_lock);

    return taken;
}

Uint8 SDL_EventState(Uint8 type, int state) {
    if (type >= SDL_NUMEVENTS) return SDL_IGNORE;
    Uint8 previous = s_state[type];
    if (state != SDL_QUERY) s_state[type] = (Uint8)state;
    return previous;
}

void SDL_SetEventFilter(SDL_EventFilter filter) {
    s_filter = filter;
}

SDL_EventFilter SDL_GetEventFilter(void) {
    return s_filter;
}

Uint8 *SDL_GetKeyState(int *numkeys) {
    if (numkeys) *numkeys = SDLK_LAST;
    return s_keys;
}

SDLMod SDL_GetModState(void) {
    return s_mod;
}

void SDL_SetModState(SDLMod modstate) {
    s_mod = modstate;
}

int SDL_EnableUNICODE(int enable) {
    int previous = s_unicode ? 1 : 0;
    if (enable >= 0) s_unicode = enable != 0;
    return previous;
}

int SDL_EnableKeyRepeat(int delay, int interval) {
    s_repeat_delay    = delay;
    s_repeat_interval = interval;
    return 0;
}

void SDL_GetKeyRepeat(int *delay, int *interval) {
    if (delay)    *delay    = s_repeat_delay;
    if (interval) *interval = s_repeat_interval;
}

Uint8 SDL_GetAppState(void) {
    return s_app_state;
}

char *SDL_GetKeyName(SDLKey key) {
    static char name[16];
    if (key >= SDLK_SPACE && key < SDLK_DELETE) {
        snprintf(name, sizeof(name), "%c", (char)key);
    } else {
        snprintf(name, sizeof(name), "key %d", (int)key);
    }
    return name;
}
