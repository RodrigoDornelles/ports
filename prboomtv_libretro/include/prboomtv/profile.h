/**
 * @file profile.h
 * @brief Where the frame goes (library/core/dopo/profile.c): with
 * PRBOOM_PROFILE=1 in the environment, the time of each part of
 * retro_run is averaged and logged every 300 frames, for the software
 * renderer and for gl alike. Sections may nest; each keeps its own total.
 */
#ifndef PRBOOMTV_PROFILE_H
#define PRBOOMTV_PROFILE_H

enum
{
   DOPO_PROF_FRAME,    /* the whole retro_run */
   DOPO_PROF_TICS,     /* game logic (TryRunTics) */
   DOPO_PROF_DISPLAY,  /* D_Display */
   DOPO_PROF_PRE,      /* D_Display's start: deferred music, wipe start,
                          I_StartDisplay */
   DOPO_PROF_PAGE,     /* intermission, finale, title pages */
   DOPO_PROF_HUERASE,  /* HU_Erase */
   DOPO_PROF_RENDER,   /* R_RenderPlayerView */
   DOPO_PROF_BSP,      /*   BSP walk, segs, sprite projection, sky columns */
   DOPO_PROF_WALLS,    /*   wall and sky replay */
   DOPO_PROF_PLANES,   /*   floors and ceilings */
   DOPO_PROF_MASKED,   /*   sprites, masked textures, weapon */
   DOPO_PROF_AUTOMAP,
   DOPO_PROF_STATUS,   /* status bar and HUD */
   DOPO_PROF_MENU,
   DOPO_PROF_PRESENT,  /* I_FinishUpdate or the wipe */
   DOPO_PROF_SOUND,    /* I_UpdateSound */
   DOPO_PROF_MUSIC,    /*   the music player's render */
   DOPO_PROF_COUNT
};

/** @brief Starts timing a section. */
void dopo_prof_begin(int id);

/** @brief Stops timing a section. */
void dopo_prof_end(int id);

/** @brief End of retro_run: counts the frame, logs every 300. */
void dopo_prof_frame(void);

#endif
