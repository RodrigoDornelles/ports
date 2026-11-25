#pragma once

#include <stdint.h>

/* library/chavo8.nim */
void chavo8_NimMain(void);
const char *chavo8_game_name(int version);
const char *chavo8_v1_line(int idioma, int memo, int index);
void chavo8_v1_create(void);
const char *chavo8_v1_state(void);
int chavo8_v1_frame(const uint16_t *keys, int count);
int chavo8_v1_start(int player, uint16_t *keys);
