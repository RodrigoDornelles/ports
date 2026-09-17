#ifndef DOPO_SDL1_H
#define DOPO_SDL1_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "shim.h"
#include "window.h"
#include "capture.h"

void         shim_events_push(const SDL_Event *ev);
void         shim_events_active(Uint8 state, bool gain);
bool         shim_events_unicode(void);

SDL_Surface *shim_surface_new(void *pixels, int width, int height, int depth, int pitch,
                              Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask);
void         shim_surface_pixels(const SDL_Surface *surface, capture_pixels_t *out,
                                 uint32_t *palette_cache);
SDL_Surface *shim_screen(void);

#endif
