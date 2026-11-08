/**
 * @brief Cheats from Dopo Options > Cheats (library/core/cheats/), set through
 * enhancements/multiplayer/settings.c so a netgame changes them on every machine at
 * the same tic.
 */
#ifndef PRBOOMTV_CHEATS_H
#define PRBOOMTV_CHEATS_H

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
  dbool doors;          /* Unlocked Doors: keyed lines open without keys */
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

struct player_s;
struct mobj_s;

/**
 * @brief Unlocked Doors, around a line a thing activates: with the cheat on
 * and a player as the thing, the player holds every key while the line is
 * tried and the cards held before go to saved (NUMCARDS of them); returns
 * the player whose cards dopo_cheat_keys_end puts back, or NULL
 * (cheats/unlocked_doors.c).
 */
struct player_s *dopo_cheat_keys_begin(struct mobj_s *thing, dbool *saved);

/**
 * @brief Puts back the cards dopo_cheat_keys_begin saved; NULL does nothing.
 */
void dopo_cheat_keys_end(struct player_s *player, const dbool *saved);

/**
 * @brief Keeps the cheats that are on up for every player, once per tic
 * (cheats/cheats.c).
 */
void dopo_cheats_tic(void);

#endif
