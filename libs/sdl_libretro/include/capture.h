#ifndef SDL_LIBRETRO_CAPTURE_H
#define SDL_LIBRETRO_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "ipc.h"

dopo_ipc_format_t capture_format(void);
bool                capture_enabled(void);
void                capture_resize(int w, int h);
void                capture_frame(void);
void                capture_quit(void);

#endif
