/**
 * @brief The SoundFont the MIDI music plays on: doom1.wad (or doom1.wad.gz,
 * or doom1.m3u) plays on a doom1.sf3 or doom1.sf2 next to it, any game on
 * a soundfont.sf3 or soundfont.sf2, the same places I_FindFile looks for a
 * wad: the content's folder, then system/prboom, then system. Upstream
 * looked for a path of its author's machine only.
 *
 * With a soundfont found, the default MIDI player (Adlib) plays on it; with
 * none, the OPL goes on as before.
 */

/**
 * @brief I_FindSoundfont: the path of the soundfont to load (malloc'd, the
 * caller frees it) or NULL; .sf3 before .sf2 of each name.
 *
 * @patch libretro/libretro.c 4235
 */
char *I_FindSoundfont(void)
{
   char name[1024];
   const char *names[2];
   static const char *const exts[] = { ".sf3", ".sf2" };
   char *p;
   int i, j;

   remove_extension(name, g_basename, sizeof(name));
   names[0] = name;
   names[1] = "soundfont";
   for (i = 0; i < 2; i++)
   {
      if (!names[i][0])
         continue;
      for (j = 0; j < 2; j++)
         if ((p = I_FindFile(names[i], exts[j])))
            return p;
   }
   return NULL;
}

/* @endpatch */

/** @patch src/flplayer.c 186-187 */
  {
    char *found = I_FindSoundfont();
    f_font = found ? fluid_synth_sfload (f_syn, found, 1) : FLUID_FAILED;
    if (found && f_font != FLUID_FAILED)
      log_cb (RETRO_LOG_INFO, "fl_init: soundfont %s\n", found);
    free (found);
  }
/* @endpatch */

/** @patch src/flplayer.c 191-191 */
    log_cb (RETRO_LOG_INFO, "fl_init: no soundfont, MIDI stays on the OPL\n");
/* @endpatch */

/**
 * @brief I_FluidAvailable: a soundfont loaded, so the synth can play.
 *
 * @patch src/flplayer.c 703
 */
int I_FluidAvailable (void)
{
  return f_syn != NULL;
}
/* @endpatch */

/** @patch src/flplayer.c 82 */
char *I_FindSoundfont(void);
/* @endpatch */

/** @patch libretro/libretro_sound.c 101 */
int I_FluidAvailable(void);
/* @endpatch */

/** @patch libretro/libretro_sound.c 1320-1320 */
        chosen_midi = I_FluidAvailable()
                    ? (music_player_t *)&fl_player
                    : (music_player_t *)&opl_synth_player;
/* @endpatch */

/** @patch src/flplayer.c 124-124 */
/* @endpatch */
