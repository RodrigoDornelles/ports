/**
 * @brief The levels a netgame is set up on: the maps the loaded wads have,
 * each with the modes its starts are for. Coop wants a player 2 to 4 start
 * (Hexen's 5 to 8 too), deathmatch a deathmatch start; a map with neither
 * takes coop, its players spawning beside player 1's start.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "doomstat.h"
#include "doomdata.h"
#include "w_wad.h"
#include "prboomtv/multiplayer.h"

#define DOPO_LEVELS 128

static dopo_mp_level_t   dopo_levels[DOPO_LEVELS];
static int               dopo_levels_n;
static const lumpinfo_t *dopo_levels_wads;
static int               dopo_levels_lumps = -1;

static dbool dopo_level_name(const char *lump, int *episode, int *map)
{
  char n[9];

  memcpy(n, lump, 8);
  n[8] = 0;
  if (gamemode == commercial || hexen)
  {
    *episode = 1;
    *map = atoi(n + 3);
    return !strncasecmp(n, "MAP", 3) && isdigit((uint8_t)n[3]) &&
           isdigit((uint8_t)n[4]) && !n[5] && *map >= 1;
  }
  *episode = n[1] - '0';
  *map = n[3] - '0';
  return toupper((uint8_t)n[0]) == 'E' && toupper((uint8_t)n[2]) == 'M' &&
         *episode >= 1 && *episode <= 9 && *map >= 1 && *map <= 9 && !n[4];
}

static void dopo_level_start(dopo_mp_level_t *level, long type)
{
  if ((type >= 2 && type <= 4) || (type >= 9100 && type <= 9103))
    level->coop = TRUE;
  else if (type == 11)
    level->dm = TRUE;
}

/* UDMF: the type of every thing block of the TEXTMAP */
static void dopo_level_textmap(dopo_mp_level_t *level, const char *text, int size)
{
  const char *p = text, *end = text + size;

  while (p < end)
  {
    const char *block;

    if (end - p < 5 || strncasecmp(p, "thing", 5) || (p > text && isalnum((uint8_t)p[-1])))
    {
      p++;
      continue;
    }
    for (p += 5; p < end && isspace((uint8_t)*p); p++);
    if (p >= end || *p != '{')
      continue;
    for (block = ++p; p < end && *p != '}'; p++)
      if (end - p > 4 && !strncasecmp(p, "type", 4) && !isalnum((uint8_t)p[4]) &&
          (p == block || !isalnum((uint8_t)p[-1])))
      {
        const char *v = p + 4;

        while (v < end && (isspace((uint8_t)*v) || *v == '='))
          v++;
        if (v < end && isdigit((uint8_t)*v))
          dopo_level_start(level, strtol(v, NULL, 10));
      }
  }
}

static void dopo_level_things(dopo_mp_level_t *level, int marker)
{
  const int lump = marker + ML_THINGS;
  const int size = W_LumpLength(lump);
  const uint8_t *data = W_CacheLumpNum(lump);
  int record = 10, at = 6, i;

  if (!strncasecmp(lumpinfo[lump].name, "TEXTMAP", 8))
    dopo_level_textmap(level, (const char *)data, size);
  else
  {
    /* Hexen's records, a BEHAVIOR after the BLOCKMAP telling them apart */
    if (marker + ML_BLOCKMAP + 1 < numlumps &&
        !strncasecmp(lumpinfo[marker + ML_BLOCKMAP + 1].name, "BEHAVIOR", 8))
      record = 20, at = 10;
    for (i = 0; i + record <= size; i += record)
      dopo_level_start(level, (short)(data[i + at] | data[i + at + 1] << 8));
  }
  W_UnlockLumpNum(lump);
  if (!level->coop && !level->dm)
    level->coop = TRUE;
}

const dopo_mp_level_t *dopo_mp_levels(int *count)
{
  if (dopo_levels_wads != lumpinfo || dopo_levels_lumps != numlumps)
  {
    static int markers[10][100];
    int i, e, m;

    dopo_levels_wads = lumpinfo;
    dopo_levels_lumps = numlumps;
    memset(markers, 0, sizeof(markers));
    /* a map is its name followed by its THINGS or TEXTMAP; the last wins */
    for (i = 0; i + 1 < numlumps; i++)
      if ((!strncasecmp(lumpinfo[i + 1].name, "THINGS", 8) ||
           !strncasecmp(lumpinfo[i + 1].name, "TEXTMAP", 8)) &&
          dopo_level_name(lumpinfo[i].name, &e, &m) && m < 100)
        markers[e][m] = i + 1;
    dopo_levels_n = 0;
    for (e = 1; e < 10; e++)
      for (m = 1; m < 100 && dopo_levels_n < DOPO_LEVELS; m++)
        if (markers[e][m])
        {
          dopo_mp_level_t *level = &dopo_levels[dopo_levels_n++];

          memset(level, 0, sizeof(*level));
          level->episode = e;
          level->map = m;
          dopo_level_things(level, markers[e][m] - 1);
        }
    /* Hexen's hubs only start from the first map for now */
    if (hexen && dopo_levels_n > 1)
      dopo_levels_n = 1;
  }
  *count = dopo_levels_n;
  return dopo_levels;
}

static dbool dopo_level_takes(const dopo_mp_level_t *level, int mode)
{
  return mode == DOPO_MP_COOP ? level->coop : level->dm;
}

dbool dopo_mp_mode_offered(int mode)
{
  int n, i;
  const dopo_mp_level_t *levels = dopo_mp_levels(&n);

  for (i = 0; i < n; i++)
    if (dopo_level_takes(&levels[i], mode))
      return TRUE;
  return FALSE;
}

void dopo_mp_levels_step(dopo_mp_config_t *c, int step)
{
  int n, i, k, at = -1;
  const dopo_mp_level_t *levels = dopo_mp_levels(&n);

  for (i = 0; i < n; i++)
    if (levels[i].episode == c->episode && levels[i].map == c->map)
      at = i;
  if (at < 0)
    at = step > 0 ? n - 1 : 0;
  for (k = 1; k <= n; k++)
  {
    const dopo_mp_level_t *level = &levels[((at + step * k) % n + n) % n];

    if (dopo_level_takes(level, c->mode))
    {
      c->episode = level->episode;
      c->map = level->map;
      return;
    }
  }
}

void dopo_mp_levels_fit(dopo_mp_config_t *c)
{
  int n, i;
  const dopo_mp_level_t *levels = dopo_mp_levels(&n);

  if (!n)
    return;
  if (!dopo_mp_mode_offered(c->mode))
    c->mode = c->mode == DOPO_MP_COOP ? DOPO_MP_DEATHMATCH : DOPO_MP_COOP;
  for (i = 0; i < n; i++)
    if (levels[i].episode == c->episode && levels[i].map == c->map &&
        dopo_level_takes(&levels[i], c->mode))
      return;
  dopo_mp_levels_step(c, 1);
}
