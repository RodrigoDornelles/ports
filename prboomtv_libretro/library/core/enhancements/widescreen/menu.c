/**
 * @brief Menus and finale in 16:9: what they draw by hand goes on the
 * 4:3 page too, and the aspect is the prboom-ratio core option's.
 */

/**
 * @brief The page geometry.
 *
 * @patch src/m_menu.c 46
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief The colour chip is drawn on the page.
 *
 * @patch src/m_menu.c 2348-2353
 */
      V_FillRect(dopo_wide_x(x), (y-1)*SCREENHEIGHT/200,
                    dopo_wide_w(8), 8*SCREENHEIGHT/200,
                 PAL_BLACK);
      V_FillRect(dopo_wide_x(x+1), y*SCREENHEIGHT/200,
                        dopo_wide_w(6), 6*SCREENHEIGHT/200,
                 (uint8_t)ch);
/* @endpatch */

/**
 * @brief The text cursor is drawn on the page.
 *
 * @patch src/m_menu.c 2404-2405
 */
      V_FillRect(dopo_wide_x(x+cursor_start-1), (y*SCREENHEIGHT)/200,
      dopo_wide_w(char_width), 9*SCREENHEIGHT/200, PAL_WHITE);
/* @endpatch */

/**
 * @brief The aspect is the prboom-ratio core option, not a menu item.
 *
 * @patch src/m_menu.c 3350-3351
 */
  /* Aspect Ratio: the prboom-ratio core option */
/* @endpatch */

/**
 * @brief The page geometry.
 *
 * @patch src/f_finale.c 39
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief The bunny scrolls on the page, the border flat around it.
 *
 * @patch src/f_finale.c 694
 */
  dopo_fill_wings(0, 0, 0, SCREENHEIGHT);
/* @endpatch */
