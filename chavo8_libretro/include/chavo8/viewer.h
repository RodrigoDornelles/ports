#pragma once

#include "chavo8/delphix.h"

/* browses what vfs_open found: the folders' pictures and waves, the exe's
 * own, fighters sliced into frames as the game does, and the font */
bool viewer_init(const uint8_t *exe, size_t size);
void viewer_run(uint16_t buttons, surface_t *screen);
void viewer_deinit(void);

void chavo8_log(const char *fmt, ...);
void chavo8_notify(const char *text);
