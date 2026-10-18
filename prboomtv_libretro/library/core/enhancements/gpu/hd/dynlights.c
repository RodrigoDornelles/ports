/**
 * @file dynlights.c
 * @brief The prboom-dynlight core option (prboomtv/gpu/dynlights.h).
 *
 * The lights are GLDEFS, GZDoom's format: light definitions (pointlight,
 * pulselight, flickerlight, flickerlight2: colour, radius, how it
 * animates, an offset from the thing that turns with it) and the sprite
 * frames that carry
 * them (object CLASS { frame BAL1C { light IMPBALL_X1 } }, a frame with no
 * letter meaning all of them; the class is not looked at, the sprite
 * is). Read in order, a later definition or binding replacing an earlier
 * one: a small built-in set for Doom (gldefs/doom1.txt, embedded by
 * dopo/scripts/xxd.cpp), GZDoom's lights.pk3 if there is one (its
 * filter/<game>/gldefs.txt, read straight from the archive, which is not
 * loaded as a wad; next to the game, next to the core or in the system
 * directory), then the GLDEFS lumps of the loaded wads. Every frame
 * the things carrying a light are gathered and the nearest few go to the
 * GPU (gl.c), which lights only what lies within their reach.
 *
 * Maps made for GZDoom also place lights as things of their own (editor
 * numbers 9800 and up, in Hexen-format and UDMF maps: the colour in args
 * 0-2, the size in arg 3, the other size in arg 4); with prboom-9800 they
 * join the lights of the frame, whatever prboom-dynlight's level.
 * Subtractive lights and spotlights are left out.
 *
 * The module is a hunk at the top of r_dynlight.c, the software
 * renderer's own approximation of GLDEFS lights, which stands down while
 * the GPU lights; a hunk in w_pk3.c reads one member of a pk3, and hunks
 * in p_mobj.c and p_setup.c take the map's lights.
 */

/**
 * @brief The module.
 *
 * @patch src/r_dynlight.c 18
 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <strings.h>

#include "lprintf.h"
#include "w_wad.h"
#include "u_scanner.h"
#include "streams/file_stream.h"
#include "prboomtv/gpu/gldefs.h"
#include "prboomtv/gpu/dynlights.h"
#include "prboomtv/gpu/render_gl.h"

unsigned char *W_PK3ExtractPath(const unsigned char *zip, int64_t zip_length,
                                const char *path, int *out_length);
extern int numsprites;

const struct retro_core_option_v2_definition dopo_dynlight_option =
{
   "prboom-dynlight",
   "Dynamic Lights (gl only)",
   NULL,
   "Coloured light from fireballs, explosions and the muzzle flash on the "
   "walls, floors and monsters around them, as GZDoom's dynamic lights; "
   "only with the gl renderer. 'all' adds the torches, lamps and glowing "
   "items; 'all + shadows' lets the walls stop them, as GZDoom's light "
   "shadowmaps (a torch lights only its room, a flash does not shine "
   "through a wall; the shadows lie on the map's plane, a step casts none), "
   "worth it in maps made for GZDoom that place their own lights, little "
   "seen in classic maps and dearer on a weak GPU. Uses GZDoom's lights.pk3 "
   "when it is next to the game, next to the core or in the system "
   "directory, a small built-in set otherwise.",
   NULL,
   NULL,
   {
      { "0", "Off" },
      { "1", "Moving" },
      { "2", "All" },
      { "3", "All + Shadows" },
      { NULL, NULL },
   },
   "2"
};

int dopo_dynlight = 2;

const struct retro_core_option_v2_definition dopo_maplight_option =
{
   "prboom-9800",
   "Map Lights (gl only)",
   NULL,
   "The lights a map made for GZDoom places itself (things 9800 and up), "
   "the lighting its author planned; with Dynamic Lights on, only with the "
   "gl renderer. Takes effect when a map loads.",
   NULL,
   NULL,
   {
      { "enabled",  NULL },
      { "disabled", NULL },
      { NULL, NULL },
   },
   "enabled"
};

int dopo_maplight = 1;

const struct retro_core_option_v2_definition dopo_lights_option =
{
   "prboom-lights",
   "Dynamic Lights at Once (gl only)",
   NULL,
   "How many dynamic lights a frame keeps, the ones reaching nearest the "
   "eye; the rest wait until they come closer. Classic maps rarely have "
   "more than 8 near at once; maps made for GZDoom place dozens. Each one "
   "is work for every lit pixel, so more is heavier on the GPU (also capped "
   "by what the GPU takes).",
   NULL,
   NULL,
   {
      { "8",  NULL },
      { "16", NULL },
      { "32", NULL },
      { NULL, NULL },
   },
   "8"
};

int dopo_lights = 8;

static char dl_system_dir[1024];
static char dl_core_dir[1024];

void dopo_dynlight_read(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-dynlight", NULL };
   const char *dir = NULL;

   dopo_dynlight = 2;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
         && (!strcmp(var.value, "0") || !strcmp(var.value, "1") || !strcmp(var.value, "3")))
      dopo_dynlight = var.value[0] - '0';
   var.key = "prboom-lights";
   var.value = NULL;
   dopo_lights = 8;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      dopo_lights = !strcmp(var.value, "32") ? 32 : !strcmp(var.value, "16") ? 16 : 8;
   var.key = "prboom-9800";
   var.value = NULL;
   dopo_maplight = !(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
                     && !strcmp(var.value, "disabled"));
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      snprintf(dl_system_dir, sizeof(dl_system_dir), "%s", dir);
   dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LIBRETRO_PATH, &dir) && dir)
   {
      /* the core's own folder */
      const char *slash = strrchr(dir, '/'), *bslash = strrchr(dir, '\\');
      if (bslash > slash)
         slash = bslash;
      if (slash)
         snprintf(dl_core_dir, sizeof(dl_core_dir), "%.*s", (int)(slash - dir), dir);
   }
}

