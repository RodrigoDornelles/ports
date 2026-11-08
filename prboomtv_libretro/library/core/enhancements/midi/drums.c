/**
 * @brief The drums stay drums: fluidlite put MIDI channel 10 (9 counted
 * from zero) on the percussion bank (128) only once, when the synth was
 * made, and every reset (flplayer.c's fl_play resets at each song) put it
 * back on bank 0; the program change 0 every Doom song sends to its
 * percussion channel then picked the grand piano, which played the drum
 * notes. As fluidsynth 2 does in General MIDI mode, the channel starts and
 * resets on bank 128 and ignores bank select.
 */

/** @patch deps/fluidlite/src/fluid_chan.c 56-56 */
  chan->banknum = (chan->channum % 16 == 9) ? DRUM_INST_BANK : 0;
/* @endpatch */

/** @patch deps/fluidlite/src/fluid_chan.c 218-218 */
      if (chan->channum % 16 != 9)
        fluid_channel_set_banknum(chan, (unsigned int)(value & 0x7f));  /* KLE */
/* @endpatch */

/** @patch deps/fluidlite/src/fluid_chan.c 227-228 */
      if (chan->channum % 16 != 9)
	fluid_channel_set_banknum(chan, (((unsigned int) value & 0x7f)
					+ ((unsigned int) chan->bank_msb << 7)));
/* @endpatch */
