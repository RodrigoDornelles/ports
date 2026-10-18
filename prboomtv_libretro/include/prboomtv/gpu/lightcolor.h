/**
 * @file lightcolor.h
 * @brief Coloured sector light (library/core/enhancements/gpu/hd/lightcolor.c),
 * gl only: a UDMF sector's lightcolor (a cave lit blue, a room lit red, as
 * maps made for GZDoom have them) tints what lies in it, its walls, floor,
 * ceiling and things. gl.c gives each colour a block of the colour table
 * with the palette tinted, so it costs nothing per pixel.
 */
#ifndef PRBOOMTV_GPU_LIGHTCOLOR_H
#define PRBOOMTV_GPU_LIGHTCOLOR_H

#include <stdint.h>

/** @brief A sector's light colour, 0xRRGGBB; 0xffffff when it has none. */
uint32_t dopo_sector_lightcolor(int secnum);

/**
 * @brief The tag a sector's floor and ceiling carry in their visplane's
 * skybox (-1, unused on a plane that is not sky): -1 when untinted, else
 * -2 - its colour's block (gl.c), so planes of other colours do not merge.
 */
int dopo_gl_plane_tag(int secnum);

#endif
