/**
 * @brief Infinite Damage cheat: anything the player hits dies in one hit,
 * with any weapon. Toggled from Dopo Options > Cheats and not saved.
 *
 * Every hit goes through P_DamageMobj, so its upstream body is kept under
 * another name and a new P_DamageMobj in front of it raises the damage of
 * hits whose source is a player on anything that is not a player. The
 * raise is twice the target's health, so halving rules (Heretic/Hexen)
 * still kill; immunities in the upstream body are still honored. In a
 * netgame the host sets it for every player (multiplayer/settings.c).
 */

/**
 * @brief The cheat's toggle and the damage raising entry point; the
 * upstream function continues right below under its new name.
 *
 * @patch src/p_inter.c 2303
 */
#include "dopo/cheats.h"

static void dopo_upstream_damage_mobj(mobj_t *target, mobj_t *inflictor,
                                      mobj_t *source, int damage);

void P_DamageMobj(mobj_t *target,mobj_t *inflictor, mobj_t *source, int damage)
{
  if (dopo_cheats.damage && !demoplayback &&
      source && source->player && target && !target->player &&
      damage < target->health * 2)
    damage = target->health * 2;

  dopo_upstream_damage_mobj(target, inflictor, source, damage);
}

/* @endpatch */

/**
 * @brief Upstream P_DamageMobj, renamed.
 *
 * @patch src/p_inter.c 2303-2303
 */
static void dopo_upstream_damage_mobj(mobj_t *target,mobj_t *inflictor, mobj_t *source, int damage)
/* @endpatch */
