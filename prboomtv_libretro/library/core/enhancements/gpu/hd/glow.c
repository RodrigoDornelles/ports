/**
 * @file glow.c
 * @brief The prboom-glow core option (prboomtv/gpu/glow.h).
 *
 * Only the full bright frames of a list of sprites glow, the ones GZDoom
 * draws with RenderStyle Add: the monsters' and the player's projectiles
 * and their explosions, the barrel's explosion, the Arch-vile's fire, the
 * teleport fog, the bullet puff's spark, and the weapon's flash layer.
 * Every full bright frame would also catch a monster's attack frame or a
 * lamp's pole, which would turn see-through. With the gl renderer only, a
 * glowing sprite becomes RenderStyle Add, which gl.c draws with an
 * additive blend (and a corona with prboom-glow 2); the software renderer
 * draws as before. The module is a hunk at the top of r_things.c, where
 * the sprites are projected.
 */

/**
 * @brief The module: the option and the list.
 *
 * @patch src/r_things.c 57
 */
#include <string.h>
#include <strings.h>

#include "prboomtv/gpu/brightmaps.h"
#include "prboomtv/gpu/glow.h"
#include "prboomtv/gpu/render_gl.h"

const struct retro_core_option_v2_definition dopo_glow_option =
{
   "prboom-glow",
   "Fire Glow (gl only)",
   NULL,
   "Draws fireballs, plasma, explosions, teleport fog and the muzzle "
   "flash additively, so they light up what is behind them instead of "
   "covering it, as GZDoom does; only with the gl renderer. 'corona' adds "
   "a soft halo around them.",
   NULL,
   NULL,
   {
      { "0", "Off" },
      { "1", "Additive" },
      { "2", "Additive + Corona" },
      { NULL, NULL },
   },
   "2"
};

int dopo_glow = 2;

void dopo_glow_read(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-glow", NULL };

   dopo_glow = 2;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
         && (!strcmp(var.value, "0") || !strcmp(var.value, "1")))
      dopo_glow = var.value[0] - '0';
}

/* the sprites that glow, from which frame to which (the rocket itself
 * does not, its explosion does; the imp's, caco's and baron's balls glow
 * while they fly, their dark red burst would read as blood) */
static const struct
{
   char name[5];
   int  first, last;
} glow_sprites[] =
{
   { "BAL1", 0, 1 }, { "BAL2", 0, 1 }, { "BAL7", 0, 1 },  /* imp, caco, baron balls */
   { "PLSS", 0, 99 }, { "PLSE", 0, 99 },                  /* plasma */
   { "BFS1", 0, 99 }, { "BFE1", 0, 99 }, { "BFE2", 0, 99 },  /* BFG ball, blast, spray */
   { "APLS", 0, 99 }, { "APBX", 0, 99 },                  /* arachnotron plasma */
   { "MANF", 0, 99 },                                     /* mancubus fireball */
   { "FBXP", 0, 99 },                                     /* revenant rocket blast */
   { "MISL", 1, 99 },                                     /* rocket blast */
   { "BEXP", 0, 99 },                                     /* barrel blast */
   { "FIRE", 0, 99 },                                     /* arch-vile fire */
   { "TFOG", 0, 99 }, { "IFOG", 0, 99 },                  /* teleport, item fog */
   { "PUFF", 0, 99 },                                     /* bullet puff spark */
};

void dopo_glow_mark(vissprite_t *vis, int sprite, int frame)
{
   size_t i;

   if (!dopo_gl || !dopo_glow || !vis->emissive || vis->translucent)
      return;
   if (sprite >= 0)
   {
      const char *name = sprnames[sprite];
      for (i = 0; i < sizeof(glow_sprites) / sizeof(*glow_sprites); i++)
         if (name && !strncasecmp(name, glow_sprites[i].name, 4))
            break;
      if (i == sizeof(glow_sprites) / sizeof(*glow_sprites)
            || (frame & FF_FRAMEMASK) < glow_sprites[i].first
            || (frame & FF_FRAMEMASK) > glow_sprites[i].last)
         return;
   }
   vis->translucent = 2;
   vis->alpha = 32;
}
/* @endpatch */

/**
 * @brief A thing that glows; first, a full bright frame whose brightmap
 * keeps it in the room's light (prboomtv/gpu/brightmaps.h).
 *
 * @patch src/r_things.c 1714
 */
   if ((thing->frame & FF_FULLBRIGHT) && !fixedcolormap)
      dopo_brightmap_project(vis, R_ColourMap(lightlevel, xscale));
   dopo_glow_mark(vis, thing->sprite, thing->frame);
/* @endpatch */

/**
 * @brief The weapon's flash layer glows.
 *
 * @patch src/r_things.c 1935
 */
   if (psp == &viewplayer->psprites[ps_flash])
      dopo_glow_mark(vis, -1, psp->state->frame);
/* @endpatch */