/* ---- definitions ---- */

enum { DLK_POINT, DLK_PULSE, DLK_FLICKER, DLK_FLICKER2 };

typedef struct
{
   char  name[48];
   float r, g, b;
   int   size, size2;      /* radius, the other one it animates to */
   int   kind;
   int   interval;         /* tics */
   float chance;           /* flicker: how often the main size */
   int   has_offset;
   float up;               /* offset above the thing's feet */
   float fwd, side;        /* offset ahead of the thing and to its left,
                            * turning with it (GLDEFS offset x, z) */
   int   attenuate;        /* fades with the angle it meets a surface at */
} dl_def_t;

typedef struct
{
   char sprite[5];
   int  frame;             /* 0.., -1 every frame */
   int  def;
} dl_bind_t;

static dl_def_t   *dl_defs = NULL;
static int         dl_ndefs = 0, dl_capdefs = 0;
static dl_bind_t  *dl_binds = NULL;
static int         dl_nbinds = 0, dl_capbinds = 0;

/* per sprite number and frame: the definition, -1 none (frame 29: all) */
#define DL_FRAMES 30
static short      *dl_table = NULL;
static int         dl_table_n = 0;
static lumpinfo_t *dl_owner = NULL;
static int         dl_owner_n = -1;

static int dl_find(const char *name)
{
   int i;
   for (i = 0; i < dl_ndefs; i++)
      if (!strcasecmp(dl_defs[i].name, name))
         return i;
   return -1;
}

/* a number, with a leading '-' */
static dbool dl_number(u_scanner_t *s, double *out)
{
   double sign = 1;
   if (!U_GetNextToken(s, TRUE))
      return FALSE;
   if (s->token == '-')
   {
      sign = -1;
      if (!U_GetNextToken(s, TRUE))
         return FALSE;
   }
   if (s->token == TK_IntConst)
      *out = sign * s->number;
   else if (s->token == TK_FloatConst)
      *out = sign * s->decimal;
   else
      return FALSE;
   return TRUE;
}

static void dl_skip_block(u_scanner_t *s)
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

