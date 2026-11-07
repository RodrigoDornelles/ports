/*
 * The OPL synth of the Music tab: upstream's oplplayer and dbopl, with the
 * few lumps and zone calls they ask for answered here. The page starts it
 * with the GENMIDI lump of the game.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <emscripten.h>

/* the player's own file, for running_tracks: the song ends when it is 0 */
#include "oplplayer.c"
#include "lprintf.h"
#include "music.h"

int mus_opl_gain = 50;

static const void *genmidi;

int (W_FindNumFromName)(const char *name, lumpinfo_namespace_t ns, int lump)
{
  return genmidi && lump < 0 && !strcasecmp(name, "GENMIDI") ? 0 : -1;
}

int W_GetNumForName(const char *name)
{
  return 0;
}

const void *W_CacheLumpNum(int lump)
{
  return genmidi;
}

void W_UnlockLumpNum(int lump)
{
}

void *(Z_Malloc)(size_t size, int tag, void **ptr)
{
  return malloc(size);
}

void (Z_Free)(void *ptr)
{
  free(ptr);
}

int lprintf(OutputLevels pri, const char *fmt, ...)
{
  return 0;
}

/* GENMIDI must stay alive while the synth is: the instruments point into it */
EMSCRIPTEN_KEEPALIVE int music_opl_init(int samplerate, const void *lump)
{
  if (music_initialized)
    I_OPL_ShutdownMusic();
  genmidi = lump;
  if (!I_OPL_InitMusic(samplerate))
    return 0;
  I_OPL_SetMusicVolume(15);
  return 1;
}

static int opl_playing(void)
{
  return running_tracks > 0;
}

/* the chip goes on with nothing scheduled: the voices fade as they would */
const music_synth_t music_opl = { &opl_synth_player, opl_playing, I_OPL_RenderSamplesFloat };
