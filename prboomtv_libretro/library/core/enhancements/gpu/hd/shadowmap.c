/**
 * @file shadowmap.c
 * @brief Light shadows, prboom-dynlight=3 (prboomtv/gpu/shadowmap.h).
 *
 * For a light, the lines near it come from the blockmap; each that stops
 * light (one-sided, or two-sided with no opening: a closed door) is drawn
 * into the light's directions, keeping in each the nearest distance where
 * the ray meets it. A light that does not move keeps its row from frame
 * to frame, and one of them is made again each frame in turn, so a door
 * that opens lets the light through a few frames later; a light that moves
 * (a fireball, a flash) misses the cache and is made every frame.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "r_main.h"
#include "r_state.h"
#include "w_wad.h"
#include "prboomtv/gpu/render_gl.h"
#include "prboomtv/gpu/shadowmap.h"

int dopo_shadowmap_on(void)
{
   return dopo_dynlight == 3;
}

/* whether the lights cast shadows */
static int sm_on(void)
{
   return dopo_shadowmap_on();
}

/* the directions, once */
static float sm_cos[DOPO_SHADOW_RAYS], sm_sin[DOPO_SHADOW_RAYS];
static int   sm_dirs_made = 0;

/* lights kept: where (map units), how far, and their row */
#define SM_CACHE 64
static struct
{
   float    x, y, r;
   unsigned used;        /* the frame it was last asked for */
   unsigned made;        /* the frame it was made */
   uint16_t row[DOPO_SHADOW_RAYS];
} sm_cache[SM_CACHE];
static unsigned    sm_frame = 0;
static lumpinfo_t *sm_owner = NULL;
static const line_t *sm_lines_owner = NULL;

/* the frame's lights, for dopo_shadowmap_lit */
static const uint16_t *sm_rows[DOPO_LIGHTS_MAX];
static float           sm_lx[DOPO_LIGHTS_MAX], sm_ly[DOPO_LIGHTS_MAX], sm_lr[DOPO_LIGHTS_MAX];
static int             sm_n = 0;

/* the light being made */
static float  sm_x, sm_y, sm_r;
static float  sm_dist[DOPO_SHADOW_RAYS];

static int sm_angle_index(double a)
{
   int k = (int)floor(a / (2 * M_PI) * DOPO_SHADOW_RAYS);
   k %= DOPO_SHADOW_RAYS;
   return k < 0 ? k + DOPO_SHADOW_RAYS : k;
}

/* a line that stops light: one-sided, or two-sided with no opening */
static int sm_blocks(const line_t *ld)
{
   const sector_t *f = ld->frontsector, *b = ld->backsector;
   if (!f || !b)
      return 1;
   return b->ceilingheight <= f->floorheight || b->floorheight >= f->ceilingheight
       || b->ceilingheight <= b->floorheight || f->ceilingheight <= f->floorheight;
}

/* the line, into the directions it covers from the light */
static dbool sm_line(line_t *ld)
{
   const double ax = ld->v1->x / 65536.0 - sm_x, ay = ld->v1->y / 65536.0 - sm_y;
   const double bx = ld->v2->x / 65536.0 - sm_x, by = ld->v2->y / 65536.0 - sm_y;
   const double ex = bx - ax, ey = by - ay;
   double a0, a1, span;
   int k, k0, count, j;

   if (!sm_blocks(ld))
      return TRUE;
   /* out of reach: its nearest point beyond the radius */
   {
      const double len2 = ex * ex + ey * ey;
      double t = len2 > 0 ? -(ax * ex + ay * ey) / len2 : 0, px, py;
      t = t < 0 ? 0 : t > 1 ? 1 : t;
      px = ax + t * ex;
      py = ay + t * ey;
      if (px * px + py * py >= (double)sm_r * sm_r)
         return TRUE;
   }
   a0 = atan2(ay, ax);
   a1 = atan2(by, bx);
   span = a1 - a0;
   while (span > M_PI)
      span -= 2 * M_PI;
   while (span < -M_PI)
      span += 2 * M_PI;
   if (span < 0)
   {
      a0 = a1;
      span = -span;
   }
   k0 = sm_angle_index(a0);
   count = (int)(span / (2 * M_PI) * DOPO_SHADOW_RAYS) + 2;
   for (j = 0; j < count; j++)
   {
      double den, t, s;
      k = (k0 + j) % DOPO_SHADOW_RAYS;
      /* the ray t * dir meets a + s * e */
      den = sm_cos[k] * ey - sm_sin[k] * ex;
      if (fabs(den) < 1e-9)
         continue;
      t = (ax * ey - ay * ex) / den;
      s = (ax * sm_sin[k] - ay * sm_cos[k]) / den;
      if (t > 0 && s >= -0.001 && s <= 1.001 && t < sm_dist[k])
         sm_dist[k] = (float)t;
   }
   return TRUE;
}