static void dl_parse_light(u_scanner_t *s, int kind)
{
   dl_def_t d;
   double v, a, b, c;
   int i;

   memset(&d, 0, sizeof(d));
   d.kind = kind;
   d.r = d.g = d.b = 1;
   d.interval = 35;
   d.chance = 0.5f;
   d.attenuate = 1;
   if (!U_GetNextToken(s, TRUE) || (s->token != TK_Identifier && s->token != TK_StringConst))
      return;
   snprintf(d.name, sizeof(d.name), "%s", s->string);
   if (!U_CheckToken(s, '{'))
      return;
   while (U_GetNextToken(s, TRUE) && s->token != '}')
   {
      if (s->token == '{')
      {
         dl_skip_block(s);
         continue;
      }
      if (s->token != TK_Identifier)
         continue;
      if (!strcasecmp(s->string, "color"))
      {
         if (dl_number(s, &a) && dl_number(s, &b) && dl_number(s, &c))
         {
            /* 0..1, or 0..255 */
            const double k = (a > 1 || b > 1 || c > 1) ? 1.0 / 255 : 1.0;
            d.r = (float)(a * k);
            d.g = (float)(b * k);
            d.b = (float)(c * k);
         }
      }
      else if (!strcasecmp(s->string, "size") && dl_number(s, &v))
         d.size = (int)v;
      else if (!strcasecmp(s->string, "secondarysize") && dl_number(s, &v))
         d.size2 = (int)v;
      else if (!strcasecmp(s->string, "interval") && dl_number(s, &v))
         d.interval = (int)(v * TICRATE + 0.5);
      else if (!strcasecmp(s->string, "chance") && dl_number(s, &v))
         d.chance = (float)v;
      else if (!strcasecmp(s->string, "offset"))
      {
         if (dl_number(s, &a) && dl_number(s, &b) && dl_number(s, &c))
         {
            d.has_offset = 1;
            d.fwd = (float)a;     /* x ahead, height, z to the side */
            d.up = (float)b;
            d.side = (float)c;
         }
      }
      else if (!strcasecmp(s->string, "subtractive") && dl_number(s, &v) && v)
         d.size = 0;              /* darkness: not done */
      else if (!strcasecmp(s->string, "attenuate") && dl_number(s, &v))
         d.attenuate = v != 0;
   }
   if (d.size2 <= 0)
      d.size2 = d.size;
   if (d.interval < 1)
      d.interval = 1;
   if (d.size <= 0)
      return;
   i = dl_find(d.name);
   if (i < 0)
   {
      if (dl_ndefs == dl_capdefs)
      {
         const int cap = dl_capdefs ? dl_capdefs * 2 : 64;
         dl_def_t *n = (dl_def_t *)realloc(dl_defs, sizeof(*n) * cap);
         if (!n)
            return;
         dl_defs = n;
         dl_capdefs = cap;
      }
      i = dl_ndefs++;
   }
   dl_defs[i] = d;
}

static void dl_bind(const char *frame, int def)
{
   dl_bind_t *b;
   if (strlen(frame) < 4)
      return;
   if (dl_nbinds == dl_capbinds)
   {
      const int cap = dl_capbinds ? dl_capbinds * 2 : 128;
      dl_bind_t *n = (dl_bind_t *)realloc(dl_binds, sizeof(*n) * cap);
      if (!n)
         return;
      dl_binds = n;
      dl_capbinds = cap;
   }
   b = &dl_binds[dl_nbinds++];
   memcpy(b->sprite, frame, 4);
   b->sprite[4] = 0;
   b->frame = frame[4] ? toupper((unsigned char)frame[4]) - 'A' : -1;
   b->def = def;
}

static void dl_parse_object(u_scanner_t *s)
{
   if (!U_GetNextToken(s, TRUE) || (s->token != TK_Identifier && s->token != TK_StringConst))
      return;
   if (!U_CheckToken(s, '{'))
      return;
   while (U_GetNextToken(s, TRUE) && s->token != '}')
   {
      char frame[16];
      if (s->token == '{')
      {
         dl_skip_block(s);
         continue;
      }
      if (s->token != TK_Identifier || strcasecmp(s->string, "frame"))
         continue;
      if (!U_GetNextToken(s, TRUE) || (s->token != TK_Identifier && s->token != TK_StringConst))
         continue;
      snprintf(frame, sizeof(frame), "%s", s->string);
      if (!U_CheckToken(s, '{'))
         continue;
      while (U_GetNextToken(s, TRUE) && s->token != '}')
         if (s->token == TK_Identifier && !strcasecmp(s->string, "light")
               && U_GetNextToken(s, TRUE)
               && (s->token == TK_Identifier || s->token == TK_StringConst))
         {
            const int d = dl_find(s->string);
            if (d >= 0)
               dl_bind(frame, d);
         }
   }
}

