/*
 * The Music tab's renderer: upstream's MIDI players (the OPL of opl.c, the
 * SoundFonts of fluid.c) built alone for the browser. The page starts a
 * synth, hands it a song, MUS or MIDI, and gets the whole song back as
 * float stereo, rendered once without looping.
 */
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include "memio.h"
#include "mus2mid.h"
#include "music.h"

/* after the last note, the release of the voices still sounding */
#define MUSIC_TAIL_SECONDS 2
/* a song that never ends (a track without its end) stops there */
#define MUSIC_MAX_SECONDS (20 * 60)
#define MUSIC_CHUNK 4096

enum { MUSIC_OPL, MUSIC_FLUID };

/* the song on synth (MUSIC_OPL, MUSIC_FLUID) as interleaved float stereo,
 * malloc'd, its frames in *frames; NULL when it is neither MUS nor MIDI the
 * player takes, or the synth was not started */
EMSCRIPTEN_KEEPALIVE float *music_render(int synth, const void *data, unsigned len, int samplerate, int *frames)
{
  const music_synth_t *s = synth == MUSIC_FLUID ? &music_fluid : &music_opl;
  const void *song;
  void *midi = (void *) data;
  size_t midi_len = len;
  MEMFILE *out = NULL;
  float *pcm = NULL;
  size_t done = 0, cap = 0, tail = 0;
  size_t max = (size_t) samplerate * MUSIC_MAX_SECONDS;

  *frames = 0;
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

  song = s->player->registersong(midi, midi_len);
  if (!song)
  {
    if (out)
      mem_fclose(out);
    return NULL;
  }
  s->player->play(song, 0);

  while (done < max && tail < (size_t) samplerate * MUSIC_TAIL_SECONDS)
  {
    if (done + MUSIC_CHUNK > cap)
    {
      size_t grow = cap ? cap * 2 : (size_t) samplerate * 60;
      float *grown = realloc(pcm, grow * 2 * sizeof(float));
      if (!grown)
        break;
      pcm = grown;
      cap = grow;
    }
    if (s->playing())
      s->player->render_float(pcm + done * 2, MUSIC_CHUNK);
    else
    {
      s->tail(pcm + done * 2, MUSIC_CHUNK);
      tail += MUSIC_CHUNK;
    }
    done += MUSIC_CHUNK;
  }

  s->player->stop();
  s->player->unregistersong(song);
  if (out)
    mem_fclose(out);
  *frames = (int) done;
  return pcm;
}
