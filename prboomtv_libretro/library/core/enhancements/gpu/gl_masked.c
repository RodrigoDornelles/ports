/**
 * @brief prboom-render = gl: sprites, the player's weapon and two-sided
 * masked textures go to the GPU post by post (enhancements/gpu/gl.c). Every post is
 * clipped by R_DrawMaskedColumn exactly as before; there the GPU takes it
 * instead of the column drawer. A sprite the GPU takes skips the direct
 * framebuffer paths, which bypass R_DrawMaskedColumn.
 */

/** @patch src/r_things.c 55 */
#include "prboomtv/render_gl.h"
/* @endpatch */

/**
 * @brief The post goes to the GPU when the thing being drawn is on it.
 *
 * @patch src/r_things.c 595-595
 */
      if (!dopo_gl_masked_post(dcvars, colfunc))
         colfunc (dcvars);
/* @endpatch */

/**
 * @brief A plain sprite (opaque, no brightmap, tint, sloped edges or
 * Raven shadow blend) is offered to the GPU; a fuzzy one too, drawn in
 * the prboom-spectre look.
 *
 * @patch src/r_things.c 1211
 */
  const dbool gl_sprite = dopo_gl_masked_begin(patch, vis->patch + firstspritelump, -1, colfunc,
        !sprmask && dcvars.edgetype != RDRAW_MASKEDCOLUMNEDGE_SLOPED && !vis->translucent
        && !(vis->tint_r | vis->tint_g | vis->tint_b)
        && !(raven && (vis->mobjflags & (MF_SHADOW | MF_ALTSHADOW)) && vis->colormap),
        !dcvars.colormap, vis->x1, vis->x2);
/* @endpatch */

/**
 * @brief On the GPU, every column goes through R_DrawMaskedColumn.
 *
 * @patch src/r_things.c 1219-1219
 */
    if (!gl_sprite && (run_cls == 1 || run_cls == 2) &&
/* @endpatch */

/** @patch src/r_things.c 1355 */
  dopo_gl_masked_end();
/* @endpatch */

/**
 * @brief A two-sided masked texture's columns, unless the line is
 * translucent (the water path above stays in software).
 *
 * @patch src/r_segs.c 479
 */
   dopo_gl_masked_begin(patch, -1, texnum, colfunc, !curline->linedef->translucent, FALSE, x1, x2);
/* @endpatch */

/** @patch src/r_segs.c 557 */
   dopo_gl_masked_end();
/* @endpatch */
