/**
 * @brief prboom-render = gl: the wall and sky records and the floor and
 * ceiling spans go to the GPU
 * (enhancements/gpu/gl.c). Each record carries the texture its column comes from,
 * which the emitters announce in R_DrawCmdTexture; the replay hands every
 * record to dopo_gl_wall and draws in software only what the GPU cannot
 * reproduce.
 */

/**
 * @brief The GPU side of the replay.
 *
 * @patch src/r_drawcmd.c 31
 */
#include "prboomtv/gpu/render_gl.h"
/* @endpatch */

/**
 * @brief Each record keeps the texture of its column, and the wall it is
 * on (its normal, for prboom-dynlight).
 *
 * @patch src/r_drawcmd.c 38
 */
  int                gltex;
  const seg_t       *glseg;
/* @endpatch */

/** @patch src/r_drawcmd.c 67 */
  cmd->gltex = R_DrawCmdTexture;
  cmd->glseg = R_DrawCmdSeg;
/* @endpatch */

/**
 * @brief In gl, the records become GPU columns; the composite locks the
 * GPU side adopted are released with the rest.
 *
 * @patch src/r_drawcmd.c 326
 */
  if (dopo_gl_walls_begin())
  {
    for (i = 0; i < cmd_count; i++)
      if (!dopo_gl_wall(&cmds[i].dc, cmds[i].fn, cmds[i].gltex, cmds[i].glseg))
        cmds[i].fn(&cmds[i].dc);
    dopo_gl_walls_end();
    cmd_count = 0;
    for (i = 0; i < lock_count; i++)
      R_UnlockTextureCompositePatchNum(locks[i]);
    lock_count = 0;
    return;
  }
/* @endpatch */

/** @patch src/r_segs.c 57 */
#include "prboomtv/gpu/render_gl.h"
/* @endpatch */

/**
 * @brief The three wall tiers announce their texture and wall before
 * emitting.
 *
 * @patch src/r_segs.c 1216
 */
         R_DrawCmdTexture = midtexture + 1;
         R_DrawCmdSeg = curline;
/* @endpatch */

/** @patch src/r_segs.c 1260 */
               R_DrawCmdTexture = toptexture + 1;
               R_DrawCmdSeg = curline;
/* @endpatch */

/** @patch src/r_segs.c 1309 */
               R_DrawCmdTexture = bottomtexture + 1;
               R_DrawCmdSeg = curline;
/* @endpatch */

/** @patch src/r_plane.c 75 */
#include "prboomtv/gpu/render_gl.h"
/* @endpatch */

/**
 * @brief Heretic's tall sky is drawn from its patch, which the GPU does
 * not have: software.
 *
 * @patch src/r_plane.c 1291
 */
               R_DrawCmdTexture = 0;
               R_DrawCmdSeg = NULL;
/* @endpatch */

/**
 * @brief The sky from its composite texture.
 *
 * @patch src/r_plane.c 1320
 */
         R_DrawCmdTexture = texture + 1;
         R_DrawCmdSeg = NULL;
/* @endpatch */

/**
 * @brief The sky goes into the wall records with gl even on one render
 * thread, so the GPU paints it: drawn straight into the keyed frame, the
 * CPU's sky never reached the screen (black windows to the sky).
 *
 * @patch src/r_plane.c 1816-1816
 */
  if (R_GetRenderThreads() <= 1 && !dopo_gl)
/* @endpatch */

/**
 * @brief ...and the plane pass skips it, as the threaded path does; only
 * the main view emits it (R_DrawPlanesEmitSky), not a skybox scene.
 *
 * @patch src/r_plane.c 1844
 */
  else if (dopo_gl && !r_in_skybox)
    plane_pass_skip_sky = 1;
/* @endpatch */

/** @patch src/r_plane.c 1853 */
  if (!span_recording)
    plane_pass_skip_sky = 0;
/* @endpatch */

/**
 * @brief The fuzz goes through dopo_gl_fuzz16, which knows the keys.
 *
 * @patch src/r_draw.c 74
 */
#include "prboomtv/gpu/render_gl.h"
#undef GETBLENDED16_9406
#define GETBLENDED16_9406(col1, col2) dopo_gl_fuzz16(col1, col2)
/* @endpatch */

/**
 * @brief Every floor and ceiling span is offered to the GPU first.
 *
 * @patch src/r_plane.c 329
 */
  if (dopo_gl_span(dv, R_SpanTexture))
    return;
/* @endpatch */

/**
 * @brief A plain flat goes to the GPU; a texture on a flat, a slope or a
 * plane under dynamic lights (tinted on the CPU) stays in software.
 *
 * @patch src/r_plane.c 1413
 */
         R_SpanTexture = (R_IsSyntheticFlat(pl->picnum) || tilt_plane
                          || plane_dynlit || plane_wallglow)
                       ? 0 : firstflat + flattranslation[pl->picnum] + 1;
         dopo_gl_plane_begin(planeheight, light);
/* @endpatch */

/** @patch src/r_main.c 68 */
#include "prboomtv/gpu/render_gl.h"
/* @endpatch */

/**
 * @brief The black edge columns of a widened view fill gaps the software
 * fill may leave; in gl the GPU leaves black wherever nothing paints, and
 * drawing them would put CPU pixels in every row of the kept-keyed view.
 *
 * @patch src/r_main.c 1293-1293
 */
    else if (render_aspect != 0 && !dopo_gl)
/* @endpatch */
