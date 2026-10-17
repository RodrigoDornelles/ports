/**
 * @file widescreen.c
 * @brief The prboom-widescreen core option and what does not fit in a
 * patch: the 2D page geometry, the border flat on the sides and the 16:9
 * frame around the classic 4:3 game.
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "doomdef.h"
#include "doomstat.h"
#include "v_video.h"
#include "w_wad.h"
#include "prboomtv/frameskip.h"
#include "prboomtv/render_gl.h"
#include "prboomtv/scale2x.h"
#include "prboomtv/glow.h"
#include "prboomtv/dynlights.h"
#include "prboomtv/widescreen.h"

dbool dopo_widescreen = TRUE;
dbool dopo_hud_align = FALSE;
int dopo_hud_line_x = -1;

/* the upstream option table (libretro_core_options.h, a header the
 * patches cannot reach), changed at runtime */
extern struct retro_core_option_v2_definition option_defs_us[];
extern struct retro_core_options_v2 options_us;

static const struct retro_core_option_v2_definition dopo_widescreen_def =
{
   "prboom-widescreen",
   "Widescreen (Restart Required)",
   NULL,
   "Widens the 3D view to fill the screen, keeping the status bar, HUD and "
   "menus in their original proportions. Disabled, the classic 4:3 game is "
   "shown with borders on the sides.",
   NULL,
   NULL,
   {
      { "enabled",  NULL },
      { "disabled", NULL },
      { NULL, NULL },
   },
   "enabled"
};

/* the screen is always 16:9: the size of the widescreen game, or of the
 * frame around the 4:3 one */
static const struct retro_core_option_value dopo_resolutions[] =
{
   { "640x360",   NULL },
   { "960x540",   NULL },
   { "1280x720",  "1280x720 (HD)" },
   { "1600x900",  NULL },
   { "1920x1080", "1920x1080 (Full HD)" },
   { "2560x1440", "2560x1440 (Quad HD)" },
   { NULL, NULL },
};

void dopo_widescreen_options(void)
{
   static struct retro_core_option_v2_definition defs[64];
   size_t count = 0;
   size_t i;

   for (i = 0; option_defs_us[i].key; i++)
   {
      if (count + 10 > sizeof(defs) / sizeof(*defs))
         return;
      if (!strcmp(option_defs_us[i].key, "prboom-resolution"))
      {
         /* widescreen first, then its resolutions */
         defs[count++] = dopo_widescreen_def;
         defs[count] = option_defs_us[i];
         memset(defs[count].values, 0, sizeof(defs[count].values));
         memcpy(defs[count].values, dopo_resolutions, sizeof(dopo_resolutions));
         defs[count++].default_value = "640x360";
      }
      else if (!strcmp(option_defs_us[i].key, "prboom-render_threads"))
      {
         /* who paints, how many threads fill (enhancements/gpu/gl.c), how many
          * frames are drawn (dopo/frameskip.c) */
         defs[count++] = dopo_gl_option;
         defs[count++] = dopo_gl_spectre_option;
         defs[count++] = dopo_gl_spectre_opacity_option;
         defs[count++] = dopo_scale_option;
         defs[count++] = dopo_glow_option;
         defs[count++] = dopo_dynlight_option;
         defs[count++] = dopo_maplight_option;
         defs[count++] = option_defs_us[i];
         defs[count++] = dopo_frameskip_option;
      }
      else
         defs[count++] = option_defs_us[i];
   }
   memset(&defs[count], 0, sizeof(*defs));
   options_us.definitions = defs;
}

void dopo_widescreen_read(retro_environment_t environ_cb)
{
   /* the option says disabled; the environment may say it other ways */
   static const char *const off[] = { "disabled", "disable", "off", "false", "no", "0" };
   struct retro_variable var = { "prboom-widescreen", NULL };
   size_t i;

   dopo_widescreen = TRUE;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)
      return;
   for (i = 0; i < sizeof(off) / sizeof(*off); i++)
      if (!strcasecmp(var.value, off[i]))
         dopo_widescreen = FALSE;
}

