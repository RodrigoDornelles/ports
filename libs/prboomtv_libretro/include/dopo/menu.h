/**
 * @brief Dopo menus (source/menu/), one folder per layer of the menu tree:
 *
 *   menu/index.c                 main menu
 *   menu/change_weapon/          Change Weapon
 *   menu/singleplayer/           New, Load, Save and Delete Game
 *   menu/multiplayer/            lobby, and the game's setup in a netgame
 *   menu/scoreboard/             players, and the admin's player options
 *   menu/settings/               Dopo Options (Patchs, Cheats, Read This!),
 *                                Game Options and Change Game
 *   menu/layout.c                M_Drawer and the shared layout
 *
 * Every file is a set of hunks in src/m_menu.c, the only place menu_t is
 * known, so this header is for those hunks only. Everything a menu needs
 * from another is declared here, so the files do not depend on the order
 * they land in.
 *
 * Layout: two fixed anchors, so moving between menus of a kind the items
 * never jump.
 *
 *   small menu (dopo_menu_small()): the menus to get somewhere -- main
 *     menu, Singleplayer, Settings, Dopo Options, a player's options,
 *     confirmations. A column under the game's logo, centered on it from
 *     the widest item the main menu can have, so it never moves.
 *   big menu (dopo_menu_big()): the menus of options with a value each --
 *     Patchs, Cheats, Change Weapon, Change Game, Multiplayer, Scoreboard.
 *     Labels from DOPO_MENU_LEFT, values right aligned on DOPO_MENU_RIGHT,
 *     from DOPO_MENU_TOP under the title.
 *
 * Text is left aligned, titles included: every title, small or big menu,
 * starts on the small menu column, so it never moves (dopo_menu_title()).
 *
 * Entering a menu starts on its first item (dopo_menu_open()); going back
 * returns to the item it was left on. The
 * menus PrBoom brings (Game Options and its submenus, Load and Save,
 * episode and skill) keep their own layout.
 */
#ifndef DOPO_MENU_H
#define DOPO_MENU_H

#include "dopo/change_game.h"
#include "dopo/change_weapon.h"
#include "dopo/cheats.h"
#include "dopo/multiplayer.h"
#include "dopo/patchs.h"

/* ------------------------------------------------------------------ */
/* Layout (menu/layout.c)                                              */
/* ------------------------------------------------------------------ */

/** @brief Top of every menu title. */
#define DOPO_MENU_TITLE_Y 15

/** @brief Small menus: first item, under the logo or the title; the
 * column's x is computed from the game's font (dopo_menu_small()), this
 * is only where it starts. */
#define DOPO_MENU_SMALL_X 96
#define DOPO_MENU_SMALL_Y 80

/** @brief Big menus: labels start here. */
#define DOPO_MENU_LEFT    60

/** @brief Big menus: values end here (the block is centered). */
#define DOPO_MENU_RIGHT   260

/** @brief Big menus: first row. */
#define DOPO_MENU_TOP     40

/** @brief Line height of the small font. */
#define DOPO_MENU_SMALL_LINE 9

/** @brief Puts a menu on the small menu anchor. */
void dopo_menu_small(menu_t *menu);

/** @brief Puts a menu on the big menu anchor. */
void dopo_menu_big(menu_t *menu);

/** @brief Enters a menu, on its first item. */
void dopo_menu_open(menu_t *menu);

/** @brief Draws a menu title in the big font, on the small menu column. */
void dopo_menu_title(const char *title);

/**
 * @brief Whether the screen shows the game's title page, whose art
 * already carries the game's logo (menu/index.c, in d_main.c).
 */
dbool dopo_title_page(void);

/** @brief Draws an item's value, right aligned on DOPO_MENU_RIGHT. */
void dopo_menu_value(const menu_t *menu, int item, const char *text, int cm);

/** @brief Draws ON/OFF in gold as the value of the first count items. */
void dopo_menu_toggles(const menu_t *menu, const dbool *const values[], int count);

/** @brief Draws a line of the small font centered on the screen. */
void dopo_menu_small_centered(int y, const char *text, int cm);

/**
 * @brief Moves value to the next (choice 1: right or confirm) or previous
 * (choice 0: left) of steps, wrapping around; an unknown value starts
 * from the first step.
 */
void dopo_menu_cycle(int *value, const int *steps, int count, int choice);

/** @brief Writes OFF for 0, or the value with its unit (200MS, 10DEG). */
void dopo_menu_format(char *buf, size_t size, int value, const char *unit);

/**
 * @brief Whether the menu refuses something a netgame cannot do (start or
 * load another game), telling so.
 */
dbool dopo_menu_netgame_blocked(void);

/**
 * @brief Whether a menu marks its own selection, so M_Drawer leaves the
 * skull out (menu/scoreboard/index.c).
 */
dbool dopo_menu_draws_cursor(const menu_t *menu);

/* ------------------------------------------------------------------ */
/* Menus, by layer                                                     */
/* ------------------------------------------------------------------ */

/* menu/change_weapon/index.c */
void dopo_change_weapon_open(int choice);

/* menu/singleplayer/index.c */
extern menu_t dopo_singleplayer_def;
void dopo_singleplayer_open(int choice);

/* menu/singleplayer/delete.c */
void dopo_delete_open(int choice);

/* menu/multiplayer/index.c */
void dopo_multiplayer_open(int choice);

/* menu/multiplayer/rooms.c */
extern menu_t dopo_list_def;
void dopo_rooms_menu_open(void);

/* menu/scoreboard/index.c */
extern menu_t dopo_scoreboard_def;
void dopo_scoreboard_open(int choice);

/* menu/scoreboard/player.c */
void dopo_player_open(int slot);

/* menu/settings/index.c */
extern menu_t dopo_settings_def;
void dopo_settings_open(int choice);

/* menu/settings/game_options.c */
void dopo_game_options_open(int choice);
void dopo_read_this(int choice);

/* menu/settings/change_game.c */
void dopo_change_game_open(int choice);

/* menu/settings/dopo/index.c */
extern menu_t dopo_dopo_options_def;
void dopo_dopo_options_open(int choice);

/* menu/settings/dopo/patchs.c, cheats.c, about.c */
void dopo_patchs_open(int choice);
void dopo_cheats_open(int choice);
void dopo_about_open(int choice);

#endif
