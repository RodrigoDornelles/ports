/**
 * @brief Change Game: the games next to the running content and the
 * switch to one of them (dopo/change_game.c).
 */
#ifndef DOPO_CHANGE_GAME_H
#define DOPO_CHANGE_GAME_H

#include "doomtype.h"

/** @brief Lists the games again and returns how many there are. */
int dopo_games_scan(void);

/** @brief Name of the i-th game as the menu shows it. */
const char *dopo_games_name(int i);

/** @brief Whether the i-th game is the one running. */
dbool dopo_games_is_current(int i);

/** @brief Asks for a switch to the i-th game at the next frame. */
void dopo_games_load(int i);

#endif
