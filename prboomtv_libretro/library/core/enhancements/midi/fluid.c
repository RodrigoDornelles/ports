/**
 * @brief The MIDI player on fluidlite (deps/fluidlite), the fluidsynth 1.x
 * without glib nor threads that upstream bundles: it renders the PCM
 * itself, so better instrument banks than the OPL need nothing from the
 * frontend. Upstream's flplayer.c was written against fluidsynth's own
 * headers and no longer built on fluidlite's.
 */

/**
 * @brief fluidlite's header is fluidlite.h, and the -1 its calls return on
 * failure is named only inside the library.
 *
 * @patch src/flplayer.c 73-73
 */
#include <fluidlite.h>
#define FLUID_FAILED (-1)
/* @endpatch */

/**
 * @brief writesysex: a sysex split across events reached the "not
 * understood" check before the synth had answered anything.
 *
 * @patch src/flplayer.c 375-375
 */
  int didrespond = 1;
/* @endpatch */

/** @patch src/flplayer.c 387-387 */
    fluid_synth_sysex (f_syn, (const char *)sysexbuff, sysexbufflen - 1, NULL, NULL, &didrespond, 0);
/* @endpatch */
