/**
 * @brief The prboom-frameskip core option on the frontend side (see
 * prboomtv/frameskip.h); D_DoomLoop asks whether to draw (profile_hooks.c).
 */

/** @patch libretro/libretro.c 71 */
#include "prboomtv/frameskip.h"
/* @endpatch */

/** @patch libretro/libretro.c 1522 */
   dopo_frameskip_read(environ_cb);
/* @endpatch */

/** @patch libretro/libretro.c 1794 */
   dopo_frameskip_begin();
/* @endpatch */

/** @patch libretro/libretro.c 1832 */
   dopo_frameskip_end(video_cb);
/* @endpatch */

/** @patch libretro/libretro.c 2374 */
   dopo_frameskip_load(environ_cb);
/* @endpatch */
