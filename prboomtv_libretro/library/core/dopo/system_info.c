/**
 * @brief The core's name and version for the frontend: PrBoomTV, so a
 * frontend tells it apart from the PrBoom core it forks (RetroArch netplay
 * refuses a different core name), and the version of include/prboomtv/version.h.
 */

/**
 * @brief Version declarations.
 *
 * @patch libretro/libretro.c 1020
 */
#include "prboomtv/version.h"

/* @endpatch */

/**
 * @brief Name and version.
 *
 * @patch libretro/libretro.c 1023-1027
 */
   info->library_name     = "PrBoomTV";
   info->library_version  = DOPO_VERSION;
/* @endpatch */
