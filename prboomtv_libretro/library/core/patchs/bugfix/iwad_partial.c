/**
 * @brief An IWAD of MAPxx maps short of Doom 2's thirty plays as Doom 2.
 * CheckIWAD gave commercial only to a full set (HACX has 22 maps): short
 * of it the mode stayed indetermined, the levels were looked up as ExMy,
 * none was found and the map loaded from lump -1 onwards (HACX's
 * DEHACKED read as sectors, then a crash). Only an IWAD left indetermined
 * with nothing but MAPxx maps is touched; Strife's, told by its ENDSTRF
 * lump, is not a Doom 2 and stays as it was.
 */

/** @patch src/d_main.c 866 */
    int strife = 0;
/* @endpatch */

/**
 * @brief The directory scan counts Strife's signature.
 *
 * @patch src/d_main.c 938
 */
        else if (!strncmp(fileinfo[length].name, "ENDSTRF", 7) &&
                 fileinfo[length].name[7] == 0)
          ++strife;
/* @endpatch */

/**
 * @brief The partial MAPxx set, after every full set.
 *
 * @patch src/d_main.c 1002
 */
    else if (cm && !sw && !rg && !ud && !strife)
    {
      lprintf(LO_INFO, "CheckIWAD: %d MAPxx maps, playing as DOOM 2\n", cm);
      *gmode  = commercial;
      *hassec = sc>=2;
    }
/* @endpatch */
