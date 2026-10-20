/**
 * @file frameskip.h
 * @brief The prboom-frameskip core option (library/core/dopo/frameskip.c):
 * frames whose drawing is skipped still run the game (tics, input,
 * sound), so the game keeps its speed and the sound stays whole when the
 * device cannot draw every frame; the frontend shows the last frame again.
 * - 0, the default: every frame is drawn.
 * - 1 / 2: one frame drawn, then 1 / 2 skipped.
 */
#ifndef PRBOOMTV_FRAMESKIP_H
#define PRBOOMTV_FRAMESKIP_H

#include "libretro.h"
#include "doomtype.h"

/** @brief The option. */
extern const struct retro_core_option_v2_definition dopo_frameskip_option;

/** @brief At load: whether the frontend can show a frame again. */
void dopo_frameskip_load(retro_environment_t environ_cb);

/** @brief Reads the option (startup and every change). */
void dopo_frameskip_read(retro_environment_t environ_cb);

/** @brief Start of retro_run. */
void dopo_frameskip_begin(void);

/** @brief Whether this frame is drawn (D_DoomLoop). */
dbool dopo_frameskip_draw(void);

/** @brief End of retro_run: a skipped frame shows the last one again. */
void dopo_frameskip_end(retro_video_refresh_t video_cb);

#endif
