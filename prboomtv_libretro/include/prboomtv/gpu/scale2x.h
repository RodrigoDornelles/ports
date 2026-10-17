/**
 * @file scale2x.h
 * @brief The prboom-scale core option (library/core/enhancements/gpu/hd/scale2x.c),
 * gl only: sprites and 2D patches made bigger with Scale2x once, on the
 * CPU, when gl.c uploads them as textures.
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

/** @brief The brightmap level of a texel of dopo_scale_image (bits 8-14:
 * dopo_brightmask_alpha, 0 none). */
#define DOPO_SCALE_LEVEL(c) (((c) >> 8) & 0x7f)

/**
 * @brief The patch's palette indices at k times its size (k = 1, 2 or 4
 * for the mode, stored in *k), DOPO_SCALE_CLEAR where transparent,
 * the brightmap's level in DOPO_SCALE_LEVEL (the patch's brightmap,
 * column major, or NULL), column major: column x is k * height texels long.
 * NULL when out of memory; the caller frees it.
 */
uint16_t *dopo_scale_image(const rpatch_t *p, const uint8_t *bright, int mode, int *k);

#endif
