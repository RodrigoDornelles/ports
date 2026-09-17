#include "sdl1.h"

#define PAD_INDEX 0

#define PAD_NUM_AXES    6
#define PAD_NUM_BUTTONS DOPO_IPC_PAD_BUTTONS
#define PAD_NUM_HATS    1

static struct {
    bool  announced;
    bool  opened;
    Uint8 buttons[PAD_NUM_BUTTONS];
    Uint8 hat;
} s_pad = { .hat = SDL_HAT_CENTERED };

static SDL_Joystick *const k_joystick = (SDL_Joystick *)&s_pad;

static bool pad_is_ours(const void *handle) {
    return handle == (const void *)&s_pad;
}

static void pad_push_button(Uint8 button, bool pressed) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type           = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
    ev.jbutton.which  = PAD_INDEX;
    ev.jbutton.button = button;
    ev.jbutton.state  = pressed ? SDL_PRESSED : SDL_RELEASED;
    shim_events_push(&ev);
}

static void pad_push_hat(Uint8 value) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type        = SDL_JOYHATMOTION;
    ev.jhat.which  = PAD_INDEX;
    ev.jhat.hat    = 0;
    ev.jhat.value  = value;
    shim_events_push(&ev);
}

static Uint8 pad_hat_bit(uint8_t pad) {
    switch (pad) {
        case DOPO_IPC_PAD_UP:    return SDL_HAT_UP;
        case DOPO_IPC_PAD_DOWN:  return SDL_HAT_DOWN;
        case DOPO_IPC_PAD_LEFT:  return SDL_HAT_LEFT;
        case DOPO_IPC_PAD_RIGHT: return SDL_HAT_RIGHT;
        default:                 return 0;
    }
}

void shim_joystick_announce(void) {
    /* SDL 1.2 has no hotplug events, the pad is simply always there */
    s_pad.announced = true;
}

void shim_joystick_input(uint8_t pad, bool pressed) {
    if (pad >= DOPO_IPC_PAD_COUNT) return;

    if (pad < PAD_NUM_BUTTONS) {
        if (s_pad.buttons[pad] == (pressed ? 1 : 0)) return;
        s_pad.buttons[pad] = pressed ? 1 : 0;
        pad_push_button(pad, pressed);
        return;
    }

    Uint8 bit = pad_hat_bit(pad);
    Uint8 hat = pressed ? (Uint8)(s_pad.hat | bit) : (Uint8)(s_pad.hat & ~bit);
    if (hat == s_pad.hat) return;
    s_pad.hat = hat;
    pad_push_hat(hat);
}

void shim_joystick_quit(void) {
    memset(s_pad.buttons, 0, sizeof(s_pad.buttons));
    s_pad.hat       = SDL_HAT_CENTERED;
    s_pad.announced = false;
    s_pad.opened    = false;
}

int SDL_NumJoysticks(void) {
    return 1;
}

const char *SDL_JoystickName(int device_index) {
    return device_index == PAD_INDEX ? DOPO_IPC_PAD_NAME : NULL;
}

SDL_Joystick *SDL_JoystickOpen(int device_index) {
    if (device_index != PAD_INDEX) {
        shim_set_error("no joystick %d", device_index);
        return NULL;
    }
    s_pad.opened = true;
    return k_joystick;
}

int SDL_JoystickOpened(int device_index) {
    return (device_index == PAD_INDEX && s_pad.opened) ? 1 : 0;
}

int SDL_JoystickIndex(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_INDEX : -1;
}

void SDL_JoystickClose(SDL_Joystick *joystick) {
    if (pad_is_ours(joystick)) s_pad.opened = false;
}

int SDL_JoystickNumAxes(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_AXES : -1;
}

int SDL_JoystickNumButtons(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_BUTTONS : -1;
}

int SDL_JoystickNumHats(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_HATS : -1;
}

int SDL_JoystickNumBalls(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? 0 : -1;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis) {
    (void)joystick; (void)axis;
    return 0;
}

Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button) {
    if (!pad_is_ours(joystick) || button < 0 || button >= PAD_NUM_BUTTONS) return 0;
    return s_pad.buttons[button];
}

Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat) {
    if (!pad_is_ours(joystick) || hat != 0) return SDL_HAT_CENTERED;
    return s_pad.hat;
}

int SDL_JoystickGetBall(SDL_Joystick *joystick, int ball, int *dx, int *dy) {
    (void)joystick; (void)ball;
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    return 0;
}

void SDL_JoystickUpdate(void) {
}

int SDL_JoystickEventState(int state) {
    (void)state;
    return SDL_ENABLE;
}
