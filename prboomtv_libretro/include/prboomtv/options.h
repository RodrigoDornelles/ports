/**
 * @file options.h
 * @brief Core options from the environment (library/core/dopo/options.c):
 * PRBOOM_RESOLUTION=1280x600 sets prboom-resolution, and so on for every
 * option (upper case, '-' as '_'), ahead of the frontend's value.
 */
#ifndef PRBOOMTV_OPTIONS_H
#define PRBOOMTV_OPTIONS_H

#include "libretro.h"

/** @brief The frontend's environment call, answering options from the
 * environment first. */
retro_environment_t dopo_options_environment(retro_environment_t cb);

#endif
