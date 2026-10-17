/**
 * @file glow.h
 * @brief The prboom-glow core option (library/core/enhancements/gpu/hd/glow.c),
 * gl only: fireballs, plasma, explosions, teleport fog and the weapon's
 * muzzle flash drawn additively, so they light what is behind them instead
 * of covering it, as in GZDoom.
 * - 0: off.
 * - 1: additive.
 * - 2, the default: additive plus a corona, a soft halo around them.
 */
#ifndef PRBOOMTV_GLOW_H
#define PRBOOMTV_GLOW_H

#include "libretro.h"
#include "r_defs.h"

/** @brief The option. */
extern const struct retro_core_option_v2_definition dopo_glow_option;

/** @brief The prboom-glow in effect, 0..2. */
extern int dopo_glow;

/** @brief Reads the option (startup and every change). */
void dopo_glow_read(retro_environment_t environ_cb);

/**
 * @brief A thing's sprite (its sprite number and frame) or, with sprite
 * -1, the weapon's flash: when it glows and gl is on, it becomes
 * additive at full strength (vis->translucent 2, alpha 32), drawn as
 * DECORATE's RenderStyle Add is.
 */
void dopo_glow_mark(vissprite_t *vis, int sprite, int frame);

#endif
