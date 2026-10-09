/**
 * @brief The frontend's environment call, for the rooms (multiplayer/
 * rooms.c), which live outside libretro.c where environ_cb is kept.
 */

/**
 * @brief Environment call from outside libretro.c.
 *
 * @patch libretro/libretro.c 908
 */
#include "prboomtv/rooms.h"

dbool dopo_environment(unsigned cmd, void *data)
{
   return environ_cb && environ_cb(cmd, data);
}

/* @endpatch */
