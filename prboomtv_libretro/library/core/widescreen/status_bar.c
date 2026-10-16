/**
 * @brief In 16:9 the status bars stay 4:3 in the middle (widescreen/
 * video.c moves their patches) with the border flat on the sides, and
 * Heretic's fullscreen HUD sticks to the screen edges.
 */

/**
 * @brief The page geometry.
 *
 * @patch src/st_stuff.c 51
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief Doom: the sides of the cached bar get the border flat.
 *
 * @patch src/st_stuff.c 445
 */
          dopo_fill_wings(BG, 0, ST_SCALED_Y, ST_SCALED_HEIGHT);
/* @endpatch */

/**
 * @brief Heretic: the sides of the cached bar get the border flat.
 *
 * @patch src/st_stuff.c 1160
 */
        dopo_fill_wings(BG, 0, bar_dst_y, bar_scaled_h);
/* @endpatch */

/**
 * @brief Hexen's drained vial is a rectangle on the page.
 *
 * @patch src/st_stuff.c 1350-1352
 */
  fx = dopo_wide_x(x);
  fy = y * SCREENHEIGHT / 200;
  fw = dopo_wide_x(x + 3) - fx;
/* @endpatch */

/**
 * @brief Hexen: the bar is drawn every frame, so are the sides below the
 * view.
 *
 * @patch src/st_stuff.c 1440
 */
  dopo_fill_wings(FG, ST_SCALED_Y, ST_SCALED_Y, SCREENHEIGHT - ST_SCALED_Y);
/* @endpatch */

/**
 * @brief Heretic's fullscreen HUD sticks to the screen edges.
 *
 * @patch src/st_stuff.c 1621-1621
 */
    {
      dopo_hud_align = TRUE;
      ST_HereticFullscreenDrawer();
      dopo_hud_align = FALSE;
    }
/* @endpatch */
