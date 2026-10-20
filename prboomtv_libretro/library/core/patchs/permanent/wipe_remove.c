/**
 * @brief No screen melt: a new level, the intermission or the title comes
 * in with a cut. The melt drew the whole screen on the CPU for a second at
 * every level change, which the gl renderer could not even reproduce (it
 * melted the key colour into black), and kept a copy of the frame around
 * every transition just in case one started.
 */

/**
 * @brief The melt never starts.
 *
 * @patch src/d_main.c 287-293
 */
  wipe = 0;
/* @endpatch */

/**
 * @brief No frame is kept for a melt that will not come (a 4 MB copy per
 * frame at 1080p around every transition, under direct render).
 *
 * @patch libretro/libretro.c 3850-3850
 */
   return 0;
/* @endpatch */
