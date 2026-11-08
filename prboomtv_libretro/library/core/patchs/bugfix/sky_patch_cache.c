/**
 * @brief The patch cache comes up before the sky: on Heretic, R_InitSkyMap
 * reads the sky's patch when its texture has one patch, and at R_Init that
 * stopped every wad whose first texture does (Blasphemer) with "Patches not
 * initialized".
 *
 * @patch src/r_main.c 615-620
 */
  lprintf(LO_INFO, "R_InitTranslationsTables\n");
  R_InitTranslationTables();
  lprintf(LO_INFO, "R_InitPatches\n");
  R_InitPatches();
  lprintf(LO_INFO, "R_InitSkyMap\n");
  R_InitSkyMap();
/* @endpatch */
