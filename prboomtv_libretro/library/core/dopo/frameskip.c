/**
 * @file frameskip.c
 * @brief The prboom-frameskip core option (prboomtv/frameskip.h).
 *
 * Each retro_run advances the game by one frame of its frame rate,
 * whether or not the frame is drawn: libretro moves time by frames, not by
 * the clock, so a device that cannot draw every frame would otherwise run
 * the game slow and starve the sound. Drawing is most of a frame; the game
 * logic and the sound are a few milliseconds, so drawing one frame in two
 * or three lets a slow device keep up.
 */
#include <string.h>

#include "doomdef.h"
#include "prboomtv/frameskip.h"
#include "prboomtv/widescreen.h"

const struct retro_core_option_v2_definition dopo_frameskip_option =
{
   "prboom-frameskip",
   "Frame Skip",
   NULL,
   "Draw only one frame in two or three, so a device that cannot draw "
   "them all keeps the game at its speed and the sound whole; the game "
   "itself still runs every frame. At 60fps on a 60Hz TV, '1' gives an "
   "even 30 frames a second and '2' an even 20.",
   NULL,
   NULL,
   {
      { "0", "Off" },
      { "1", "1 (draw 1 of 2)" },
      { "2", "2 (draw 1 of 3)" },
      { NULL, NULL },
   },
   "0"
};

static int   skip_mode = 0;    /* frames skipped after each drawn one */
static dbool can_dupe = FALSE;
static int   counter = 0;      /* frames since the last drawn one */
static dbool skipped = FALSE;  /* this frame was not drawn */

void dopo_frameskip_load(retro_environment_t environ_cb)
{
   bool dupe = false;
   can_dupe = environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &dupe) && dupe;
}

void dopo_frameskip_read(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-frameskip", NULL };

   skip_mode = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
         && (!strcmp(var.value, "1") || !strcmp(var.value, "2")))
      skip_mode = var.value[0] - '0';
   counter = 0;
}

void dopo_frameskip_begin(void)
{
   skipped = FALSE;
}

dbool dopo_frameskip_draw(void)
{
   skipped = can_dupe && skip_mode && counter != 0;
   return !skipped;
}

void dopo_frameskip_end(retro_video_refresh_t video_cb)
{
   if (skip_mode)
      counter = (counter + 1) % (skip_mode + 1);
   if (skipped && video_cb)
      video_cb(NULL, dopo_widescreen ? SCREENWIDTH : dopo_pillar_width(),
               SCREENHEIGHT, 0);
}
