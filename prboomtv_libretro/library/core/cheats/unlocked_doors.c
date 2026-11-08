/**
 * @brief Unlocked Doors cheat: a player opens any locked door, switch or
 * line without its key. Toggled from Dopo Options > Cheats and not saved.
 *
 * The keys are checked in many places (Doom's and Heretic's doors, Boom's
 * generalized locks, ZDoom's lock numbers, Hexen's keys), so instead of
 * each check the activation of a line is wrapped: while a player's line is
 * tried (used, crossed, shot or, in Hexen, pushed), the player holds every
 * key, and the cards held before come back right after, so no key shows on
 * the status bar. Off in demos. In a netgame the host sets it for every
 * player (enhancements/multiplayer/settings.c).
 */

/**
 * @brief The keys lent and given back.
 *
 * @patch src/p_switch.c 538
 */
#include "prboomtv/cheats.h"

player_t *dopo_cheat_keys_begin(mobj_t *thing, dbool *saved)
{
  player_t *player = thing ? thing->player : NULL;
  int i;

  if (!dopo_cheats.doors || demoplayback || !player)
    return NULL;
  for (i = 0; i < NUMCARDS; i++)
  {
    saved[i] = player->cards[i];
    player->cards[i] = TRUE;
  }
  return player;
}

void dopo_cheat_keys_end(player_t *player, const dbool *saved)
{
  int i;

  if (player)
    for (i = 0; i < NUMCARDS; i++)
      player->cards[i] = saved[i];
}

/**
 * @brief A used line (Doom, Boom, Heretic), with the keys lent; upstream
 * continues right below under its new name.
 */
static dbool dopo_upstream_use_special_line(mobj_t *thing, line_t *line, int side);

dbool P_UseSpecialLine(mobj_t *thing, line_t *line, int side)
{
  dbool saved[NUMCARDS];
  player_t *lent = dopo_cheat_keys_begin(thing, saved);
  dbool used = dopo_upstream_use_special_line(thing, line, side);

  dopo_cheat_keys_end(lent, saved);
  return used;
}

/* @endpatch */

/**
 * @brief Upstream P_UseSpecialLine, renamed.
 *
 * @patch src/p_switch.c 538-542
 */
static dbool dopo_upstream_use_special_line(mobj_t *thing, line_t *line, int side)
/* @endpatch */

/**
 * @brief A crossed line (Doom, Boom, Heretic: walk-over locks), with the
 * keys lent.
 *
 * @patch src/p_spec.c 1338
 */
#include "prboomtv/cheats.h"

static void dopo_upstream_cross_special_line(line_t *line, int side, mobj_t *thing);

void P_CrossSpecialLine(line_t *line, int side, mobj_t *thing)
{
  dbool saved[NUMCARDS];
  player_t *lent = dopo_cheat_keys_begin(thing, saved);

  dopo_upstream_cross_special_line(line, side, thing);
  dopo_cheat_keys_end(lent, saved);
}

/* @endpatch */

/**
 * @brief Upstream P_CrossSpecialLine, renamed.
 *
 * @patch src/p_spec.c 1338-1338
 */
static void dopo_upstream_cross_special_line(line_t *line, int side, mobj_t *thing)
/* @endpatch */

/**
 * @brief A shot line (Boom's gun locks), with the keys lent.
 *
 * @patch src/p_spec.c 2237
 */
static void dopo_upstream_shoot_special_line(mobj_t *thing, line_t *line);

void P_ShootSpecialLine(mobj_t *thing, line_t *line)
{
  dbool saved[NUMCARDS];
  player_t *lent = dopo_cheat_keys_begin(thing, saved);

  dopo_upstream_shoot_special_line(thing, line);
  dopo_cheat_keys_end(lent, saved);
}

/* @endpatch */

/**
 * @brief Upstream P_ShootSpecialLine, renamed.
 *
 * @patch src/p_spec.c 2237-2239
 */
static void dopo_upstream_shoot_special_line(mobj_t *thing, line_t *line)
/* @endpatch */

/**
 * @brief Every Hexen (and ZDoom) line activation, use, cross, impact and
 * push, with the keys lent: its locked doors and scripts check them here.
 *
 * @patch src/hexen/p_spec_hexen.c 2652
 */
#include "prboomtv/cheats.h"

static dbool dopo_upstream_activate_hexen_line(line_t *line, mobj_t *mo, int side,
                                               int activation);

static dbool P_ActivateHexenLine(line_t *line, mobj_t *mo, int side,
                                 int activation)
{
  dbool saved[NUMCARDS];
  player_t *lent = dopo_cheat_keys_begin(mo, saved);
  dbool ok = dopo_upstream_activate_hexen_line(line, mo, side, activation);

  dopo_cheat_keys_end(lent, saved);
  return ok;
}

/* @endpatch */

/**
 * @brief Upstream P_ActivateHexenLine, renamed.
 *
 * @patch src/hexen/p_spec_hexen.c 2652-2653
 */
static dbool dopo_upstream_activate_hexen_line(line_t *line, mobj_t *mo, int side,
                                               int activation)
/* @endpatch */
