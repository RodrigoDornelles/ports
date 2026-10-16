/**
 * @brief In 16:9 the 320x200 page is drawn 4:3 in the middle of the
 * screen instead of stretched over all of it (widescreen/ratio.c):
 * patches, rectangles and full-screen pages map through the page's
 * width and offset, and full-screen pages get the border flat on the
 * sides.
 */

/**
 * @brief The page geometry.
 *
 * @patch src/v_video.c 49
 */
#include "prboomtv/widescreen.h"
/* @endpatch */

/**
 * @brief Stretched rectangles go through the page too.
 *
 * @patch src/v_video.c 279-284
 */
    const int page = dopo_wide_width();
    srcx=dopo_wide_offset(flags, srcx)+srcx*page/320;
    srcy=srcy*SCREENHEIGHT/200;
    width=width*page/320;
    height=height*SCREENHEIGHT/200;
    destx=dopo_wide_offset(flags, destx)+destx*page/320;
    desty=desty*SCREENHEIGHT/200;
/* @endpatch */

/**
 * @brief Patches scale to the page width.
 *
 * @patch src/v_video.c 621-622
 */
   int DX  = (dopo_wide_width()<<16) / 320;
   int DXI = (320<<16) / dopo_wide_width();
/* @endpatch */

/**
 * @brief A hi-res full-screen page fits the page, not the screen.
 *
 * @patch src/v_video.c 637-638
 */
         DX  = (dopo_wide_width() << 16) / patch->width;
         DXI = (patch->width << 16) / dopo_wide_width();
/* @endpatch */

/**
 * @brief Stretched patches start where the page (or the HUD's screen
 * edge) does.
 *
 * @patch src/v_video.c 686-688
 */
   {
   const int xoff = (flags & VPT_STRETCH) ? dopo_wide_offset(flags, x) : 0;
   left   = (( x * DX ) >> FRACBITS) + xoff;
   top    = ( y * DY ) >> FRACBITS;
   right  = (( (x + patch->width) * DX ) >> FRACBITS) + xoff;
   }
/* @endpatch */

/**
 * @brief A full-screen page has the border flat around it.
 *
 * @patch src/v_video.c 824
 */
  if (flags & VPT_STRETCH)
    dopo_fill_wings(scrn, 0, 0, SCREENHEIGHT);
/* @endpatch */

/**
 * @brief Native-colour pages always fill the screen, so in 16:9 they
 * take the patch path, which keeps them 4:3.
 *
 * @patch src/v_video.c 988
 */
  if (dopo_wide_width() != SCREENWIDTH)
    return 0;
/* @endpatch */

/**
 * @brief The cached full-screen pages have the border flat around them.
 *
 * @patch src/v_video.c 1114
 */
  dopo_fill_wings(scrn, 0, 0, SCREENHEIGHT);
/* @endpatch */

/**
 * @brief Boxes are drawn on the page.
 *
 * @patch src/v_video.c 1763-1764
 */
  ax = dopo_wide_x(ax);
  bx = dopo_wide_x(bx);
/* @endpatch */
