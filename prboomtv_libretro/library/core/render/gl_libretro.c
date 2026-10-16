/**
 * @brief prboom-render = gl on the frontend side (render/gl.c): the GL
 * context is asked for when the game loads, and the software frame the
 * CPU still draws is 16-bit, the format the GPU lays over its columns.
 * The frame goes out through dopo_gl_present (widescreen/libretro.c).
 */

/** @patch libretro/libretro.c 64 */
#include "prboomtv/render_gl.h"
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
