/**
 * @file brightmaps.c
 * @brief Brightmaps with prboom-dynlight (prboomtv/gpu/brightmaps.h).
 *
 * GZDoom's brightmaps.pk3 holds, for each game, a GLDEFS of brightmap
 * blocks (filter/<game>/gldefs.bm: brightmap sprite TROOA1 { map
 * "brightmaps/doom/trooa1.png" iwad }) and the masks as images the size of
 * the sprite; for Doom they are all sprites. It is read straight from the
 * archive, which is not loaded as a wad, and each sprite's mask is made
 * the first time gl.c uploads the sprite: how bright a texel is at least,
 * from the image's lightness (graded). "iwad" keeps a mask to the IWAD's own
 * sprite, so a wad that redraws the sprite does not get the old mask. The
 * wads' own GLDEFS brightmaps, for sprites, textures and flats, are the
 * software renderer's (u_brightmap.c) and win over the archive's.
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "doomstat.h"
#include "lprintf.h"
#include "r_data.h"
#include "r_main.h"
#include "u_brightmap.h"
#include "u_scanner.h"
#include "w_wad.h"
#include "formats/image.h"
#include "formats/rpng.h"
#include "prboomtv/gpu/brightmaps.h"
#include "prboomtv/gpu/dynlights.h"
#include "prboomtv/gpu/render_gl.h"

/* the archive's sprite brightmaps: the definitions, and per sprite lump
 * its mask once made (bm_none: none) */
typedef struct
{
   char name[9];
   char path[96];
   int  iwad;
   int  nofullbright;   /* disablefullbright */
} bm_def_t;

static bm_def_t      *bm_defs = NULL;
static int            bm_ndefs = 0;
static unsigned char *bm_zip = NULL;
static int64_t        bm_zip_len = 0;
static uint8_t      **bm_masks = NULL;
static int            bm_nmasks = 0;
static lumpinfo_t    *bm_owner = NULL;
static int            bm_owner_n = -1;
static uint8_t        bm_none;

static void bm_reset(void)
{
   int i;
   for (i = 0; i < bm_nmasks; i++)
      if (bm_masks[i] && bm_masks[i] != &bm_none)
         free(bm_masks[i]);
   free(bm_masks);
   free(bm_defs);
   free(bm_zip);
   bm_masks = NULL;
   bm_defs = NULL;
   bm_zip = NULL;
   bm_nmasks = bm_ndefs = 0;
   bm_zip_len = 0;
}

static void bm_skip_block(u_scanner_t *s)
{
   int depth = 1;
   while (depth > 0 && U_GetNextToken(s, TRUE))
   {
      if (s->token == '{')
         depth++;
      else if (s->token == '}')
         depth--;
   }
}

/* brightmap sprite NAME { map "path" [iwad] [disablefullbright] } */
static void bm_parse(const char *text, int len)
{
   u_scanner_t s = U_ScanOpen(text, len, "brightmaps.pk3");
   int cap = 0;
   while (U_GetNextToken(&s, TRUE))
   {
      bm_def_t d;
      dbool sprite;
      if (s.token == '{')
      {
         bm_skip_block(&s);
         continue;
      }
      if (s.token != TK_Identifier || strcasecmp(s.string, "brightmap"))
         continue;
      if (!U_GetNextToken(&s, TRUE) || s.token != TK_Identifier)
         continue;
      sprite = !strcasecmp(s.string, "sprite");
      if (!U_GetNextToken(&s, TRUE) || (s.token != TK_Identifier && s.token != TK_StringConst))
         continue;
      memset(&d, 0, sizeof(d));
      snprintf(d.name, sizeof(d.name), "%s", s.string);
      if (!U_CheckToken(&s, '{'))
         continue;
      while (U_GetNextToken(&s, TRUE) && s.token != '}')
      {
         if (s.token != TK_Identifier)
            continue;
         if (!strcasecmp(s.string, "map") && U_GetNextToken(&s, TRUE)
               && s.token == TK_StringConst)
            snprintf(d.path, sizeof(d.path), "%s", s.string);
         else if (!strcasecmp(s.string, "iwad"))
            d.iwad = 1;
         else if (!strcasecmp(s.string, "disablefullbright"))
            d.nofullbright = 1;
      }
      if (!sprite || !d.path[0])
         continue;
      if (bm_ndefs == cap)
      {
         bm_def_t *n;
         cap = cap ? cap * 2 : 256;
         n = (bm_def_t *)realloc(bm_defs, sizeof(*n) * cap);
         if (!n)
            break;
         bm_defs = n;
      }
      bm_defs[bm_ndefs++] = d;
   }
   U_ScanClose(&s);
}

/* everything again for a new set of wads: the archive's definitions */
static void bm_load(void)
{
   char member[64], where[1200];
   int text_len = 0;
   unsigned char *text;

   bm_reset();
   bm_owner = lumpinfo;
   bm_owner_n = numlumps;
   bm_nmasks = numspritelumps;
   bm_masks = (uint8_t **)calloc(numspritelumps > 0 ? numspritelumps : 1, sizeof(*bm_masks));
   if (!bm_masks)
      bm_nmasks = 0;
   bm_zip = (unsigned char *)dopo_resource_read("brightmaps.pk3", &bm_zip_len, where, sizeof(where));
   if (!bm_zip)
      return;
   snprintf(member, sizeof(member), "filter/%s/gldefs.bm", dopo_resource_game());
   text = W_PK3ExtractPath(bm_zip, bm_zip_len, member, &text_len);
   if (text)
   {
      bm_parse((const char *)text, text_len);
      free(text);
   }
   lprintf(LO_INFO, "prboom-brightmaps: %d sprite brightmaps from %s\n", bm_ndefs, where);
}