static void dl_parse(const char *text, int len, const char *name)
{
   u_scanner_t s = U_ScanOpen(text, len, name);
   while (U_GetNextToken(&s, TRUE))
   {
      if (s.token == '{')
      {
         dl_skip_block(&s);
         continue;
      }
      if (s.token != TK_Identifier)
         continue;
      if (!strcasecmp(s.string, "pointlight"))
         dl_parse_light(&s, DLK_POINT);
      else if (!strcasecmp(s.string, "pulselight"))
         dl_parse_light(&s, DLK_PULSE);
      else if (!strcasecmp(s.string, "flickerlight"))
         dl_parse_light(&s, DLK_FLICKER);
      else if (!strcasecmp(s.string, "flickerlight2"))
         dl_parse_light(&s, DLK_FLICKER2);
      else if (!strcasecmp(s.string, "object"))
         dl_parse_object(&s);
   }
   U_ScanClose(&s);
}

/* the folder of a path, "" when it has none */
static void dl_dirname(char *out, size_t size, const char *path)
{
   const char *slash = strrchr(path, '/'), *bslash = strrchr(path, '\\');
   if (bslash > slash)
      slash = bslash;
   if (slash)
      snprintf(out, size, "%.*s", (int)(slash - path), path);
   else
      out[0] = 0;
}

/* A file of GZDoom's (lights.pk3, brightmaps.pk3), the first found: next
 * to the game (the folder of any loaded wad, the IWAD's or the content's),
 * next to the core, in the system directory or its prboom folder; its
 * bytes (the caller frees them) and where it was, NULL when nowhere. */
void *dopo_resource_read(const char *file, int64_t *len, char *where, size_t where_size)
{
   char dirs[16][1100], path[1200];
   int i, k, n = 0;

   for (k = 0; k < (int)numwadfiles && n < 12; k++)
      if (wadfiles[k].name)
      {
         dl_dirname(dirs[n], sizeof(dirs[n]), wadfiles[k].name);
         for (i = 0; i < n; i++)
            if (!strcmp(dirs[i], dirs[n]))
               break;
         if (i == n && dirs[n][0])
            n++;
      }
   if (dl_core_dir[0])
      snprintf(dirs[n++], sizeof(dirs[0]), "%s", dl_core_dir);
   if (dl_system_dir[0])
   {
      snprintf(dirs[n++], sizeof(dirs[0]), "%s", dl_system_dir);
      snprintf(dirs[n++], sizeof(dirs[0]), "%s/prboom", dl_system_dir);
   }
   for (i = 0; i < n; i++)
   {
      void *data = NULL;
      snprintf(path, sizeof(path), "%.1100s/%s", dirs[i], file);
      if (!filestream_exists(path) || filestream_read_file(path, &data, len) <= 0 || !data)
         continue;
      if (where)
         snprintf(where, where_size, "%s", path);
      return data;
   }
   return NULL;
}

/* the folder of GZDoom's filter/ this game reads */
const char *dopo_resource_game(void)
{
   return hexen ? "hexen" : heretic ? "heretic"
        : W_CheckNumForName("FREEDOOM") >= 0 ? "doom.freedoom" : "doom.id";
}

/* lights.pk3's GLDEFS for this game */
static void dl_parse_lights_pk3(void)
{
   char member[64], where[1200];
   int64_t len = 0;
   void *zip = dopo_resource_read("lights.pk3", &len, where, sizeof(where));
   unsigned char *text;
   int text_len = 0;

   if (!zip)
      return;
   snprintf(member, sizeof(member), "filter/%s/gldefs.txt", dopo_resource_game());
   text = W_PK3ExtractPath((const unsigned char *)zip, len, member, &text_len);
   free(zip);
   if (text)
   {
      dl_parse((const char *)text, text_len, "lights.pk3");
      free(text);
      lprintf(LO_INFO, "prboom-dynlight: %s from %s\n", member, where);
   }
}

/* ---- the map's lights ---- */

#define DL_MAP_MAX 1024
static struct
{
   fixed_t         x, y, z;
   dl_def_t        d;
   const sector_t *sector;   /* a sector light: scaled by its light */
} dl_map[DL_MAP_MAX];
static int dl_map_n = 0;

void dopo_maplight_clear(void)
{
   dl_map_n = 0;
}

int dopo_maplight_count(void)
{
   return dl_map_n;
}

