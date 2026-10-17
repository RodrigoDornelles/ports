/**
 * @file render_gl.h
 * @brief prboom-render = gl (library/core/render/): the software renderer
 * keeps deciding everything, the GPU paints (plan_gpu.txt).
 *
 * GLES 2 only (a desktop GL 2 compatibility context runs the same shaders
 * when the frontend has no GLES). So far the GPU paints the wall and sky
 * columns, the floor and ceiling spans, the sprites and the masked
 * textures; everything else is still drawn by the CPU into the software
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
#include "r_patch.h"

/** @brief prboom-render is gl; read in retro_load_game, fixed until the
 * core restarts (SET_HW_RENDER is only accepted there). */
extern dbool dopo_gl;

/**
 * @brief The key colours the CPU leaves in the software frame: [0] where
 * the GPU shows, [1] where it shows darkened, under the fuzz of a Spectre.
 * Both 0 while gl is off.
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

/** @brief The prboom-spectre core option (gl only): how fuzzy things look. */
extern const struct retro_core_option_v2_definition dopo_gl_spectre_option;

/** @brief The prboom-spectre-opacity core option (gl only): how strong. */
extern const struct retro_core_option_v2_definition dopo_gl_spectre_opacity_option;

/** @brief Reads prboom-spectre and prboom-spectre-opacity (startup and
 * every change). */
void dopo_gl_spectre_read(retro_environment_t environ_cb);

/** @brief (see above) */
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
 * @brief R_DoDrawPlane starts making a plane into spans: its height over
 * the view and its light level (the zlight row), for the rectangles the
 * GPU makes of its spans.
 */
void dopo_gl_plane_begin(fixed_t height, int level);

/**
 * @brief One floor or ceiling span of the view being replayed, queued for
 * the GPU. False when it stays in software (brightmap, translucent 3D
 * floor, another view, or R_SpanTexture 0).
 */
dbool dopo_gl_span(const draw_span_vars_t *ds, int lump);

/**
 * @brief A sprite (lump of its patch, texnum -1) or a two-sided masked
 * texture (texnum, lump -1) is about to be drawn post by post through
 * R_DrawMaskedColumn; plain says it is opaque, without brightmap, tint or
 * sloped edges; fuzz that it is a Spectre-like shadow, drawn in the
 * prboom-spectre look; x1..x2 its columns. True when the GPU takes its posts
 * (dopo_gl_masked_post),
 * so the caller must use the R_DrawMaskedColumn path for every column.
 */
dbool dopo_gl_masked_begin(const rpatch_t *patch, int lump, int texnum,
                           R_DrawColumn_f colfunc, dbool plain, dbool fuzz,
                           int x1, int x2);

/** @brief The masked draw started by dopo_gl_masked_begin is done. */
void dopo_gl_masked_end(void);

/**
 * @brief One clipped post from R_DrawMaskedColumn, queued for the GPU in
 * drawing order. False when the CPU draws it.
 */
dbool dopo_gl_masked_post(const draw_column_vars_t *dc, R_DrawColumn_f fn);

/**
 * @brief Rows y0..y1 of the frame (screen 0) were drawn on by the CPU:
 * the GPU takes them from the software frame (render/gl.c, view_keyed).
 * Called by the 2D drawing (v_video.c) and whatever else writes the frame.
 */
void dopo_gl_dirty(int y0, int y1);

/** @brief Pixels [x0, x1) of rows [y0, y1) were drawn on (as above). */
void dopo_gl_dirty_rect(int x0, int y0, int x1, int y1);

/** @brief Whether the status bar redraws whole this frame (d_main.c). */
dbool dopo_gl_statusbar_refresh(void);

/** @brief Software renderer: logs fps and frame time like gl does. */
void dopo_gl_software_frame(void);

/**
 * @brief Presents the frame: the queued GPU columns, then the software
 * frame over them, into the frontend's framebuffer.
 */
void dopo_gl_present(retro_video_refresh_t video_cb, const void *frame,
                     unsigned width, unsigned height, size_t pitch);

#endif
