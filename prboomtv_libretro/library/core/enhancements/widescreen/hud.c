/**
 * @brief In 16:9 the HUD (messages, automap title and coordinates, the
 * fullscreen HUD widgets) sticks to the screen edges, each line to the
 * edge nearest to where it starts on the page.
 */

/**
 * @brief The page geometry.
 *
 * @patch src/hu_lib.c 37
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief The whole line goes to the edge its start is nearest to.
 *
 * @patch src/hu_lib.c 172
 */
  dopo_hud_line_x = x;
/* @endpatch */

/**
 * @brief Done with the line.
 *
 * @patch src/hu_lib.c 239
 */
  dopo_hud_line_x = -1;
/* @endpatch */

/**
 * @brief The page geometry.
 *
 * @patch src/d_main.c 69
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief The HUD sticks to the screen edges.
 *
 * @patch src/d_main.c 384-384
 */
        dopo_hud_align = TRUE;
        HU_Drawer();
        dopo_hud_align = FALSE;
/* @endpatch */
