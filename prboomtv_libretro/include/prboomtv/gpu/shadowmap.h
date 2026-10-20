/**
 * @file shadowmap.h
 * @brief The prboom-shadowmap core option (library/core/enhancements/gpu/hd/shadowmap.c),
 * gl only: the walls stop the dynamic lights, as GZDoom's light shadowmaps
 * do. For each light of the frame, the distance to the nearest wall in
 * each of DOPO_SHADOW_RAYS directions around it, on the map's plane (a
 * step or a ledge casts none, as in GZDoom); what lies farther is in its
 * shadow.
 */
#ifndef PRBOOMTV_GPU_SHADOWMAP_H
#define PRBOOMTV_GPU_SHADOWMAP_H

#include <stdint.h>

#include "libretro.h"
#include "prboomtv/gpu/dynlights.h"

/** @brief Directions around a light. */
#define DOPO_SHADOW_RAYS 512

/** @brief Whether the lights cast shadows: prboom-dynlight=3, "All +
 * Shadows" (off by default, as GZDoom's light shadowmaps). */
int dopo_shadowmap_on(void);

/**
 * @brief The shadows of the frame's lights (relative to the eye, as
 * dopo_dynlight_collect gives them): row i, DOPO_SHADOW_RAYS values, the
 * distance to the nearest wall in each direction over the light's radius,
 * 0..65535 (65535: nothing within reach). Direction k points at angle
 * 2 pi (k + 0.5) / DOPO_SHADOW_RAYS on the map, counterclockwise from east.
 * Lights that do not move are kept between frames.
 */
void dopo_shadowmap_build(const dopo_light_t *lights, int n, uint16_t *out);

/** @brief Whether a point (relative to the eye, map units) is lit by the
 * frame's light i, after dopo_shadowmap_build. */
int dopo_shadowmap_lit(int i, float x, float y);

#endif
