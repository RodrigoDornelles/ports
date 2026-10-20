/**
 * @file profile.c
 * @brief The frame profile (prboomtv/profile.h): one clock read at each
 * section's start and end, off unless PRBOOM_PROFILE is set.
 */
#include <stdlib.h>
#include <time.h>

#include "doomtype.h"
#include "lprintf.h"
#include "prboomtv/profile.h"

#define DOPO_PROF_FRAMES 300

static int    prof_on = -1;
static double prof_start[DOPO_PROF_COUNT];
static double prof_total[DOPO_PROF_COUNT];
static int    prof_frames;

static double dopo_prof_now(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static int dopo_prof_enabled(void)
{
   if (prof_on < 0)
   {
      const char *env = getenv("PRBOOM_PROFILE");
      prof_on = env && *env && *env != '0';
   }
   return prof_on;
}

void dopo_prof_begin(int id)
{
   if (dopo_prof_enabled())
      prof_start[id] = dopo_prof_now();
}

void dopo_prof_end(int id)
{
   if (dopo_prof_enabled() && prof_start[id] > 0)
   {
      prof_total[id] += dopo_prof_now() - prof_start[id];
      prof_start[id] = 0;
   }
}

void dopo_prof_frame(void)
{
   double *t = prof_total;
   const double n = DOPO_PROF_FRAMES;
   int i;

   if (!dopo_prof_enabled() || ++prof_frames < DOPO_PROF_FRAMES)
      return;
   lprintf(LO_INFO, "prboom-profile (ms per frame): frame %.2f | tics %.2f | "
           "render %.2f (bsp %.2f, walls %.2f, planes %.2f, masked %.2f) | "
           "automap %.2f | status+hud %.2f | menu %.2f | present %.2f | "
           "sound %.2f (music %.2f) | display start %.2f, pages %.2f, hu erase %.2f, "
           "wipe (rest of display) %.2f | other %.2f\n",
           t[DOPO_PROF_FRAME] / n, t[DOPO_PROF_TICS] / n, t[DOPO_PROF_RENDER] / n,
           t[DOPO_PROF_BSP] / n, t[DOPO_PROF_WALLS] / n, t[DOPO_PROF_PLANES] / n,
           t[DOPO_PROF_MASKED] / n, t[DOPO_PROF_AUTOMAP] / n, t[DOPO_PROF_STATUS] / n,
           t[DOPO_PROF_MENU] / n, t[DOPO_PROF_PRESENT] / n, t[DOPO_PROF_SOUND] / n,
           t[DOPO_PROF_MUSIC] / n, t[DOPO_PROF_PRE] / n, t[DOPO_PROF_PAGE] / n,
           t[DOPO_PROF_HUERASE] / n,
           (t[DOPO_PROF_DISPLAY] - t[DOPO_PROF_RENDER] - t[DOPO_PROF_AUTOMAP]
            - t[DOPO_PROF_STATUS] - t[DOPO_PROF_MENU] - t[DOPO_PROF_PRESENT]
            - t[DOPO_PROF_PRE] - t[DOPO_PROF_PAGE] - t[DOPO_PROF_HUERASE]) / n,
           (t[DOPO_PROF_FRAME] - t[DOPO_PROF_TICS] - t[DOPO_PROF_DISPLAY]
            - t[DOPO_PROF_SOUND]) / n);
   for (i = 0; i < DOPO_PROF_COUNT; i++)
      prof_total[i] = 0;
   prof_frames = 0;
}
