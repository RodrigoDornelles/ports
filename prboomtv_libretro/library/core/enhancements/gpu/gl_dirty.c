/**
 * @brief prboom-render = gl: the 2D drawing (v_video.c) first offers each
 * draw on screen 0 to the GPU (enhancements/gpu/gl.c, "2D"); what the CPU
 * still draws on it marks the rectangles it touches (dopo_gl_dirty), so
 * only those go to the GPU from the software frame. Writers that do not
 * mark are still caught, a few frames later, by a sparse comparison with
 * the key. Cached full-screen images become GPU images sent once.
 */

/** @patch src/v_video.c 50 */
#include "prboomtv/render_gl.h"
/* @endpatch */

/**
 * @brief Copied rectangles: from a back screen (the status bar's), a copy
 * of its GPU mirror.
 *
 * @patch src/v_video.c 287
 */
  if (destscrn == 0)
  {
    if (srcscrn != 0 && dopo_gl_2d_copy(srcscrn, srcx, srcy, width, height, destx, desty))
      return;
    dopo_gl_dirty_rect(destx, desty, destx + width, desty + height);
  }
  else
    dopo_gl_2d_screen_dirty(destscrn);
/* @endpatch */

/**
 * @brief Full-screen backgrounds: the cached one is a GPU image; one built
 * is drawn by the CPU.
 *
 * @patch src/v_video.c 334
 */
  if (scrn != 0)
    dopo_gl_2d_screen_dirty(scrn);
/* @endpatch */

/** @patch src/v_video.c 368-368 */
    if (scrn == 0 && dopo_gl_2d_image_full(bg_cache))
      return;
    if (scrn == 0)
      dopo_gl_dirty(0, SCREENHEIGHT - 1);
    memcpy(screens[scrn].data, bg_cache, surf_bytes);
/* @endpatch */

/** @patch src/v_video.c 390 */
  if (scrn == 0)
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/v_video.c 417 */
      dopo_gl_2d_image_dirty(bg_cache);
/* @endpatch */

/** @patch src/v_video.c 444 */
  dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/** @patch src/v_video.c 983 */
  if (scrn == 0)
    dopo_gl_dirty(0, SCREENHEIGHT - 1);
  else
    dopo_gl_2d_screen_dirty(scrn);
/* @endpatch */

/**
 * @brief Cached full-screen pages (title, intermission): the cache is a
 * GPU image; while it is built the CPU draws, so it can be kept.
 *
 * @patch src/v_video.c 1110-1110
 */
    if (scrn == 0 && dopo_gl_2d_image_full(fs_cache))
      return;
    if (scrn == 0)
      dopo_gl_dirty(0, SCREENHEIGHT - 1);
    else
      dopo_gl_2d_screen_dirty(scrn);
    memcpy(screens[scrn].data, fs_cache, surf_bytes);
/* @endpatch */

/** @patch src/v_video.c 1116 */
  dopo_gl_2d_suspend(TRUE);
/* @endpatch */

/** @patch src/v_video.c 1127 */
  dopo_gl_2d_suspend(FALSE);
/* @endpatch */

/** @patch src/v_video.c 1139 */
      dopo_gl_2d_image_dirty(fs_cache);
/* @endpatch */

/**
 * @brief Patches: the status bar, the HUD, menus, text; the GPU draws a
 * patch on screen 0 as one quad.
 *
 * @patch src/v_video.c 690
 */
   if (scrn == 0 && dopo_gl_2d_patch(patch, left, top, right, bottom, DXI, DYI,
                                     (flags & VPT_FLIP) != 0,
                                     (flags & VPT_TRANS) ? trans : NULL))
   {
      drawvars.short_topleft = old_short_topleft;
      drawvars.int_topleft   = old_int_topleft;
      return;
   }
   if (scrn == 0)
      dopo_gl_dirty_rect(left, top, right, bottom);
   else
      dopo_gl_2d_screen_dirty(scrn);
/* @endpatch */

/**
 * @brief Filled rectangles.
 *
 * @patch src/v_video.c 1603
 */
  if (dopo_gl_2d_fill(x, y, width, height, colour))
    return;
  dopo_gl_dirty_rect(x, y, x + width, y + height);
/* @endpatch */

/**
 * @brief Pixels, and the automap lines made of them (still the CPU's).
 *
 * @patch src/v_video.c 1674
 */
   if (scrn == 0)
      dopo_gl_dirty_rect(x, y, x + 1, y + 1);
   else
      dopo_gl_2d_screen_dirty(scrn);
/* @endpatch */

/** @patch src/d_main.c 102 */
#include "prboomtv/render_gl.h"
/* @endpatch */

/**
 * @brief The frame begins: the GPU takes the 2D from here.
 *
 * @patch src/d_main.c 297
 */
  dopo_gl_frame_begin();
/* @endpatch */

/**
 * @brief No frozen frame in gl: the GPU draws what screen 0 no longer
 * holds, so a copy of screen 0 would miss it.
 *
 * @patch src/d_main.c 358-358
 */
        && !(automapmode & am_active) && !dopo_gl;  /* the automap can still pan/zoom */
/* @endpatch */

/**
 * @brief Frames restored whole from a cache.
 *
 * @patch src/d_main.c 371
 */
        dopo_gl_dirty(0, SCREENHEIGHT - 1);
/* @endpatch */

/**
 * @brief The demo pages: the cache is a GPU image, sent when rebuilt.
 *
 * @patch src/d_main.c 590
 */
        dopo_gl_2d_image_dirty(page_cache);
/* @endpatch */

/** @patch src/d_main.c 603-603 */
    if (!dopo_gl_2d_image_full(page_cache))
    {
      dopo_gl_dirty(0, SCREENHEIGHT - 1);
      memcpy(screens[0].data, page_cache, fb_bytes);
    }
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
