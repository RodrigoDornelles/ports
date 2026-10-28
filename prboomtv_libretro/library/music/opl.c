/*
 * The Music tab's synth: upstream's OPL player (mus2mid, midifile,
 * oplplayer, dbopl) built alone for the browser, with the few lumps and
 * zone calls it asks for answered here. The page hands it the GENMIDI
 * lump of the game and a song, MUS or MIDI, and gets the whole song back
 * as float stereo, rendered once without looping.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <emscripten.h>

/* the player's own file, for running_tracks: the song ends when it is 0 */
#include "oplplayer.c"
#include "lprintf.h"
#include "memio.h"
#include "mus2mid.h"

/* after the last note, the release of the voices still sounding */
#define MUSIC_TAIL_SECONDS 2
/* a song that never ends (a track without its end) stops there */
#define MUSIC_MAX_SECONDS (20 * 60)
#define MUSIC_CHUNK 4096

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
EMSCRIPTEN_KEEPALIVE int music_init(int samplerate, const void *lump)
{
  if (music_initialized)
    I_OPL_ShutdownMusic();
  genmidi = lump;
  if (!I_OPL_InitMusic(samplerate))
    return 0;
  I_OPL_SetMusicVolume(15);
  return 1;
}

/* the song as interleaved float stereo, malloc'd, its frames in *frames;
 * NULL when it is neither MUS nor MIDI the player takes */
EMSCRIPTEN_KEEPALIVE float *music_render(const void *data, unsigned len, int samplerate, int *frames)
{
  const void *song;
  void *midi = (void *) data;
  size_t midi_len = len;
  MEMFILE *out = NULL;
  float *pcm = NULL;
  size_t done = 0, cap = 0, tail = 0;
  size_t max = (size_t) samplerate * MUSIC_MAX_SECONDS;

  *frames = 0;
  if (!music_initialized)
    return NULL;
  if (len > 4 && !memcmp(data, "MUS\x1a", 4))
  {
    MEMFILE *in = mem_fopen_read(data, len);
    out = mem_fopen_write();
    int failed = mus2mid(in, out);
    mem_fclose(in);
    if (failed)
    {
      mem_fclose(out);
      return NULL;
    }
    mem_get_buf(out, &midi, &midi_len);
  }

  song = I_OPL_RegisterSong(midi, midi_len);
  if (!song)
  {
    if (out)
      mem_fclose(out);
    return NULL;
  }
  I_OPL_PlaySong(song, 0);

  while (done < max && tail < (size_t) samplerate * MUSIC_TAIL_SECONDS)
  {
    if (done + MUSIC_CHUNK > cap)
    {
      float *grown = realloc(pcm, (cap = cap ? cap * 2 : (size_t) samplerate * 60) * 2 * sizeof(float));
      if (!grown)
        break;
      pcm = grown;
    }
    I_OPL_RenderSamplesFloat(pcm + done * 2, MUSIC_CHUNK);
    done += MUSIC_CHUNK;
    if (running_tracks <= 0)
      tail += MUSIC_CHUNK;
  }

  I_OPL_StopSong();
  I_OPL_UnRegisterSong(song);
  if (out)
    mem_fclose(out);
  *frames = (int) done;
  return pcm;
}
