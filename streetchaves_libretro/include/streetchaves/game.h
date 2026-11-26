#pragma once

#include <stdint.h>

/* library/streetchaves.nim */
void streetchaves_NimMain(void);
const char *streetchaves_game_name(int version);
const char *streetchaves_v1_line(int idioma, int memo, int index);
void streetchaves_v1_create(void);
void streetchaves_v1_nivel(int nivel);
const char *streetchaves_v1_state(void);
int streetchaves_v1_frame(const uint16_t *keys, int count);
int streetchaves_v1_start(int player, uint16_t *keys);
int streetchaves_v1_tela(void);
int streetchaves_v1_back(void);
