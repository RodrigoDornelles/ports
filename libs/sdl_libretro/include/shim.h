#ifndef DOPO_SHIM_H
#define DOPO_SHIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ipc.h"

#ifndef SHIM_SDL_MAJOR
#define SHIM_SDL_MAJOR 0
#endif

#ifndef SHIM_TAG
#if SHIM_SDL_MAJOR == 2
#define SHIM_TAG "[libSDL2-shim]"
#elif SHIM_SDL_MAJOR == 1
#define SHIM_TAG "[libSDL1-shim]"
#else
#define SHIM_TAG "[" DOPO_NAME "]"
#endif
#endif

#ifndef SHIM_API
#define SHIM_API DOPO_IPC_API_OTHER
#endif

/* implemented by common/, shared by every frontend */
void        shim_set_error(const char *fmt, ...);
void        shim_clear_error(void);
const char *shim_error_text(void);
uint64_t    shim_now_ms(void);
uint64_t    shim_ticks_ms(void);

void     shim_ipc_connect(void);
void     shim_ipc_close(void);
void     shim_ipc_send(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg);
void     shim_ipc_send_blob(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg,
                            const void *payload, size_t bytes);
void     shim_ipc_pump(void);

/* implemented by each frontend, called by common/ and window/ */
void     shim_events_init(void);
void     shim_events_quit(void);
void     shim_events_pump(void);
void     shim_events_key(uint16_t scancode, uint32_t keycode, bool pressed);
void     shim_events_quit_request(void);

void     shim_joystick_announce(void);
void     shim_joystick_input(uint8_t pad, bool pressed);
void     shim_joystick_quit(void);

void     shim_video_quit(void);
void     shim_audio_quit(void);

#endif
