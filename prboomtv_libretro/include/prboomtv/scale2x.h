/**
 * @file scale2x.h
 * @brief The prboom-scale core option (library/core/enhancements/gpu/scale2x.c):
 * sprites and 2D patches made bigger with Scale2x once, on the CPU, when
 * they are loaded; the gl renderer sends them as textures, the software
 * renderer draws its sprites from them.
 * - 0: off.
 * - 1: twice the size, only the contour from Scale2x.
 * - 2, the default: twice the size, all of it from Scale2x.
 * - 3: as 2, then the contour again at four times the size.
 */
#ifndef PRBOOMTV_SCALE2X_H
#define PRBOOMTV_SCALE2X_H

#include <stdint.h>

#include "libretro.h"
#include "r_patch.h"

/** @brief The option. */
extern const struct retro_core_option_v2_definition dopo_scale_option;

/** @brief The prboom-scale in effect, 0..3. */
extern int dopo_scale;

/** @brief Reads the option (startup and every change). */
void dopo_scale_read(retro_environment_t environ_cb);

/** @brief A transparent texel of dopo_scale_image. */
#define DOPO_SCALE_CLEAR 0xffff

/**
 * @brief The patch's palette indices at k times its size (k = 1, 2 or 4
 * for the mode, stored in *k), DOPO_SCALE_CLEAR where transparent,
 * column major: column x is k * height texels long. NULL when out of
 * memory; the caller frees it.
 */
uint16_t *dopo_scale_image(const rpatch_t *p, int mode, int *k);

/**
 * @brief A sprite lump's patch at the prboom-scale size, for the software
 * renderer (cached until the wads or the option change), and its factor
 * in *k. NULL when the option is off or it cannot be made.
 */
const rpatch_t *dopo_scale_sprite(int lump, const rpatch_t *p, int *k);

#endif
