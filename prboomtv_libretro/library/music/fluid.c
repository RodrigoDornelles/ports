/*
 * The SoundFont synth of the Music tab: upstream's flplayer on fluidlite,
 * the core's patched copies (the drums on channel 10, .sf3 through
 * rvorbis, library/core/enhancements/midi). The page writes the .sf2 or
 * .sf3 in the module's file system and starts it with its path.
 */
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include <libretro.h>

/* the player's own file, for f_playing (0 after the end of the song) and
 * the synth behind it */
#include "flplayer.c"
#include "music.h"

static void music_log(enum retro_log_level level, const char *fmt, ...)
{
}

retro_log_printf_t log_cb = music_log;

static const char *soundfont;

/* where the patched fl_init looks for the soundfont of the game */
char *I_FindSoundfont(void)
{
  return soundfont ? strdup(soundfont) : NULL;
}

/* the samples are read whole: the file can go once this returns */
EMSCRIPTEN_KEEPALIVE int music_fluid_init(int samplerate, const char *path)
{
  int ready;

  fl_shutdown();
  soundfont = path;
  ready = fl_init(samplerate);
  soundfont = NULL;
  if (ready)
    fl_setvolume(15);
  return ready;
}

static int fluid_playing(void)
{
  return f_playing;
}

/* fl_render is silent once stopped: the synth itself rings out */
static void fluid_tail(void *dest, unsigned nsamp)
{
  fl_writesamples_ex_f(dest, nsamp);
}

const music_synth_t music_fluid = { &fl_player, fluid_playing, fluid_tail };