int dopo_wide_width(void)
{
   if (!dopo_widescreen || base_width_43 <= 0 || base_width_43 >= SCREENWIDTH)
      return SCREENWIDTH;
   return base_width_43;
}

int dopo_wide_offset(int flags, int x)
{
   const int offset = (SCREENWIDTH - dopo_wide_width()) / 2;
   int align = flags & (DOPO_VPT_ALIGN_LEFT | DOPO_VPT_ALIGN_RIGHT);

   if (offset <= 0)
      return 0;
   if (!align && dopo_hud_align)
      align = (dopo_hud_line_x >= 0 ? dopo_hud_line_x : x) < 160
         ? DOPO_VPT_ALIGN_LEFT : DOPO_VPT_ALIGN_RIGHT;
   if (align == DOPO_VPT_ALIGN_LEFT)
      return 0;
   if (align == DOPO_VPT_ALIGN_RIGHT)
      return offset * 2;
   return offset;
}

int dopo_wide_x(int x)
{
   return dopo_wide_offset(0, x) + x * dopo_wide_width() / 320;
}

int dopo_wide_w(int w)
{
   return w * dopo_wide_width() / 320;
}

/* the flat vanilla tiles around a shrunk view */
static const char *dopo_border_flat(void)
{
   if (hexen)
      return "F_022";
   if (heretic)
      return "FLAT513";
   return gamemode == commercial ? "GRNROCK" : "FLOOR7_2";
}

/* the border flat tiled over a whole screen of the given width.
 * V_DrawBackground scales its tile by SCREENWIDTH/320, so it draws on a
 * stand-in screen 0 as wide as the 4:3 area, where the tile is square,
 * and that is repeated sideways (the area is five tiles wide) */
static void dopo_border_draw(uint8_t *buf, int width)
{
   static uint8_t *tile = NULL;
   const char *flat         = dopo_border_flat();
   const int depth          = SURFACE_PIXEL_DEPTH;
   const int area           = base_width_43 > 0 && base_width_43 < width ? base_width_43 : width;
   const int saved_width    = SCREENWIDTH;
   const int saved_pitch    = SCREENPITCH;
   unsigned char *saved_fg  = screens[0].data;
   uint8_t *grown;
   int row;

   memset(buf, 0, (size_t)width * depth * SCREENHEIGHT);
   if ((W_CheckNumForName)(flat, ns_flats) < 0)
      return;
   grown = (uint8_t *)realloc(tile, (size_t)area * depth * SCREENHEIGHT);
   if (!grown)
      return;
   tile = grown;

   SCREENWIDTH     = area;
   SCREENPITCH     = area * depth;
   screens[0].data = tile;
   V_DrawBackground(flat, 0);
   SCREENWIDTH     = saved_width;
   SCREENPITCH     = saved_pitch;
   screens[0].data = saved_fg;

   for (row = 0; row < SCREENHEIGHT; row++)
   {
      const uint8_t *src = tile + (size_t)area * depth * row;
      uint8_t *dest      = buf + (size_t)width * depth * row;
      int x;

      for (x = 0; x < width; x += area)
         memcpy(dest + (size_t)x * depth, src,
                (size_t)(width - x < area ? width - x : area) * depth);
   }
}