/* a mask from an image the size of the patch: how bright each texel is
 * at least (its lightness where opaque; GZDoom's brightmaps are graded:
 * an imp's eyes glow dimmer than a torch's flame) */
static uint8_t *bm_from_png(const unsigned char *png, int len, const rpatch_t *p)
{
   rpng_t *rpng = rpng_alloc();
   uint32_t *argb = NULL;
   unsigned w = 0, h = 0, x, y;
   uint8_t *mask = NULL;
   int r;

   if (!rpng)
      return NULL;
   if (!rpng_set_buf_ptr(rpng, (void *)png, (size_t)len) || !rpng_start(rpng))
   {
      rpng_free(rpng);
      return NULL;
   }
   while (rpng_iterate_image(rpng))
      ;
   if (!rpng_is_valid(rpng))
   {
      rpng_free(rpng);
      return NULL;
   }
   do
      r = rpng_process_image(rpng, (void **)&argb, (size_t)len, &w, &h, true);
   while (r == IMAGE_PROCESS_NEXT);
   rpng_free(rpng);
   if (r == IMAGE_PROCESS_END && argb && (int)w == p->width && (int)h == p->height
         && (mask = (uint8_t *)calloc((size_t)w * h, 1)) != NULL)
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
         {
            const uint32_t c = argb[y * w + x];
            const unsigned l = (((c >> 16) & 255) * 3 + ((c >> 8) & 255) * 6 + (c & 255)) / 10;
            mask[x * h + y] = (c >> 24) >= 128 && l >= 8 ? (uint8_t)l : 0;
         }
   free(argb);
   return mask;
}

/* the archive's definition for a sprite lump, NULL when none applies */
static const bm_def_t *bm_def_for(int spritelump)
{
   const lumpinfo_t *li;
   int i;
   if (bm_owner != lumpinfo || bm_owner_n != numlumps || bm_nmasks != numspritelumps)
      bm_load();
   if (spritelump < 0 || spritelump >= bm_nmasks || !bm_ndefs)
      return NULL;
   li = &lumpinfo[firstspritelump + spritelump];
   for (i = 0; i < bm_ndefs; i++)
      if (!strncasecmp(bm_defs[i].name, li->name, 8))
         return bm_defs[i].iwad && li->source != source_iwad ? NULL : &bm_defs[i];
   return NULL;
}

void dopo_brightmap_project(vissprite_t *vis, const lighttable_t *lit)
{
   const bm_def_t *d;
   if (!dopo_dynlight || !dopo_gl || !vis->colormap || vis->colormap != fullcolormap)
      return;
   if ((d = bm_def_for(vis->patch)) != NULL && d->nofullbright)
   {
      /* the frame stays in the room's light; its brightmap glows */
      vis->colormap = lit;
      vis->emissive = 0;
   }
}

const uint8_t *dopo_brightmask_sprite(int spritelump, const rpatch_t *p)
{
   const uint8_t *mask;
   const lumpinfo_t *li;
   int i, lump;

   if (!dopo_dynlight || !dopo_gl || spritelump < 0)
      return NULL;
   /* a wad's own */
   if ((mask = U_BrightmaskForSprite(spritelump)) != NULL)
      return mask;
   if (bm_owner != lumpinfo || bm_owner_n != numlumps || bm_nmasks != numspritelumps)
      bm_load();
   if (spritelump >= bm_nmasks || !bm_ndefs || !p)
      return NULL;
   if (bm_masks[spritelump])
      return bm_masks[spritelump] == &bm_none ? NULL : bm_masks[spritelump];
   bm_masks[spritelump] = &bm_none;
   lump = firstspritelump + spritelump;
   li = &lumpinfo[lump];
   for (i = 0; i < bm_ndefs; i++)
      if (!strncasecmp(bm_defs[i].name, li->name, 8))
         break;
   if (i == bm_ndefs || (bm_defs[i].iwad && li->source != source_iwad))
      return NULL;
   {
      int len = 0;
      unsigned char *png = W_PK3ExtractPath(bm_zip, bm_zip_len, bm_defs[i].path, &len);
      uint8_t *made = png ? bm_from_png(png, len, p) : NULL;
      free(png);
      if (made)
         bm_masks[spritelump] = made;
   }
   return bm_masks[spritelump] == &bm_none ? NULL : bm_masks[spritelump];
}

const uint8_t *dopo_brightmask_texture(int texnum)
{
   return dopo_dynlight && dopo_gl ? U_BrightmaskForTexture(texnum) : NULL;
}

const uint8_t *dopo_brightmask_flat(int flatnum)
{
   return dopo_dynlight && dopo_gl ? U_BrightmaskForFlat(flatnum) : NULL;
}
