/**
 * @brief Cheats from Dopo Options > Cheats (library/core/cheats/), set through
 * multiplayer/settings.c so a netgame changes them on every machine at
 * the same tic.
 */
#ifndef DOPO_CHEATS_H
#define DOPO_CHEATS_H

#include "doomtype.h"

/**
 * @brief Cheats from Dopo Options > Cheats. They act on every player, so
 * in a netgame only the host changes them and the change reaches every
 * machine at the same tic.
 */
typedef struct
{
  dbool ammo;
  dbool life;
  dbool weapons;
  dbool damage;
  int   aim_assist;     /* degrees on each side of the crosshair, 0 = off */
  int   trigger_assist; /* same as aim_assist */
} dopo_cheats_t;

/**
 * @brief The cheats the game runs with now.
 */
extern dopo_cheats_t dopo_cheats;

/**
 * @brief The cheats the menu shows: the last ones asked for, even while
 * they wait for their tic.
 */
const dopo_cheats_t *dopo_cheats_wanted(void);

/**
 * @brief Asks for new cheats: right away outside a netgame, from the next
 * tic on every machine inside one (host only).
 */
void dopo_cheats_request(const dopo_cheats_t *cheats);

/**
 * @brief Cheats for every machine from a tic of the netgame on (-1: right
 * away), as the host sent them.
 */
void dopo_cheats_at(int tic, const dopo_cheats_t *cheats);

/**
 * @brief Keeps the cheats that are on up for every player, once per tic
 * (cheats/cheats.c).
 */
void dopo_cheats_tic(void);

#endif
