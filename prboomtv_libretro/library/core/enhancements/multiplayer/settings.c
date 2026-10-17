/**
 * @brief What the host controls during a netgame, and the tic each change
 * applies from.
 *
 * Outside a netgame a change applies right away (in the lobby it goes
 * with the START packet). Inside one the host stamps it with the next tic
 * it builds a ticcmd for, which no machine runs before having the host's
 * ticcmd, so every machine applies it right before the same tic.
 */
#include "doomstat.h"
#include "prboomtv/cheats.h"
#include "prboomtv/multiplayer.h"

dopo_cheats_t dopo_cheats;

/**
 * @brief Cheats waiting for their tic.
 */
static struct
{
  dbool         set;
  int           tic;
  dopo_cheats_t cheats;
} dopo_pending_cheats;

const dopo_cheats_t *dopo_cheats_wanted(void)
{
  return dopo_pending_cheats.set ? &dopo_pending_cheats.cheats : &dopo_cheats;
}

void dopo_cheats_at(int tic, const dopo_cheats_t *cheats)
{
  if (tic < 0)
  {
    dopo_cheats = *cheats;
    dopo_pending_cheats.set = FALSE;
    return;
  }
  dopo_pending_cheats.cheats = *cheats;
  dopo_pending_cheats.tic = tic;
  dopo_pending_cheats.set = TRUE;
}

void dopo_cheats_request(const dopo_cheats_t *cheats)
{
  if (!dopo_mp_is_host())
    return;
  if (dopo_mp_state() != DOPO_MP_GAME)
  {
    dopo_cheats_at(-1, cheats);
    return;
  }
  dopo_cheats_at(dopo_mp_lockstep_next_build(), cheats);
  dopo_mp_send_cheats(dopo_pending_cheats.tic);
}

void dopo_mp_tic_begin(int tic)
{
  if (tic >= 0)
    dopo_mp_session_tic(tic);
  if (dopo_pending_cheats.set && tic >= dopo_pending_cheats.tic)
  {
    dopo_cheats = dopo_pending_cheats.cheats;
    dopo_pending_cheats.set = FALSE;
  }
  dopo_cheats_tic();
}