int dopo_maplight_add(int x, int y, int angle, int type)
{
   const int group = (type - 9800) / 10, variant = type % 10;
   const fixed_t fx = x * FRACUNIT, fy = y * FRACUNIT;
   const sector_t *sec;
   dl_def_t *d;

   if (type == 1502 || type == 1503)
   {
      /* Vavoom's lights: the size in arg 0, a colour in args 1-3 (1503) */
      if (dl_map_n == DL_MAP_MAX || hexen_thing_args[0] <= 0)
         return 1;
      sec = R_PointInSubsector(fx, fy)->sector;
      dl_map[dl_map_n].x = fx;
      dl_map[dl_map_n].y = fy;
      dl_map[dl_map_n].z = sec->floorheight + hexen_thing_height * FRACUNIT;
      dl_map[dl_map_n].sector = NULL;
      d = &dl_map[dl_map_n].d;
      memset(d, 0, sizeof(*d));
      d->r = d->g = d->b = 1;
      if (type == 1503)
      {
         d->r = hexen_thing_args[1] / 255.0f;
         d->g = hexen_thing_args[2] / 255.0f;
         d->b = hexen_thing_args[3] / 255.0f;
      }
      d->size = d->size2 = hexen_thing_args[0];
      d->kind = DLK_POINT;
      d->interval = TICRATE;
      d->has_offset = 1;
      dl_map_n++;
      return 1;
   }
   if (type < 9800 || type > 9899)
      return 0;
   /* point lights, plain, additive or attenuated; the others (subtractive,
    * spotlights) are taken and left out */
   if ((group != 0 && group != 1 && group != 3) || variant > 4 || dl_map_n == DL_MAP_MAX
         || hexen_thing_args[3] <= 0)
      return 1;
   sec = R_PointInSubsector(fx, fy)->sector;
   dl_map[dl_map_n].x = fx;
   dl_map[dl_map_n].y = fy;
   dl_map[dl_map_n].z = sec->floorheight + hexen_thing_height * FRACUNIT;
   dl_map[dl_map_n].sector = variant == 3 ? sec : NULL;
   d = &dl_map[dl_map_n].d;
   memset(d, 0, sizeof(*d));
   d->r = hexen_thing_args[0] / 255.0f;
   d->g = hexen_thing_args[1] / 255.0f;
   d->b = hexen_thing_args[2] / 255.0f;
   d->size = hexen_thing_args[3];
   d->size2 = hexen_thing_args[4] > 0 ? hexen_thing_args[4] : d->size;
   /* the angle: a pulse's period in tics, a flicker's chance in degrees */
   d->kind = variant == 1 ? DLK_PULSE : variant == 2 ? DLK_FLICKER
           : variant == 4 ? DLK_FLICKER2 : DLK_POINT;
   d->interval = angle > 0 ? angle : TICRATE;
   d->chance = angle > 0 ? angle / 360.0f : 0.5f;
   d->has_offset = 1;
   /* 9800-9814 light as GZDoom's legacy lights, whatever the angle; 9830
    * and up fade with it */
   d->attenuate = group == 3;
   dl_map_n++;
   return 1;
}

/* everything again for a new set of wads */
static void dl_load(void)
{
   static const char *const lumps[] = { "GLDEFS", "DOOMDEFS", "HTICDEFS", "HEXNDEFS" };
   size_t k;
   int i;

   free(dl_defs);
   free(dl_binds);
   dl_defs = NULL;
   dl_binds = NULL;
   dl_ndefs = dl_capdefs = dl_nbinds = dl_capbinds = 0;
   if (!raven)
      dl_parse((const char *)doom1, (int)doom1_size, "gldefs/doom1.txt");
   dl_parse_lights_pk3();
   for (k = 0; k < sizeof(lumps) / sizeof(*lumps); k++)
   {
      int lump = -1;
      while ((lump = W_FindNumFromName(lumps[k], lump)) >= 0)
      {
         dl_parse((const char *)W_CacheLumpNum(lump), W_LumpLength(lump), lumps[k]);
         W_UnlockLumpNum(lump);
      }
   }

   free(dl_table);
   dl_table_n = numsprites;
   dl_table = (short *)malloc(sizeof(short) * DL_FRAMES * (numsprites > 0 ? numsprites : 1));
   if (!dl_table)
      dl_table_n = 0;
   for (i = 0; i < DL_FRAMES * dl_table_n; i++)
      dl_table[i] = -1;
   for (i = 0; i < dl_nbinds; i++)
   {
      int sp;
      for (sp = 0; sp < dl_table_n; sp++)
         if (sprnames[sp] && !strncasecmp(sprnames[sp], dl_binds[i].sprite, 4))
            break;
      if (sp == dl_table_n)
         continue;
      if (dl_binds[i].frame < 0)
         dl_table[sp * DL_FRAMES + DL_FRAMES - 1] = (short)dl_binds[i].def;
      else if (dl_binds[i].frame < DL_FRAMES - 1)
         dl_table[sp * DL_FRAMES + dl_binds[i].frame] = (short)dl_binds[i].def;
   }
   dl_owner = lumpinfo;
   dl_owner_n = numlumps;
   lprintf(LO_INFO, "prboom-dynlight: %d lights, %d bindings\n", dl_ndefs, dl_nbinds);
}

