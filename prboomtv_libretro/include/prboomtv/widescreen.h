/**
 * @file widescreen.h
 * @brief The prboom-widescreen core option (library/core/enhancements/widescreen/);
 * the game is always shown in 16:9:
 * - enabled, the default: the 3D view is widened (hor+), while the status
 *   bar, menus and full-screen pages keep their 4:3 proportions,
 *   centered, with the border flat on the sides; the HUD sticks to the
 *   screen edges.
 * - disabled: the classic 4:3 game, centered in the 16:9 frame with the
 *   border flat on the sides.
 *
 * The 2D is drawn on a 320x200 page; in 16:9 the page is the 4:3 area
 * (base_width_43 wide) in the middle of the screen.
 */
#ifndef PRBOOMTV_WIDESCREEN_H
#define PRBOOMTV_WIDESCREEN_H

#include <stddef.h>
#include "libretro.h"
#include "doomtype.h"

/** @brief Patch flags pinning a 2D draw to a screen edge in 16:9. */
#define DOPO_VPT_ALIGN_LEFT  16
#define DOPO_VPT_ALIGN_RIGHT 32

/** @brief The prboom-widescreen option, read at startup. */
extern dbool dopo_widescreen;

/** @brief Width of the 4:3 area for the current height (libretro.c). */
extern int base_width_43;

/**
 * @brief While set (around the HUD), a 2D draw with no alignment sticks
 * to the nearest screen edge in 16:9; a text line decides by where it
 * starts (dopo_hud_line_x), so it never splits between both edges.
 */
extern dbool dopo_hud_align;
extern int dopo_hud_line_x;

/** @brief Adds prboom-widescreen and makes the resolutions 16:9; also adds
 * prboom-render (render_gl.h). */
void dopo_widescreen_options(void);

/** @brief Reads prboom-widescreen. */
void dopo_widescreen_read(retro_environment_t environ_cb);

/** @brief Width the 320 wide page is drawn at. */
int dopo_wide_width(void);

/** @brief Screen x where the page starts, for a draw at page x. */
int dopo_wide_offset(int flags, int x);

/** @brief Page x to screen x, for draws that scale by hand. */
int dopo_wide_x(int x);

/** @brief Page width to screen width. */
int dopo_wide_w(int w);

/**
 * @brief Fills the sides of the page with the border flat: h rows of
 * screen scrn from row y, taken from screen row src_y so the tiles line
 * up with the rest of the screen.
 */
void dopo_fill_wings(int scrn, int y, int src_y, int h);

/** @brief Width of the 16:9 frame the 4:3 game is shown in. */
int dopo_pillar_width(void);

/** @brief The 4:3 frame centered in the 16:9 frame, borders filled. */
const void *dopo_pillarbox(const void *frame, unsigned *width, size_t *pitch);

#endif
