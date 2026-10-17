/**
 * @file scale2x.c
 * @brief The prboom-scale core option (prboomtv/scale2x.h).
 *
 * Scale2x (EPX) only reuses the original pixels: each texel becomes four,
 * each taking a neighbour's index where the two neighbours that meet at
 * its corner agree, so the result stays in the palette and the shading
 * and the colormaps keep working. On Doom's noisy sprites it smooths
 * little inside; the transparency is clean, so the outline is where it
 * shows. The gl renderer sends the result as a texture (gl.c); the
 * software renderer draws a sprite from a patch rebuilt from it, its
 * projection scaled down by the same factor (the R_DrawVisSprite hunk
 * below). The module itself is a hunk at the top of r_things.c, the one
 * file that uses it from upstream; gl.c and libretro.c reach it through
 * prboomtv/scale2x.h.
 */
/**
 * @brief The module: the option, Scale2x and the bigger sprite patches.
 *
 * @patch src/r_things.c 56
 */
#include <stdlib.h>
#include <string.h>

#include "w_wad.h"
#include "prboomtv/scale2x.h"

const struct retro_core_option_v2_definition dopo_scale_option =
{
   "prboom-scale",
   "Sprite Smoothing",
   NULL,
   "Draws the monsters, items and the weapon (and with the gl renderer "
   "the 2D: status bar, HUD, menus) from copies at twice their size, "
   "smoothed with Scale2x, a filter that only reuses the original pixels. "
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
static void scale_2x(const uint16_t *src, uint16_t *dst, int w, int h, dbool contour)
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
      scale_2x(src, mid, w, h, FALSE);
      scale_2x(mid, big, w * 2, h * 2, TRUE);
      free(mid);
   }
   else
      scale_2x(src, big, w, h, mode == 1);
   free(src);
   return big;
}

/* ---- sprites for the software renderer ---- */

static rpatch_t   **scale_sprites = NULL;     /* per lump, NULL until made */
static dbool       *scale_sprites_bad = NULL;
static int          scale_sprites_n = 0;
static lumpinfo_t  *scale_sprites_owner = NULL;
static int          scale_sprites_mode = 0;

static void scale_sprites_reset(void)
{
   int i;
   for (i = 0; i < scale_sprites_n; i++)
      if (scale_sprites[i])
      {
         free(scale_sprites[i]->data);
         free(scale_sprites[i]);
      }
   free(scale_sprites);
   free(scale_sprites_bad);
   scale_sprites = (rpatch_t **)calloc(numlumps > 0 ? numlumps : 1, sizeof(*scale_sprites));
   scale_sprites_bad = (dbool *)calloc(numlumps > 0 ? numlumps : 1, sizeof(*scale_sprites_bad));
   scale_sprites_n = scale_sprites && scale_sprites_bad ? numlumps : 0;
   scale_sprites_owner = lumpinfo;
   scale_sprites_mode = dopo_scale;
}

/* a patch like r_patch.c makes them, from the image: one block holding
 * the pixels (every column whole, its holes filled from above as
 * r_patch.c does, for the filtered drawers), the columns and the posts */
static rpatch_t *scale_sprite_patch(const rpatch_t *p, const uint16_t *img, int k)
{
   const int width = p->width * k, height = p->height * k;
   rpatch_t *patch;
   int x, y, posts = 0;
   size_t pixels_size, columns_size;

   for (x = 0; x < width; x++)
      for (y = 0; y < height; y++)
         if (img[x * height + y] != DOPO_SCALE_CLEAR
               && (y == 0 || img[x * height + y - 1] == DOPO_SCALE_CLEAR))
            posts++;
   patch = (rpatch_t *)calloc(1, sizeof(*patch));
   if (!patch)
      return NULL;
   pixels_size  = (size_t)width * height;
   columns_size = sizeof(rcolumn_t) * width;
   patch->data = (unsigned char *)calloc(1, pixels_size + columns_size
                                            + sizeof(rpost_t) * (posts + 1) + 16);
   if (!patch->data)
   {
      free(patch);
      return NULL;
   }
   patch->width = width;
   patch->height = height;
   patch->widthmask = 0;
   patch->isNotTileable = 1;
   patch->leftoffset = p->leftoffset * k;
   patch->topoffset = p->topoffset * k;
   patch->columns = (rcolumn_t *)patch->data;
   patch->posts = (rpost_t *)(patch->data + columns_size);
   patch->pixels = patch->data + columns_size + sizeof(rpost_t) * (posts + 1);

   posts = 0;
   for (x = 0; x < width; x++)
   {
      rcolumn_t *column = &patch->columns[x];
      const uint16_t *src = img + (size_t)x * height;
      unsigned char last = 0;
      column->pixels = patch->pixels + (size_t)x * height;
      column->posts = &patch->posts[posts];
      column->numPosts = 0;
      for (y = 0; y < height; y++)
         if (src[y] != DOPO_SCALE_CLEAR)
         {
            last = (unsigned char)src[y];
            break;
         }
      for (y = 0; y < height; y++)
      {
         if (src[y] == DOPO_SCALE_CLEAR)
         {
            column->pixels[y] = last;
            continue;
         }
         last = column->pixels[y] = (unsigned char)src[y];
         if (y == 0 || src[y - 1] == DOPO_SCALE_CLEAR)
         {
            rpost_t *post = &patch->posts[posts++];
            post->topdelta = y;
            post->length = 0;
            post->slope = 0;
            column->numPosts++;
         }
         patch->posts[posts - 1].length++;
      }
   }
   return patch;
}

const rpatch_t *dopo_scale_sprite(int lump, const rpatch_t *p, int *k)
{
   uint16_t *img;

   if (!dopo_scale || !p || p->width <= 0 || p->height <= 0)
      return NULL;
   if (scale_sprites_owner != lumpinfo || scale_sprites_n != numlumps || scale_sprites_mode != dopo_scale)
      scale_sprites_reset();
   if (lump < 0 || lump >= scale_sprites_n || scale_sprites_bad[lump])
      return NULL;
   *k = dopo_scale == 3 ? 4 : 2;
   if (scale_sprites[lump])
      return scale_sprites[lump];
   img = dopo_scale_image(p, dopo_scale, k);
   if (img)
   {
      scale_sprites[lump] = scale_sprite_patch(p, img, *k);
      free(img);
   }
   if (!scale_sprites[lump])
      scale_sprites_bad[lump] = TRUE;
   return scale_sprites[lump];
}
/* @endpatch */

/**
 * @brief A sprite the CPU draws is drawn from its patch at k times the
 * size, its projection scaled to match (k times the texels per pixel, 1/k
 * the pixels per texel), so it lands exactly where the original would and
 * the column drawers walk the bigger posts without knowing. Not with a
 * brightmap (its mask is the original's size) or sloped edges.
 *
 * @patch src/r_things.c 1212
 */
  if (!gl_sprite && !sprmask && dopo_scale && dcvars.edgetype != RDRAW_MASKEDCOLUMNEDGE_SLOPED)
  {
    int k;
    const rpatch_t *big = dopo_scale_sprite(vis->patch + firstspritelump, patch, &k);
    if (big)
    {
      static vissprite_t scaled;
      scaled = *vis;
      scaled.startfrac *= k;
      scaled.xiscale *= k;
      scaled.scale /= k;
      vis = &scaled;
      patch = big;
      dcvars.iscale *= k;
      dcvars.texturemid *= k;
      spryscale = scaled.scale;
      frac = scaled.startfrac;
      if (filter == RDRAW_FILTER_LINEAR)
        frac -= (FRACUNIT>>1);
    }
  }
/* @endpatch */
