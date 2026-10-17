/**
 * @brief prboom-render = gl on the frontend side (enhancements/gpu/gl.c): the GL
 * context is asked for when the game loads, and the software frame the
 * CPU still draws is 16-bit, the format the GPU lays over its columns.
 * The frame goes out through dopo_gl_present (enhancements/widescreen/libretro.c).
 */

/** @patch libretro/libretro.c 64 */
#include "prboomtv/render_gl.h"
#include "prboomtv/scale2x.h"
#include "prboomtv/glow.h"
/* @endpatch */

/**
 * @brief The software frame of gl is always RGB565.
 *
 * @patch libretro/libretro.c 1400
 */
   if (dopo_gl)
      want = VID_MODE565;
/* @endpatch */

/**
 * @brief prboom-render is read first: gl asks for the context, and a
 * frontend that refuses it fails the load.
 *
 * @patch libretro/libretro.c 2372
 */
   if (!dopo_gl_load(environ_cb))
      return false;
/* @endpatch */

/**
 * @brief The software frame drawn straight into the frontend's buffer is
 * counted too (enhancements/gpu/gl.c), to compare with gl.
 *
 * @patch libretro/libretro.c 3919
 */
      dopo_gl_software_frame();
/* @endpatch */

/**
 * @brief prboom-spectre, prboom-scale and prboom-glow can change while
 * playing.
 *
 * @patch libretro/libretro.c 1525
 */
   dopo_gl_spectre_read(environ_cb);
   dopo_scale_read(environ_cb);
   dopo_glow_read(environ_cb);
/* @endpatch */
