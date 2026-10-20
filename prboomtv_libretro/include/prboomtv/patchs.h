/**
 * @brief Patchs toggled from Dopo Options > Patchs, not saved
 * (library/core/patchs/).
 */
#ifndef PRBOOMTV_PATCHS_H
#define PRBOOMTV_PATCHS_H

#include "doomtype.h"

/** @brief Contextual action button (patchs/tv/action_button.c). */
extern dbool dopo_action_button;

/** @brief Auto Fist (patchs/tv/auto_fist.c). */
extern dbool dopo_auto_fist;

/** @brief Toggle Fire (patchs/tv/toggle_fire.c). */
extern dbool dopo_toggle_fire;

/** @brief Side Walk window in milliseconds, 0 = off (patchs/tv/side_walk.c). */
extern int dopo_side_walk_ms;

#endif
