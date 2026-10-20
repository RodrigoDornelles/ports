/**
 * @brief A netgame keeps going when its page is hidden: the other players'
 * lockstep waits for this one's input, so a tab in the background would
 * stall them all. Out of netplay the page still stops while hidden, as
 * upstream does, to spare the battery.
 *
 * The browser stops requestAnimationFrame for hidden pages and slows
 * their timers down, so background.js gives the main loop a Web Worker's
 * clock while the page is hidden in a netgame.
 */

/**
 * @brief netplay's state, and background.js
 *
 * @patch frontend/drivers/platform_emscripten.c 57
 */
#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif

extern void dopo_background(bool keep_running);

/* @endpatch */

/**
 * @brief Hidden pages drop their frames, except in a netgame.
 *
 * @patch frontend/drivers/platform_emscripten.c 438-441
 */
bool platform_emscripten_should_drop_iter(void)
{
   bool keep_running = false;

#ifdef HAVE_NETWORKING
   keep_running = netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
   dopo_background(keep_running);
   if (keep_running)
      return emscripten_platform_data->gl_context_lost;
   return (emscripten_platform_data->gl_context_lost || (emscripten_platform_data->window_hidden && emscripten_platform_data->raf_interval));
}
/* @endpatch */
