/**
 * @file brightmaps.h
 * @brief Brightmaps (library/core/enhancements/gpu/hd/brightmaps.c), with
 * prboom-dynlight on, gl only: the texels a brightmap marks are drawn at
 * full brightness whatever the light around them, as GZDoom does
 * (monsters' eyes, muzzle flashes, torch flames, and with the wads' own
 * brightmaps screens and lamps on walls). The sprites' come from GZDoom's brightmaps.pk3 when it
 * is next to the game, next to the core or in the system directory; the
 * wads' own GLDEFS brightmaps (read by u_brightmap.c) win over it.
 */
#ifndef PRBOOMTV_BRIGHTMAPS_H
#define PRBOOMTV_BRIGHTMAPS_H

#include <stdint.h>

#include "libretro.h"
#include "r_defs.h"
#include "r_patch.h"

/**
 * @brief The mask of a sprite lump (relative to firstspritelump) whose
 * patch is p, column major (mask[x * height + y]): how bright each texel
 * is at least, 0 none to 255 full (1 also full: the wads' own masks,
 * u_brightmap.c, are 0 or 1); or NULL: none, or prboom-dynlight is off.
 */
const uint8_t *dopo_brightmask_sprite(int spritelump, const rpatch_t *p);

/** @brief The alpha gl.c stores a mask's level in (a half and below: the
 * level, 1..127; 255 none). */
static inline uint8_t dopo_brightmask_alpha(uint8_t level)
{
   const unsigned v = level == 1 ? 255 : level;
   return v ? (uint8_t)((v + 1) / 2 > 127 ? 127 : (v + 1) / 2) : 255;
}

/**
 * @brief A thing's full bright frame (R_ProjectSprite) whose brightmap says
 * disablefullbright (the archive's, as GZDoom's for muzzle flashes): it
 * takes lit, the room's light, so only what its brightmap marks glows.
 */
void dopo_brightmap_project(vissprite_t *vis, const lighttable_t *lit);

/** @brief As dopo_brightmask_sprite for a wall texture (column major). */
const uint8_t *dopo_brightmask_texture(int texnum);

/** @brief As dopo_brightmask_sprite for a flat (64x64, row major). */
const uint8_t *dopo_brightmask_flat(int flatnum);

#endif
