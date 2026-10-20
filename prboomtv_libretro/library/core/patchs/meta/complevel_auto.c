/**
 * @brief prboom-complevel 'Default (Auto)' follows what the wad asks for,
 * as dsda-doom does: its COMPLVL lump (vanilla, boom, mbf, mbf21), else
 * MBF21 when a patch of the wad declares "Doom version = 2021" (DECOHack
 * writes it in every MBF21 patch). A wad that asks for nothing still gets
 * the latest PrBoom level, as before. Auto only ever resolved to that one,
 * so an MBF21 wad played with none of its MBF21 code pointers, flags or
 * arguments unless the level was forced by hand; a level forced in the
 * option still wins, and demos keep the level of their header.
 */

/**
 * @brief A patch declared MBF21.
 *
 * @patch src/d_deh.c 3207
 */
int dopo_deh_mbf21 = 0;
/* @endpatch */

/** @patch src/d_deh.c 3212 */
  {
    int version;
    if (!strncasecmp(line, "Doom version", 12)
        && sscanf(line + 12, " = %d", &version) == 1 && version == 2021)
      dopo_deh_mbf21 = 1;
  }
/* @endpatch */

/**
 * @brief Only the wad's own patches count, those loaded from here on: an
 * IWAD's or an autoloaded one says nothing about the wad being played.
 *
 * @patch src/d_main.c 1726
 */
  {
    extern int dopo_deh_mbf21;
    dopo_deh_mbf21 = 0;
  }
/* @endpatch */

/**
 * @brief The level the wad asks for, when the option is on auto. Before
 * the wads are loaded (the first call, at startup) there is nothing to
 * ask; a new game asks again.
 *
 * @patch src/g_game.c 2816
 */
  if (compatibility_level == (unsigned) -1 && numlumps)
  {
    extern int dopo_deh_mbf21;
    int lump = W_CheckNumForName("COMPLVL");
    if (lump >= 0)
    {
      const char *data = (const char *)W_CacheLumpNum(lump);
      int len = W_LumpLength(lump), i = 0, n = 0;
      char word[8];
      /* the first word, lower case (g_game.c has no ctype.h) */
      while (i < len && (unsigned char)data[i] <= ' ')
        i++;
      while (i < len && n < (int)sizeof(word) - 1 && (unsigned char)data[i] > ' ')
      {
        char c = data[i++];
        word[n++] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
      }
      word[n] = '\0';
      W_UnlockLumpNum(lump);
      if (!strcmp(word, "vanilla"))
        compatibility_level = gamemission == pack_tnt || gamemission == pack_plut
                              ? finaldoom_compatibility
                              : gamemode == retail ? ultdoom_compatibility
                              : doom2_19_compatibility;
      else if (!strcmp(word, "boom"))
        compatibility_level = boom_202_compatibility;
      else if (!strcmp(word, "mbf"))
        compatibility_level = mbf_compatibility;
      else if (!strcmp(word, "mbf21"))
        compatibility_level = mbf21_compatibility;
      else
        lprintf(LO_WARN, "G_ReloadDefaults: COMPLVL '%s' unknown, level left on auto\n", word);
    }
    if (compatibility_level == (unsigned) -1 && dopo_deh_mbf21)
      compatibility_level = mbf21_compatibility;
    if (compatibility_level != (unsigned) -1)
      lprintf(LO_INFO, "G_ReloadDefaults: complevel %d, asked for by the wad (%s)\n",
              compatibility_level, lump >= 0 ? "COMPLVL" : "MBF21 patch");
  }
/* @endpatch */
