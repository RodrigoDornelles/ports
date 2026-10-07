/**
 * @brief Change Weapon (menu/change_weapon/index.c).
 */
#ifndef DOPO_CHANGE_WEAPON_H
#define DOPO_CHANGE_WEAPON_H

#include "doomdef.h"

/**
 * @brief Weapon chosen in the submenu, sent by the next ticcmd as a weapon
 * key would; WP_NOCHANGE when none.
 */
extern weapontype_t dopo_weapon_request;

#endif
