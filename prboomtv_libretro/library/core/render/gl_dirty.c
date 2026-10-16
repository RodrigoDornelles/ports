/**
 * @brief prboom-render = gl: whatever draws on the frame with the CPU
 * marks the rows it touches (dopo_gl_dirty, render/gl.c), so only those
 * rows of the software frame go to the GPU. Writers that do not mark are
 * still caught, a few frames later, by a sparse comparison with the key.
 */

/** @patch src/v_video.c 50 */
#include "prboomtv/render_gl.h"
/* @endpatch */

/**
 * @brief Copied rectangles.
 *
 * @patch src/v_video.c 287
 */
  if (destscrn == 0)
    dopo_gl_dirty_rect(destx, desty, destx + width, desty + height);
/* @endpatch */

/**
 * @brief Full-screen backgrounds and pages.
 *
 * @patch src/v_video.c 334
 */
  if (scrn == 0)
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/v_video.c 444 */
  dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/v_video.c 983 */
  if (scrn == 0)
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/v_video.c 1095 */
  if (scrn == 0)
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/**
 * @brief Patches: the status bar, the HUD, menus, text.
 *
 * @patch src/v_video.c 690
 */
   if (scrn == 0)
      dopo_gl_dirty_rect(left, top, right, bottom);
/* @endpatch */

/**
 * @brief Filled rectangles.
 *
 * @patch src/v_video.c 1603
 */
  dopo_gl_dirty_rect(x, y, x + width, y + height);
/* @endpatch */

/**
 * @brief Pixels, and the automap lines made of them.
 *
 * @patch src/v_video.c 1674
 */
   if (scrn == 0)
      dopo_gl_dirty_rect(x, y, x + 1, y + 1);
/* @endpatch */

/** @patch src/d_main.c 102 */
#include "prboomtv/render_gl.h"
/* @endpatch */

/**
 * @brief Frames restored whole from a cache.
 *
 * @patch src/d_main.c 371
 */
        dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/d_main.c 603 */
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/**
 * @brief A 3D skybox or line portal laid into the view by the CPU.
 *
 * @patch src/r_main.c 1188
 */
  dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/**
 * @brief In gl the status bar updates only what changed when it can.
 *
 * @patch src/d_main.c 382-382
 */
            dopo_gl_statusbar_refresh(),
/* @endpatch */
