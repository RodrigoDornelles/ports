/**
 * @brief The SoundFont the MIDI music plays on: the one an m3u playlist
 * lists, else doom1.wad (or doom1.wad.gz, or doom1.m3u) plays on a
 * doom1.sf3 or doom1.sf2 next to it, any game on a soundfont.sf3 or
 * soundfont.sf2, the same places I_FindFile looks for a wad: the content's
 * folder, then system/prboom, then system. Upstream looked for a path of
 * its author's machine only.
 *
 * With a soundfont found, the default MIDI player (Adlib) plays on it; with
 * none, the OPL goes on as before.
 */

/**
 * @brief A SoundFont in an m3u playlist (a RIFF file of form sfbk, .sf2 or
 * .sf3, whatever its name) is the one the music plays on; upstream skipped
 * the line as no wad. The web player lists the bank picked on its page.
 *
 * @patch libretro/libretro.c 2273
 */
static char m3u_soundfont[PATH_MAX + 1];

static int is_soundfont(const char *path)
{
   unsigned char head[12];
   int64_t got;
   RFILE *fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fp)
      return 0;
   got = filestream_read(fp, head, sizeof(head));
   filestream_close(fp);
   return got == (int64_t)sizeof(head) && !memcmp(head, "RIFF", 4)
       && !memcmp(head + 8, "sfbk", 4);
}
/* @endpatch */

/** @patch libretro/libretro.c 2333 */
      if (is_soundfont(resolved))
      {
         strlcpy(m3u_soundfont, resolved, sizeof(m3u_soundfont));
         if (log_cb)
            log_cb(RETRO_LOG_INFO, "m3u: soundfont '%s'\n", resolved);
         continue;
      }
/* @endpatch */

/** @brief Each content starts with no playlist soundfont.
 *
 * @patch libretro/libretro.c 2404
 */
      m3u_soundfont[0] = '\0';
/* @endpatch */

/**
 * @brief I_FindSoundfont: the path of the soundfont to load (malloc'd, the
 * caller frees it) or NULL: the playlist's, then .sf3 before .sf2 of each
 * name.
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

   if (m3u_soundfont[0])
      return strdup(m3u_soundfont);
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

/**
 * @brief A soundfont that does not load (truncated, not a SoundFont) is no
 * soundfont: fluidlite kept the half-made font in the synth's list, over a
 * freed bank, and returned its id as a success, so the first note crashed.
 *
 * @patch deps/fluidlite/src/fluid_synth.c 2532-2532
 */
    if (fluid_sfloader_load(loader, filename) == NULL) {
      loader->data = NULL;
      synth->sfont = fluid_list_remove(synth->sfont, sfont);
      FLUID_FREE(sfont);
      synth->sfont_id--;
      continue;
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
