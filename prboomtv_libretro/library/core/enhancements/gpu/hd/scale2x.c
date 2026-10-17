/**
 * @file scale2x.c
 * @brief The prboom-scale core option (prboomtv/scale2x.h).
 *
 * Scale2x (EPX) only reuses the original pixels: each texel becomes four,
 * each taking a neighbour's index where the two neighbours that meet at
 * its corner agree, so the result stays in the palette and the shading
 * and the colormaps keep working. On Doom's noisy sprites it smooths
 * little inside; the transparency is clean, so the outline is where it
 * shows. Made once, on the CPU, when gl.c uploads a sprite or a 2D patch
 * as a texture; the software renderer draws as before.
 */
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "prboomtv/scale2x.h"

const struct retro_core_option_v2_definition dopo_scale_option =
{
   "prboom-scale",
   "Sprite Smoothing (gl only)",
   NULL,
   "Gives the GPU the monsters, items, the weapon and the 2D (status bar, "
   "HUD, menus) at twice their size, smoothed with Scale2x, a filter that "
   "only reuses the original pixels; only with the gl renderer. "
   "'contour' rounds off the outline and keeps the inside as it was; "
   "'full' smooths the inside too, where neighbouring pixels share a "
   "colour; 'full + 4x contour' then rounds the outline once more, at "
   "four times the size (four times the memory of 'full', for PCs).",
   NULL,
   NULL,
   {
      { "0", "Off" },
      { "1", "Contour" },
      { "2", "Full" },
      { "3", "Full + 4x Contour" },
      { NULL, NULL },
   },
   "2"
};

int dopo_scale = 2;

void dopo_scale_read(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-scale", NULL };

   dopo_scale = 2;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
         && (!strcmp(var.value, "0") || !strcmp(var.value, "1")
             || !strcmp(var.value, "3")))
      dopo_scale = var.value[0] - '0';
}

/* Scale2x on a w x h image; contour keeps the index of every texel that
 * stays opaque, so only the outline changes. */
static void scale2x(const uint16_t *src, uint16_t *dst, int w, int h, dbool contour)
{
   int x, y, i;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         const uint16_t e = src[y * w + x];
         const uint16_t b = src[(y > 0 ? y - 1 : y) * w + x];
         const uint16_t hh = src[(y < h - 1 ? y + 1 : y) * w + x];
         const uint16_t d = src[y * w + (x > 0 ? x - 1 : x)];
         const uint16_t f = src[y * w + (x < w - 1 ? x + 1 : x)];
         uint16_t o[4] = { e, e, e, e };
         uint16_t *row = dst + (size_t)(2 * y) * (2 * w) + 2 * x;
         if (b != hh && d != f)
         {
            if (d == b) o[0] = d;
            if (b == f) o[1] = f;
            if (d == hh) o[2] = d;
            if (hh == f) o[3] = f;
         }
         if (contour)
            for (i = 0; i < 4; i++)
               if ((o[i] == DOPO_SCALE_CLEAR) == (e == DOPO_SCALE_CLEAR))
                  o[i] = e;
         row[0] = o[0];
         row[1] = o[1];
         row[2 * w] = o[2];
         row[2 * w + 1] = o[3];
      }
}

uint16_t *dopo_scale_image(const rpatch_t *p, int mode, int *k)
{
   /* one column after another: a row of the image is a patch column */
   const int w = p->height, h = p->width;
   uint16_t *src, *big, *mid;
   int x, y, i;

   *k = mode == 3 ? 4 : mode ? 2 : 1;
   src = (uint16_t *)malloc(sizeof(uint16_t) * w * h);
   if (!src)
      return NULL;
   for (i = 0; i < w * h; i++)
      src[i] = DOPO_SCALE_CLEAR;
   for (x = 0; x < p->width; x++)
   {
      const rcolumn_t *column = &p->columns[x];
      for (i = 0; i < column->numPosts; i++)
      {
         const rpost_t *post = &column->posts[i];
         for (y = post->topdelta; y < post->topdelta + post->length && y < p->height; y++)
            if (y >= 0)
               src[x * w + y] = column->pixels[y];
      }
   }
   if (*k == 1)
      return src;
   big = (uint16_t *)malloc(sizeof(uint16_t) * w * h * *k * *k);
   if (!big)
   {
      free(src);
      return NULL;
   }
   if (*k == 4)
   {
      mid = (uint16_t *)malloc(sizeof(uint16_t) * w * h * 4);
      if (!mid)
      {
         free(src);
         free(big);
         return NULL;
      }
      scale2x(src, mid, w, h, FALSE);
      scale2x(mid, big, w * 2, h * 2, TRUE);
      free(mid);
   }
   else
      scale2x(src, big, w, h, mode == 1);
   free(src);
   return big;
}