static const dl_def_t *dl_for(int sprite, int frame)
{
   int d;
   if (sprite < 0 || sprite >= dl_table_n)
      return NULL;
   frame &= FF_FRAMEMASK;
   d = frame < DL_FRAMES - 1 ? dl_table[sprite * DL_FRAMES + frame] : -1;
   if (d < 0)
      d = dl_table[sprite * DL_FRAMES + DL_FRAMES - 1];
   return d >= 0 ? &dl_defs[d] : NULL;
}

/* a per light, per tic noise in 0..1, from where the thing is (never the
 * game's random numbers: demos and netplay stay in sync; the same run
 * looks the same) */
static float dl_noise(unsigned tic, const mobj_t *who)
{
   unsigned h = tic * 2654435761u
              ^ (unsigned)(who->x >> FRACBITS) * 73856093u
              ^ (unsigned)(who->y >> FRACBITS) * 19349663u;
   h ^= h >> 15;
   h *= 2246822519u;
   h ^= h >> 13;
   return (h & 0xffff) / 65535.0f;
}

static float dl_radius(const dl_def_t *d, const mobj_t *who)
{
   switch (d->kind)
   {
   case DLK_PULSE:
   {
      const double f = 0.5 + 0.5 * sin(leveltime * 2 * M_PI / d->interval);
      return (float)(d->size + (d->size2 - d->size) * f);
   }
   case DLK_FLICKER:
      return dl_noise(leveltime, who) < d->chance ? (float)d->size : (float)d->size2;
   case DLK_FLICKER2:
      return d->size + (d->size2 - d->size) * dl_noise(leveltime / d->interval, who);
   default:
      return (float)d->size;
   }
}

