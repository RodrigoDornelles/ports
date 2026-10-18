/**
 * @file lightcolor.c
 * @brief Coloured sector light (prboomtv/gpu/lightcolor.h).
 *
 * The UDMF parser (udmf.c) learns the sector key lightcolor and keeps it
 * here, per sector, for gl.c; nothing else reads it, so the software
 * renderer draws as before. The floor and ceiling a sector adds to the
 * view carry their colour in their visplane's skybox field, which only sky
 * planes use: two planes merge only when it matches, and it goes along
 * when a plane is split. The module is a hunk at the top of udmf.c.
 */

/**
 * @brief The module: the colours per sector.
 *
 * @patch src/udmf.c 18
 */
#include <stdint.h>
#include <stdlib.h>

#include "prboomtv/gpu/lightcolor.h"

static uint32_t *dopo_lightcolors = NULL;
static int       dopo_lightcolors_n = 0, dopo_lightcolors_cap = 0;

/* a new map's colours (the parse begins) */
static void dopo_lightcolor_clear(void)
{
   dopo_lightcolors_n = 0;
}

/* sector index's colour, the ones before it untinted */
static void dopo_lightcolor_set(int index, int color)
{
   if (index < 0)
      return;
   if (index >= dopo_lightcolors_cap)
   {
      int cap = dopo_lightcolors_cap ? dopo_lightcolors_cap : 256;
      uint32_t *n;
      while (cap <= index)
         cap *= 2;
      n = (uint32_t *)realloc(dopo_lightcolors, sizeof(*n) * cap);
      if (!n)
         return;
      dopo_lightcolors = n;
      dopo_lightcolors_cap = cap;
   }
   while (dopo_lightcolors_n <= index)
      dopo_lightcolors[dopo_lightcolors_n++] = 0xffffff;
   dopo_lightcolors[index] = (uint32_t)color & 0xffffff;
}

uint32_t dopo_sector_lightcolor(int secnum)
{
   return secnum >= 0 && secnum < dopo_lightcolors_n ? dopo_lightcolors[secnum] : 0xffffff;
}
/* @endpatch */

/** @patch src/udmf.c 457 */
    else if (scanner_string_match(s, "lightcolor"))
    {
      int color = 0xffffff;
      SCAN_INT(color);
      dopo_lightcolor_set((int)udmf_sectors.count, color);
    }
/* @endpatch */

/** @patch src/udmf.c 576 */
  dopo_lightcolor_clear();
/* @endpatch */

/** @patch src/r_bsp.c 50 */
#include "prboomtv/gpu/lightcolor.h"
/* @endpatch */

/**
 * @brief A floor carries its sector's colour (prboomtv/gpu/lightcolor.h).
 *
 * @patch src/r_bsp.c 626-626
 */
                frontsector->floorpic == skyflatnum ? frontsector->skybox
                                                    : dopo_gl_plane_tag(r_real_secnum),
/* @endpatch */

/** @patch src/r_bsp.c 646-646 */
                frontsector->ceilingpic == skyflatnum ? frontsector->skybox
                                                      : dopo_gl_plane_tag(r_real_secnum),
/* @endpatch */
