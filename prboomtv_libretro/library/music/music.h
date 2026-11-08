#ifndef PRBOOMTV_MUSIC_H
#define PRBOOMTV_MUSIC_H

#include "musicplayer.h"

/* a synth of the Music tab: upstream's player, whether the song it plays
 * is still going, and the sound left after the end (the voices' release,
 * the reverb) */
typedef struct
{
  const music_player_t *player;
  int (*playing)(void);
  void (*tail)(void *dest, unsigned nsamp);
} music_synth_t;

extern const music_synth_t music_opl;
extern const music_synth_t music_fluid;

#endif