int dopo_dynlight_collect(dopo_light_t *out, int max)
{
   static dopo_light_t all[1024];
   static float dist[1024];
   thinker_t *th;
   const float vx = viewx / 65536.0f, vy = viewy / 65536.0f, vz = viewz / 65536.0f;
   const float fx = (float)cos(viewangle * (2 * M_PI / 4294967296.0));
   const float fy = (float)sin(viewangle * (2 * M_PI / 4294967296.0));
   int n = 0, i, j;

   if (!dopo_dynlight || !dopo_gl)
      return 0;
   if (dl_owner != lumpinfo || dl_owner_n != numlumps || dl_table_n != numsprites)
      dl_load();
   if (!dl_ndefs && !(dopo_maplight && dl_map_n))
      return 0;
   for (th = thinkercap.next; th != &thinkercap && n < 1024; th = th->next)
   {
      const mobj_t *mo;
      const dl_def_t *d;
      dopo_light_t *l;
      float r, fwd;
      if (th->function.arg1 != (void (*)(void *))P_MobjThinker)
         continue;
      mo = (const mobj_t *)th;
      if ((d = dl_for(mo->sprite, mo->frame)) == NULL)
         continue;
      /* 1: only what moves or fights; the decorations and items with 2 */
      if (dopo_dynlight < 2 && mo->info && !mo->info->speed
            && !(mo->flags & (MF_MISSILE | MF_COUNTKILL)) && mo->health > 0 && !mo->player)
         continue;
      /* GZDoom's reach is twice the size (DynamicLight::GetRadius) */
      r = 2 * dl_radius(d, mo);
      l = &all[n];
      l->x = mo->x / 65536.0f - vx;
      l->y = mo->y / 65536.0f - vy;
      l->z = (mo->z / 65536.0f) + (d->has_offset ? d->up : mo->height / 131072.0f) - vz;
      if (d->fwd != 0 || d->side != 0)
      {
         /* ahead of the thing, where it faces (a muzzle flash), as GZDoom
          * turns the offset with the actor */
         const double a = mo->angle * (2 * M_PI / 4294967296.0);
         l->x += (float)(d->fwd * cos(a) + d->side * sin(a));
         l->y += (float)(d->fwd * sin(a) - d->side * cos(a));
      }
      l->radius = r;
      l->r = d->r;
      l->g = d->g;
      l->b = d->b;
      l->attenuate = (float)d->attenuate;
      /* behind the eye with all of its reach: nothing it lights shows */
      fwd = l->x * fx + l->y * fy;
      if (fwd < -r)
         continue;
      /* how far the eye is from its reach: the nearest edges win, so a
       * big light over a room is kept over a candle at the eye's side */
      dist[n] = (float)sqrt(l->x * l->x + l->y * l->y + l->z * l->z) - r;
      n++;
   }
   /* the map's own lights */
   for (i = 0; dopo_maplight && i < dl_map_n && n < 1024; i++)
   {
      dopo_light_t *l = &all[n];
      const dl_def_t *d = &dl_map[i].d;
      mobj_t where;
      float r, fwd;
      where.x = dl_map[i].x;
      where.y = dl_map[i].y;
      r = 2 * dl_radius(d, &where);
      if (dl_map[i].sector)
         r *= dl_map[i].sector->lightlevel / 255.0f;
      if (r < 1)
         continue;
      l->x = dl_map[i].x / 65536.0f - vx;
      l->y = dl_map[i].y / 65536.0f - vy;
      l->z = dl_map[i].z / 65536.0f - vz;
      l->radius = r;
      l->r = d->r;
      l->g = d->g;
      l->b = d->b;
      l->attenuate = (float)d->attenuate;
      fwd = l->x * fx + l->y * fy;
      if (fwd < -r)
         continue;
      /* how far the eye is from its reach: the nearest edges win, so a
       * big light over a room is kept over a candle at the eye's side */
      dist[n] = (float)sqrt(l->x * l->x + l->y * l->y + l->z * l->z) - r;
      n++;
   }
   /* the max with the nearest reach */
   for (i = 0; i < n && i < max; i++)
   {
      int best = i;
      for (j = i + 1; j < n; j++)
         if (dist[j] < dist[best])
            best = j;
      out[i] = all[best];
      all[best] = all[i];
      dist[best] = dist[i];
   }
   return i;
}
/* @endpatch */

/**
 * @brief The software renderer's GLDEFS lights stand down while the GPU
 * lights.
 *
 * @patch src/r_dynlight.c 40
 */
  if (dopo_gl && dopo_dynlight)
    return;
/* @endpatch */

/**
 * @brief One member of a pk3, by its path (lights.pk3's GLDEFS).
 *
 * @patch src/w_pk3.c 766
 */
unsigned char *W_PK3ExtractPath(const unsigned char *zip, int64_t zip_length,
                                const char *path, int *out_length)
{
  pk3_zip_t      z;
  unsigned char *out = NULL;
  int            i;

  if (zip_length > (int64_t)0x7fffffff || !pk3_zip_open(&z, zip, (int)zip_length))
    return NULL;
  for (i = 0; i < z.n; i++)
    if (!z.ent[i].is_dir && !strcasecmp(z.ent[i].name, path))
    {
      out = pk3_zip_extract(&z, &z.ent[i]);
      if (out && out_length)
        *out_length = (int)z.ent[i].usize;
      break;
    }
  pk3_zip_close(&z);
  return out;
}

/* @endpatch */

/** @patch src/p_mobj.c 62 */
#include "prboomtv/gpu/dynlights.h"
/* @endpatch */

/**
 * @brief The map's lights (things 9800 and up), taken like the other
 * ZDoom editor-only things around them.
 *
 * @patch src/p_mobj.c 2480
 */
      if (dopo_maplight_add(mthing->x, mthing->y, mthing->angle, thingtype))
        return;
/* @endpatch */

/** @patch src/p_setup.c 67 */
#include "prboomtv/gpu/dynlights.h"
/* @endpatch */

/** @patch src/p_setup.c 3097 */
   dopo_maplight_clear();       /* and last level's map lights */
/* @endpatch */
