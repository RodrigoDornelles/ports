/**
 * @brief Lockstep: the tic loop of d_client.c, where every machine runs
 * the same tics from the same ticcmds.
 *
 * In a netgame each machine builds the ticcmd of its own player once per
 * tic, up to DOPO_MP_LEAD tics ahead, and sends it; a tic only runs once
 * the ticcmds of every player in it are there, otherwise the frame shows
 * the same tic again. Tics are counted from the start of the netgame.
 */

/**
 * @brief Multiplayer declarations.
 *
 * @patch src/d_client.c 57
 */
#include <limits.h>
#include "dopo/multiplayer.h"
/* @endpatch */

/**
 * @brief Ticcmds of every player by tic, and the counters of the game.
 *
 * @patch src/d_client.c 66
 */
static ticcmd_t dopo_mp_cmds[MAXPLAYERS][BACKUPTICS];
static int      dopo_mp_cmd_tic[MAXPLAYERS][BACKUPTICS]; /* tic held, -1 none */
static int      dopo_mp_received[MAXPLAYERS];  /* tics 0..n-1 all held */
static int      dopo_mp_leave[MAXPLAYERS];     /* gone from this tic on */
static int      dopo_mp_base;                  /* gametic of tic 0 */
static int      dopo_mp_built;                 /* next tic to build */
static int      dopo_mp_run;                   /* next tic to run */
static dbool    dopo_mp_running;

void dopo_mp_lockstep_reset(void)
{
  int i, t;

  for (i = 0; i < MAXPLAYERS; i++)
  {
    for (t = 0; t < BACKUPTICS; t++)
      dopo_mp_cmd_tic[i][t] = -1;
    dopo_mp_received[i] = 0;
    dopo_mp_leave[i] = INT_MAX;
  }
  dopo_mp_built = dopo_mp_run = 0;
  dopo_mp_running = FALSE;
}

void dopo_mp_lockstep_begin(void)
{
  dopo_mp_base = maketic = gametic;
  localcmds = netcmds[consoleplayer];
  dopo_mp_running = TRUE;
}

void dopo_mp_lockstep_store(int slot, int tic, const ticcmd_t *cmd)
{
  if (slot < 0 || slot >= MAXPLAYERS ||
      tic < dopo_mp_run || tic >= dopo_mp_run + BACKUPTICS)
    return;

  dopo_mp_cmds[slot][tic % BACKUPTICS] = *cmd;
  dopo_mp_cmd_tic[slot][tic % BACKUPTICS] = tic;
  while (dopo_mp_cmd_tic[slot][dopo_mp_received[slot] % BACKUPTICS] ==
         dopo_mp_received[slot])
    dopo_mp_received[slot]++;
}

void dopo_mp_lockstep_leave(int slot, int tic)
{
  if (slot >= 0 && slot < MAXPLAYERS && tic < dopo_mp_leave[slot])
    dopo_mp_leave[slot] = tic;
}

int dopo_mp_lockstep_next_build(void)
{
  return dopo_mp_built;
}

int dopo_mp_lockstep_next_run(void)
{
  return dopo_mp_run;
}

int dopo_mp_lockstep_received(int slot)
{
  return dopo_mp_received[slot];
}

/**
 * @brief Whether every player still in the game has a ticcmd for a tic.
 */
static dbool dopo_mp_have_all(int tic)
{
  int i;

  for (i = 0; i < MAXPLAYERS; i++)
    if (playeringame[i] && tic < dopo_mp_leave[i] &&
        dopo_mp_cmd_tic[i][tic % BACKUPTICS] != tic)
      return FALSE;
  return TRUE;
}

/**
 * @brief One frame of a netgame: polls input, and when a tic is due
 * builds and sends this machine's ticcmd, then runs the next tic if every
 * ticcmd for it is there. Returns FALSE outside a netgame.
 */
static dbool dopo_mp_lockstep_frame(void)
{
  fixed_t overflow = 0;
  int i;

  if (!dopo_mp_running)
    return FALSE;
  if (dopo_mp_state() != DOPO_MP_GAME)
  {
    dopo_mp_running = FALSE;
    return FALSE;
  }

  I_StartTic();

  tic_vars.frac += tic_vars.frac_step;
  if (tic_vars.frac > FRACUNIT)
  {
    overflow = tic_vars.frac - FRACUNIT;
    tic_vars.frac = FRACUNIT;
  }
  if (tic_vars.frac < FRACUNIT)
    return TRUE;

  if (dopo_mp_built - dopo_mp_run < DOPO_MP_LEAD)
  {
    ticcmd_t *cmd = &localcmds[(dopo_mp_base + dopo_mp_built) % BACKUPTICS];

    maketic = dopo_mp_base + dopo_mp_built;
    G_BuildTiccmd(cmd);
    dopo_mp_lockstep_store(consoleplayer, dopo_mp_built, cmd);
    dopo_mp_send_tic(consoleplayer, dopo_mp_built, cmd);
    maketic = dopo_mp_base + ++dopo_mp_built;
  }

  /* waiting for someone: the frame shows this tic again */
  if (!dopo_mp_have_all(dopo_mp_run))
    return TRUE;
  tic_vars.frac = overflow;

  for (i = 0; i < MAXPLAYERS; i++)
  {
    if (!playeringame[i])
      continue;
    if (dopo_mp_run >= dopo_mp_leave[i])
    {
      playeringame[i] = FALSE;
      doom_printf("Player %d left the game", i + 1);
      continue;
    }
    netcmds[i][gametic % BACKUPTICS] =
      dopo_mp_cmds[i][dopo_mp_run % BACKUPTICS];
  }

  dopo_mp_tic_begin(dopo_mp_run);
  G_Ticker();
  if (menuactive)
    M_Ticker();
  gametic++;
  dopo_mp_run++;
  return TRUE;
}
/* @endpatch */

/**
 * @brief Starts or ends a netgame waiting to, then hands the frame to the
 * lockstep during one.
 *
 * @patch src/d_client.c 145
 */
  dopo_mp_frame();
  if (dopo_mp_lockstep_frame())
    return;

/* @endpatch */

/**
 * @brief Applies what was stamped for this tic right before it runs
 * (outside a netgame, only the cheats' upkeep).
 *
 * @patch src/d_client.c 159
 */
      dopo_mp_tic_begin(-1);
/* @endpatch */

/**
 * @brief Clears the consistency checks for a new netgame.
 *
 * @patch src/g_game.c 106
 */
void dopo_mp_consistancy_reset(void)
{
  memset(consistancy, 0, sizeof(consistancy));
}
/* @endpatch */
