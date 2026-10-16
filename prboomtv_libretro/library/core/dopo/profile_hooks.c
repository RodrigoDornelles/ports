/**
 * @brief The sections of the frame profile (prboomtv/profile.h).
 */

/** @patch libretro/libretro.c 65 */
#include "prboomtv/profile.h"
/* @endpatch */

/** @patch libretro/libretro.c 1793 */
   dopo_prof_begin(DOPO_PROF_FRAME);
/* @endpatch */

/** @patch libretro/libretro.c 1830-1830 */
   dopo_prof_begin(DOPO_PROF_SOUND);
   I_UpdateSound();
   dopo_prof_end(DOPO_PROF_SOUND);
/* @endpatch */

/** @patch libretro/libretro.c 1848 */
   dopo_prof_end(DOPO_PROF_FRAME);
   dopo_prof_frame();
/* @endpatch */

/** @patch libretro/libretro_sound.c 15 */
#include "prboomtv/profile.h"
/* @endpatch */

/** @patch libretro/libretro_sound.c 856-856 */
      dopo_prof_begin(DOPO_PROF_MUSIC);
      current_player->render_float(fmixbuffer, out_frames);
      dopo_prof_end(DOPO_PROF_MUSIC);
/* @endpatch */

/** @patch libretro/libretro_sound.c 863-863 */
         dopo_prof_begin(DOPO_PROF_MUSIC);
         current_player->render(mixbuffer, out_frames);
         dopo_prof_end(DOPO_PROF_MUSIC);
/* @endpatch */

/** @patch src/d_main.c 103 */
#include "prboomtv/profile.h"
/* @endpatch */

/** @patch src/d_main.c 241 */
  dopo_prof_begin(DOPO_PROF_PRE);
/* @endpatch */

/** @patch src/d_main.c 298 */
  dopo_prof_end(DOPO_PROF_PRE);
  dopo_prof_begin(DOPO_PROF_PAGE);
/* @endpatch */

/** @patch src/d_main.c 320 */
    dopo_prof_end(DOPO_PROF_PAGE);
/* @endpatch */

/** @patch src/d_main.c 321-321 */
    dopo_prof_end(DOPO_PROF_PAGE);
    dopo_prof_begin(DOPO_PROF_HUERASE);
    HU_Erase();
    dopo_prof_end(DOPO_PROF_HUERASE);
/* @endpatch */

/** @patch src/d_main.c 375 */
        dopo_prof_begin(DOPO_PROF_RENDER);
/* @endpatch */

/** @patch src/d_main.c 377 */
        dopo_prof_end(DOPO_PROF_RENDER);
        dopo_prof_begin(DOPO_PROF_AUTOMAP);
/* @endpatch */

/** @patch src/d_main.c 379 */
        dopo_prof_end(DOPO_PROF_AUTOMAP);
        dopo_prof_begin(DOPO_PROF_STATUS);
/* @endpatch */

/** @patch src/d_main.c 386 */
        dopo_prof_end(DOPO_PROF_STATUS);
/* @endpatch */

/** @patch src/d_main.c 425 */
  dopo_prof_begin(DOPO_PROF_MENU);
/* @endpatch */

/** @patch src/d_main.c 427 */
  dopo_prof_end(DOPO_PROF_MENU);
/* @endpatch */

/** @patch src/d_main.c 438 */
  dopo_prof_begin(DOPO_PROF_PRESENT);
/* @endpatch */

/** @patch src/d_main.c 446 */
  dopo_prof_end(DOPO_PROF_PRESENT);
/* @endpatch */

/** @patch src/d_main.c 1942-1942 */
   dopo_prof_begin(DOPO_PROF_TICS);
   TryRunTics (); // will run at least one tic
   dopo_prof_end(DOPO_PROF_TICS);
/* @endpatch */

/** @patch src/d_main.c 1954-1954 */
   dopo_prof_begin(DOPO_PROF_DISPLAY);
   D_Display();
   dopo_prof_end(DOPO_PROF_DISPLAY);
/* @endpatch */

/** @patch src/r_main.c 69 */
#include "prboomtv/profile.h"
/* @endpatch */

/** @patch src/r_main.c 1315 */
  dopo_prof_begin(DOPO_PROF_BSP);
/* @endpatch */

/** @patch src/r_main.c 1322 */
  dopo_prof_end(DOPO_PROF_BSP);
  dopo_prof_begin(DOPO_PROF_WALLS);
/* @endpatch */

/** @patch src/r_main.c 1327 */
  dopo_prof_end(DOPO_PROF_WALLS);
/* @endpatch */

/** @patch src/r_main.c 1331 */
    dopo_prof_begin(DOPO_PROF_PLANES);
/* @endpatch */

/** @patch src/r_main.c 1333 */
    dopo_prof_end(DOPO_PROF_PLANES);
/* @endpatch */

/** @patch src/r_main.c 1346 */
    dopo_prof_begin(DOPO_PROF_MASKED);
/* @endpatch */

/** @patch src/r_main.c 1349 */
    dopo_prof_end(DOPO_PROF_MASKED);
/* @endpatch */
