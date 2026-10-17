/**
 * @brief The prboom-widescreen core option on the frontend side: the
 * option itself, the 16:9 resolutions, the aspect it forces and the 16:9
 * frame around the classic 4:3 game (enhancements/widescreen/widescreen.c).
 */

/**
 * @brief Widescreen and its helpers; options also come from the
 * environment.
 *
 * @patch libretro/libretro.c 63
 */
#include "prboomtv/options.h"
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief The 4:3 width is shared with the 2D drawing, which keeps the
 * 320x200 page that wide in widescreen.
 *
 * @patch libretro/libretro.c 81-81
 */
int base_width_43 = 320;
/* @endpatch */

/**
 * @brief Without widescreen, the 4:3 game is shown in a 16:9 frame.
 *
 * @patch libretro/libretro.c 1176
 */
  if (!dopo_widescreen)
  {
    info->geometry.base_width   = dopo_pillar_width();
    info->geometry.max_width    = info->geometry.base_width;
    advertised_max_width        = info->geometry.max_width;
    info->geometry.aspect_ratio = 16.0 / 9.0;
  }
/* @endpatch */

/**
 * @brief Options are read from the environment first (PRBOOM_RESOLUTION
 * for prboom-resolution), then from the frontend.
 *
 * @patch libretro/libretro.c 1194-1194
 */
	environ_cb = dopo_options_environment(cb);
/* @endpatch */

/**
 * @brief prboom-widescreen joins the options, the resolutions are 16:9.
 *
 * @patch libretro/libretro.c 1196
 */
   dopo_widescreen_options();
/* @endpatch */

/**
 * @brief Widescreen is read with the resolution, both at startup.
 *
 * @patch libretro/libretro.c 1563
 */
      dopo_widescreen_read(environ_cb);
/* @endpatch */

/**
 * @brief With no resolution from the frontend, the option's default.
 *
 * @patch libretro/libretro.c 1633-1634
 */
         SCREENWIDTH  = 640;
         SCREENHEIGHT = 360;
/* @endpatch */

/**
 * @brief The screen is 16:9; the game is laid out on its 4:3 area, as
 * tall and 4/3 as wide (a multiple of 4 for the column drawer), which
 * the aspect widens back to the screen (I_AspectWidthFor).
 *
 * @patch libretro/libretro.c 1642-1642
 */
      if (SCREENHEIGHT < 240)
         SCREENHEIGHT = 240;
      SCREENWIDTH = (SCREENHEIGHT * 4 / 3) & ~3;
      SCREENPITCH = (SCREENWIDTH * SURFACE_PIXEL_DEPTH);
      base_width_43 = SCREENWIDTH;
/* @endpatch */

/**
 * @brief The 4:3 game goes out inside its 16:9 frame; in gl, through the
 * GPU (enhancements/gpu/gl.c).
 *
 * @patch libretro/libretro.c 3940-3940
 */
   {
      unsigned width = SCREENWIDTH;
      size_t pitch = SCREENPITCH;
      const void *frame = screen_buf;
      if (!dopo_widescreen)
         frame = dopo_pillarbox(screen_buf, &width, &pitch);
      if (dopo_gl)
         dopo_gl_present(video_cb, frame, width, SCREENHEIGHT, pitch);
      else
      {
         dopo_gl_software_frame();
         video_cb(frame, width, SCREENHEIGHT, pitch);
      }
   }
/* @endpatch */

/**
 * @brief Widescreen decides the aspect: 16:9 widens the view, 4:3 keeps
 * it (the menu's Aspect Ratio is gone).
 *
 * @patch libretro/libretro.c 4012
 */
   render_aspect = dopo_widescreen ? 1 : 0;
/* @endpatch */

/**
 * @brief The 4:3 game is copied into its frame, so it never renders
 * straight into the frontend's buffer; nor does gl, whose frame goes to
 * the GPU.
 *
 * @patch libretro/libretro.c 4092-4092
 */
   if (in_retro_run && environ_cb && dopo_widescreen && !dopo_gl)
/* @endpatch */