static void sm_make(float x, float y, float r, uint16_t *row)
{
   int bx0, bx1, by0, by1, bx, by, k;

   sm_x = x;
   sm_y = y;
   sm_r = r;
   for (k = 0; k < DOPO_SHADOW_RAYS; k++)
      sm_dist[k] = r;
   bx0 = (int)(((int64_t)((x - r) * 65536.0) - bmaporgx) >> MAPBLOCKSHIFT);
   bx1 = (int)(((int64_t)((x + r) * 65536.0) - bmaporgx) >> MAPBLOCKSHIFT);
   by0 = (int)(((int64_t)((y - r) * 65536.0) - bmaporgy) >> MAPBLOCKSHIFT);
   by1 = (int)(((int64_t)((y + r) * 65536.0) - bmaporgy) >> MAPBLOCKSHIFT);
   if (bx0 < 0) bx0 = 0;
   if (by0 < 0) by0 = 0;
   if (bx1 >= bmapwidth) bx1 = bmapwidth - 1;
   if (by1 >= bmapheight) by1 = bmapheight - 1;
   validcount++;
   for (by = by0; by <= by1; by++)
      for (bx = bx0; bx <= bx1; bx++)
         P_BlockLinesIterator(bx, by, sm_line);
   for (k = 0; k < DOPO_SHADOW_RAYS; k++)
   {
      const double f = sm_dist[k] / r;
      row[k] = (uint16_t)(f >= 1 ? 65535 : f * 65535.0);
   }
}

void dopo_shadowmap_build(const dopo_light_t *lights, int n, uint16_t *out)
{
   const float vx = viewx / 65536.0f, vy = viewy / 65536.0f;
   int i, c, refresh = -1;
   unsigned oldest = ~0u;

   if (!sm_dirs_made)
   {
      for (i = 0; i < DOPO_SHADOW_RAYS; i++)
      {
         const double a = 2 * M_PI * (i + 0.5) / DOPO_SHADOW_RAYS;
         sm_cos[i] = (float)cos(a);
         sm_sin[i] = (float)sin(a);
      }
      sm_dirs_made = 1;
   }
   /* a new map, or another set of wads: nothing kept */
   if (sm_owner != lumpinfo || sm_lines_owner != lines)
   {
      memset(sm_cache, 0, sizeof(sm_cache));
      sm_owner = lumpinfo;
      sm_lines_owner = lines;
   }
   sm_frame++;
   sm_n = n;
   for (i = 0; i < n; i++)
   {
      const float x = lights[i].x + vx, y = lights[i].y + vy, r = lights[i].radius;
      uint16_t *row = out + (size_t)i * DOPO_SHADOW_RAYS;
      sm_lx[i] = x;
      sm_ly[i] = y;
      sm_lr[i] = r;
      sm_rows[i] = row;
      if (!sm_on() || !bmapwidth)
      {
         for (c = 0; c < DOPO_SHADOW_RAYS; c++)
            row[c] = 65535;
         continue;
      }
      for (c = 0; c < SM_CACHE; c++)
         if (sm_cache[c].made && fabsf(sm_cache[c].x - x) < 0.5f
               && fabsf(sm_cache[c].y - y) < 0.5f && fabsf(sm_cache[c].r - r) < 0.5f)
            break;
      if (c == SM_CACHE)
      {
         /* a light not seen before (or moving): made now, kept in the
          * slot asked for longest ago */
         unsigned least = ~0u;
         int slot = 0;
         for (c = 0; c < SM_CACHE; c++)
            if (sm_cache[c].used < least)
            {
               least = sm_cache[c].used;
               slot = c;
            }
         c = slot;
         sm_make(x, y, r, sm_cache[c].row);
         sm_cache[c].x = x;
         sm_cache[c].y = y;
         sm_cache[c].r = r;
         sm_cache[c].made = sm_frame;
      }
      else if (sm_cache[c].made < oldest)
      {
         oldest = sm_cache[c].made;
         refresh = c;
      }
      sm_cache[c].used = sm_frame;
      memcpy(row, sm_cache[c].row, sizeof(sm_cache[c].row));
   }
   /* the kept light made longest ago, made again (a door may have moved) */
   if (refresh >= 0 && sm_cache[refresh].made != sm_frame)
   {
      sm_make(sm_cache[refresh].x, sm_cache[refresh].y, sm_cache[refresh].r,
              sm_cache[refresh].row);
      sm_cache[refresh].made = sm_frame;
      for (i = 0; i < n; i++)
         if (sm_lx[i] == sm_cache[refresh].x && sm_ly[i] == sm_cache[refresh].y)
            memcpy(out + (size_t)i * DOPO_SHADOW_RAYS, sm_cache[refresh].row,
                   sizeof(sm_cache[refresh].row));
   }
}

int dopo_shadowmap_lit(int i, float x, float y)
{
   double dx, dy, d;
   if (i < 0 || i >= sm_n || !sm_on())
      return 1;
   dx = x + viewx / 65536.0 - sm_lx[i];
   dy = y + viewy / 65536.0 - sm_ly[i];
   d = sqrt(dx * dx + dy * dy);
   return d <= sm_rows[i][sm_angle_index(atan2(dy, dx))] / 65535.0 * sm_lr[i] + 4;
}
