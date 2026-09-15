#ifndef DOPO_IPC_H
#define DOPO_IPC_H

#include <stdint.h>

#include "dopo.h"

#define DOPO_IPC_ENV_SOCKET  "DOPO_IPC_SOCKET"
#define DOPO_SDL2_ENV_NATIVE  "DOPO_SDL2_NATIVE"
#define DOPO_SDL2_ENV_X11KEYS "DOPO_SDL2_X11_KEYS"
#define DOPO_ENV_DEBUG       "DOPO_DEBUG"
#define DOPO_SDL2_ENV_SYNC   "DOPO_SDL2_SYNC"
#define DOPO_SDL2_ENV_FORMAT "DOPO_SDL2_FORMAT"
#define DOPO_SDL2_SHIM_NAME  "libSDL2-2.0.so.0"

typedef enum __attribute__((packed)) {
    DOPO_IPC_PKT_NONE = 0,
    DOPO_IPC_PKT_HELLO,
    DOPO_IPC_PKT_BYE,
    DOPO_IPC_PKT_KEY,
    DOPO_IPC_PKT_QUIT,
    DOPO_IPC_PKT_WINDOW,
    DOPO_IPC_PKT_PAD,

    DOPO_IPC_PKT_AUDIO_CFG,
    DOPO_IPC_PKT_AUDIO,
    DOPO_IPC_PKT_AUDIO_STOP,

    DOPO_IPC_PKT_FB_INIT,
    DOPO_IPC_PKT_FB_FRAME,
} dopo_ipc_pkt_type_t;

typedef enum __attribute__((packed)) {
    DOPO_IPC_FORMAT_EGL = 0,
    DOPO_IPC_FORMAT_RGBA8888,
    DOPO_IPC_FORMAT_RGB565,
} dopo_ipc_format_t;

#define DOPO_IPC_FB_MAX_WIDTH  1920
#define DOPO_IPC_FB_MAX_HEIGHT 1080

typedef enum __attribute__((packed)) {
    DOPO_IPC_PAD_A = 0,
    DOPO_IPC_PAD_B,
    DOPO_IPC_PAD_C,
    DOPO_IPC_PAD_D,
    DOPO_IPC_PAD_E,
    DOPO_IPC_PAD_F,
    DOPO_IPC_PAD_MENU,
    DOPO_IPC_PAD_BUTTONS,

    DOPO_IPC_PAD_UP = DOPO_IPC_PAD_BUTTONS,
    DOPO_IPC_PAD_DOWN,
    DOPO_IPC_PAD_LEFT,
    DOPO_IPC_PAD_RIGHT,
    DOPO_IPC_PAD_COUNT,
} dopo_ipc_pad_t;

#define DOPO_IPC_PAD_NAME "Dopo Libretro Pad"
#define DOPO_IPC_PAD_GUID_PREFIX "0300000000000000444f504f504144"
#define DOPO_IPC_PAD_GUID_0      DOPO_IPC_PAD_GUID_PREFIX "30"
#define DOPO_IPC_PAD_MAX_PLAYERS 4

typedef struct {
    uint8_t  type;
    uint8_t  flag;
    uint16_t code;
    uint32_t arg;
} dopo_ipc_pkt_t;

#define DOPO_IPC_AUDIO_MAX_CHANNELS 2
#define DOPO_IPC_AUDIO_MAX_FRAMES   2048
#define DOPO_IPC_AUDIO_MAX_BYTES \
    (DOPO_IPC_AUDIO_MAX_FRAMES * DOPO_IPC_AUDIO_MAX_CHANNELS * 2)

#define DOPO_IPC_PKT_MAX \
    (sizeof(dopo_ipc_pkt_t) + DOPO_IPC_AUDIO_MAX_BYTES)

#endif