void dopo_fill_wings(int scrn, int y, int src_y, int h)
{
   static uint8_t *wings = NULL;
   static int wings_width = -1;
   static int wings_height = -1;
   static int wings_depth = -1;
   static const uint16_t *wings_palette = NULL;
   const int width  = dopo_wide_width();
   const int offset = (SCREENWIDTH - width) / 2;
   const int depth  = SURFACE_PIXEL_DEPTH;
   const int right  = offset + width;
   int row;

   if (offset <= 0 || h <= 0)
      return;

   if (!wings || wings_width != SCREENWIDTH || wings_height != SCREENHEIGHT
         || wings_depth != depth || wings_palette != V_Palette16)
   {
      uint8_t *buf = (uint8_t *)realloc(wings, (size_t)SCREENPITCH * SCREENHEIGHT);
      if (!buf)
         return;
      wings = buf;
      dopo_border_draw(wings, SCREENWIDTH);
      wings_width   = SCREENWIDTH;
      wings_height  = SCREENHEIGHT;
      wings_depth   = depth;
      wings_palette = V_Palette16;
      dopo_gl_2d_image_dirty(wings);
   }

   /* in gl, the two sides as GPU images of the same rows */
   if (scrn == 0 && src_y >= 0 && src_y + h <= SCREENHEIGHT
         && dopo_gl_2d_image(wings, SCREENWIDTH, SCREENHEIGHT, 0, src_y, offset, h, 0, y)
         && dopo_gl_2d_image(wings, SCREENWIDTH, SCREENHEIGHT, right, src_y,
                             SCREENWIDTH - right, h, right, y))
      return;
   if (scrn == 0)
      dopo_gl_dirty_rect(0, y, SCREENWIDTH, y + h);
   else
      dopo_gl_2d_screen_dirty(scrn);

   for (row = 0; row < h; row++)
   {
      int src_row = src_y + row;
      const uint8_t *src;
      uint8_t *dest;

      if (src_row < 0)
         src_row = 0;
      if (src_row >= SCREENHEIGHT)
         src_row = SCREENHEIGHT - 1;
      src  = wings + (size_t)SCREENPITCH * src_row;
      dest = screens[scrn].data + (size_t)SCREENPITCH * (y + row);
      memcpy(dest, src, (size_t)offset * depth);
      memcpy(dest + (size_t)right * depth, src + (size_t)right * depth,
             (size_t)(SCREENWIDTH - right) * depth);
   }
}

int dopo_pillar_width(void)
{
   /* the 16:9 width of I_AspectWidthFor, from the 4:3 one */
   int width = (int)(((int64_t)base_width_43 * 16 * 3) / (9 * 4)) & ~3;

   if (width > MAX_SCREENWIDTH)
      width = MAX_SCREENWIDTH;
   if (width < SCREENWIDTH)
      width = SCREENWIDTH;
   return width;
}

const void *dopo_pillarbox(const void *frame, unsigned *width, size_t *pitch)
{
   static uint8_t *out = NULL;
   static int out_width = -1;
   static int out_height = -1;
   static int out_depth = -1;
   static const uint16_t *out_palette = NULL;
   const int frame_width = dopo_pillar_width();
   const int depth       = SURFACE_PIXEL_DEPTH;
   const size_t out_pitch = (size_t)frame_width * depth;
   const int offset      = (frame_width - SCREENWIDTH) / 2;
   int row;

   /* the borders are drawn once per palette, the game is copied over the
    * middle every frame */
   if (!out || out_width != frame_width || out_height != SCREENHEIGHT
         || out_depth != depth || out_palette != V_Palette16)
   {
      uint8_t *buf = (uint8_t *)realloc(out, out_pitch * SCREENHEIGHT);
      if (!buf)
      {
         *width = SCREENWIDTH;
         *pitch = SCREENPITCH;
         return frame;
      }
      out = buf;
      dopo_border_draw(out, frame_width);
      out_width   = frame_width;
      out_height  = SCREENHEIGHT;
      out_depth   = depth;
      out_palette = V_Palette16;
   }

   for (row = 0; row < SCREENHEIGHT; row++)
      memcpy(out + out_pitch * row + (size_t)offset * depth,
             (const uint8_t *)frame + (size_t)SCREENPITCH * row,
             (size_t)SCREENWIDTH * depth);

   *width = frame_width;
   *pitch = out_pitch;
   return out;
}
