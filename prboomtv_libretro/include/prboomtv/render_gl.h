/**
 * @file render_gl.h
 * @brief prboom-render = gl (library/core/render/): the software renderer
 * keeps deciding everything, the GPU paints (plan_gpu.txt).
 *
 * GLES 2 only (a desktop GL 2 compatibility context runs the same shaders
 * when the frontend has no GLES). So far the GPU paints the wall and sky
 * columns and the floor and ceiling spans; everything else is still drawn by the CPU into the software
 * frame, which is uploaded every frame and laid over the GPU columns. The
 * view starts each frame filled with a key colour, which only what the CPU
 * draws after the walls (flats, sprites, masked textures, HUD) covers, so
 * the key is left exactly where the GPU paints.
 */
#ifndef PRBOOMTV_RENDER_GL_H
#define PRBOOMTV_RENDER_GL_H

#include <stddef.h>
#include <stdint.h>
#include "libretro.h"
#include "doomtype.h"
#include "r_draw.h"

/** @brief prboom-render is gl; read in retro_load_game, fixed until the
 * core restarts (SET_HW_RENDER is only accepted there). */
extern dbool dopo_gl;

/**
 * @brief The key colours the CPU leaves in the software frame: [0] where
 * a GPU column shows, [1] where it shows 15/16 as bright, under the fuzz
 * of a Spectre. Both 0 while gl is off.
 */
extern uint16_t dopo_gl_keys[2];

/**
 * @brief The 16-bit fuzz (GETBLENDED16_9406 of r_filter.h, which r_draw.c
 * routes here): the fuzz cannot see the GPU column under a key, so it
 * leaves the darkening to the GPU instead of fuzzing the key into another
 * colour. Unchanged while gl is off (black stays black).
 */
static inline uint16_t dopo_gl_fuzz16(uint16_t col1, uint16_t col2)
{
   if (col1 == dopo_gl_keys[0] || col1 == dopo_gl_keys[1])
      return dopo_gl_keys[1];
   return (uint16_t)(((((col1 & 0xf81f) * 15 + (col2 & 0xf81f)) >> 4) & 0xf81f)
                   | ((((col1 & 0x07e0) * 15 + (col2 & 0x07e0)) >> 4) & 0x07e0));
}

/** @brief The prboom-render core option. */
extern const struct retro_core_option_v2_definition dopo_gl_option;

/**
 * @brief Texture number + 1 of the wall or sky columns being emitted into
 * the draw records (r_drawcmd.c), 0 when they come from something the GPU
 * cannot address (a patch instead of a composite texture). Set by every
 * emitter before it emits.
 */
extern int R_DrawCmdTexture;

/**
 * @brief Reads prboom-render and, for gl, asks the frontend for the GL
 * context. False only when gl was asked and the frontend refused it: the
 * game does not load (no fallback to software).
 */
dbool dopo_gl_load(retro_environment_t environ_cb);

/**
 * @brief Starts replaying the frame's wall records on the GPU, filling the
 * view with the key colour. False when
 * this replay stays in software (gl off, no context yet, or a view drawn
 * off the screen, like the 3D skybox).
 */
dbool dopo_gl_walls_begin(void);

/**
 * @brief One wall or sky column of the replay, queued for the GPU. False
 * when the GPU cannot
 * reproduce it exactly (brightmap, coloured light, unknown texture): the
 * caller draws it in software.
 */
dbool dopo_gl_wall(const draw_column_vars_t *dc, R_DrawColumn_f fn, int gltex);

/** @brief Ends the replay started by dopo_gl_walls_begin. */
void dopo_gl_walls_end(void);

/**
 * @brief Flat lump + 1 of the floor or ceiling spans being emitted, 0 when
 * the GPU cannot reproduce them (a texture on a flat, a slope, dynamic
 * lights). Set by R_DoDrawPlane before it makes the spans.
 */
extern int R_SpanTexture;

/**
 * @brief One floor or ceiling span of the view being replayed, queued for
 * the GPU. False when it stays in software (brightmap, translucent 3D
 * floor, another view, or R_SpanTexture 0).
 */
dbool dopo_gl_span(const draw_span_vars_t *ds, int lump);

/** @brief Software renderer: logs fps and frame time like gl does. */
void dopo_gl_software_frame(void);

/**
 * @brief Presents the frame: the queued GPU columns, then the software
 * frame over them, into the frontend's framebuffer.
 */
void dopo_gl_present(retro_video_refresh_t video_cb, const void *frame,
                     unsigned width, unsigned height, size_t pitch);

#endif
