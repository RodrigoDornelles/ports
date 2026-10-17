/**
 * @file dynlights.h
 * @brief The prboom-dynlight core option (library/core/enhancements/gpu/hd/dynlights.c),
 * gl only: coloured lights from fireballs, explosions, the muzzle flash,
 * torches and lamps, lighting the walls, floors, ceilings and sprites
 * near them, as GZDoom's dynamic lights. The lights come from GLDEFS: a
 * small built-in set for Doom (gpu/gldefs/doom1.txt), GZDoom's lights.pk3
 * when it is next to the game, next to the core or in the system
 * directory, and the wads' own GLDEFS.
 * - 0: off.
 * - 1: what moves (projectiles, explosions, attacks).
 * - 2, the default: everything, the torches, lamps and items too.
 */
#ifndef PRBOOMTV_DYNLIGHTS_H
#define PRBOOMTV_DYNLIGHTS_H

#include <stddef.h>
#include <stdint.h>

#include "libretro.h"

/** @brief The option. */
extern const struct retro_core_option_v2_definition dopo_dynlight_option;

/** @brief The prboom-dynlight in effect, 0..2. */
extern int dopo_dynlight;

/** @brief Reads the option (startup and every change), and where to look
 * for lights.pk3. */
void dopo_dynlight_read(retro_environment_t environ_cb);

/** @brief The prboom-9800 core option: the lights a map places itself. */
extern const struct retro_core_option_v2_definition dopo_maplight_option;

/** @brief prboom-9800 in effect. */
extern int dopo_maplight;

/** @brief A new map: its lights are gathered again (P_SetupLevel). */
void dopo_maplight_clear(void);

/**
 * @brief A map thing of editor number type at (x, y), facing angle, with
 * its Hexen-format args and height in hexen_thing_args/_height: true when
 * it is a light (9800 and up), which spawns nothing.
 */
int dopo_maplight_add(int x, int y, int angle, int type);

/**
 * @brief A file of GZDoom's (lights.pk3, brightmaps.pk3), the first found
 * next to the game, next to the core, in the system directory or its
 * prboom folder: its bytes, which the caller frees, and where it was;
 * NULL when nowhere.
 */
void *dopo_resource_read(const char *file, int64_t *len, char *where, size_t where_size);

/** @brief The folder of GZDoom's filter/ this game reads (doom.id...). */
const char *dopo_resource_game(void);

/** @brief One member of a pk3 by its path, malloc'd; NULL if missing. */
unsigned char *W_PK3ExtractPath(const unsigned char *zip, int64_t zip_length,
                                const char *path, int *out_length);

/** @brief The most lights a frame keeps (the nearest). */
#define DOPO_LIGHTS_MAX 8

/** @brief A light of the frame, relative to the view (map units). */
typedef struct
{
   float x, y, z;      /* from the eye */
   float radius;
   float r, g, b;      /* 0..1 */
} dopo_light_t;

/**
 * @brief The lights of this frame near the view, nearest first, at most
 * max; 0 when the option is off or nothing glows.
 */
int dopo_dynlight_collect(dopo_light_t *out, int max);

#endif
