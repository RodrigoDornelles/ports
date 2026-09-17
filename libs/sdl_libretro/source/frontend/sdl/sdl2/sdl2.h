#ifndef DOPO_SDL2_H
#define DOPO_SDL2_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "shim.h"
#include "window.h"

void     shim_events_push(const SDL_Event *ev);
void     shim_events_window(uint8_t event, int32_t data1, int32_t data2);
uint32_t shim_window_id(void);

#endif
