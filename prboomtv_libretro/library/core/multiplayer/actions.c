/**
 * @brief Admin actions that change the game: they run inside a game tic,
 * the same on every machine (multiplayer/session.c stamps them).
 */
#include "doomstat.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_mobj.h"
#include "r_demo.h"
#include "r_main.h"
#include "s_sound.h"
#include "tables.h"
#include "dopo/multiplayer.h"

/**
 * @brief Kills a player even in god mode or invulnerable: those are put
 * aside for the blow, since Boom's god mode stops even a telefrag
 * (comp_god off), and god mode comes back for the next life.
 */
void dopo_mp_kill(int slot)
{
  player_t *player;
  mobj_t *mo;
  int god;

  if (slot < 0 || slot >= MAXPLAYERS || !playeringame[slot])
    return;
  player = &players[slot];
  mo = player->mo;
  if (!mo || player->health <= 0)
    return;

  god = player->cheats & CF_GODMODE;
  player->cheats &= ~CF_GODMODE;
  player->powers[pw_invulnerability] = 0;
  mo->flags2 &= ~MF2_INVULNERABLE;
  P_DamageMobj(mo, NULL, NULL, 10000);
  player->cheats |= god;
}

/**
 * @brief Whether who fits at x,y: no wall, thing or low ceiling there.
 */
static dbool dopo_mp_fits(mobj_t *who, fixed_t x, fixed_t y)
{
  return P_CheckPosition(who, x, y) && tmceilingz - tmfloorz >= who->height;
}

/**
 * @brief Teleports a player next to another, as a teleporter does (fog,
 * sound, a short stop), facing them. The spot is the first free one
 * around the target, starting behind it, never on it, so nobody is
 * telefragged; with no free spot nothing happens.
 */
void dopo_mp_teleport(int slot, int target)
{
  mobj_t *who, *to;
  fixed_t distance;
  int i;

  if (slot < 0 || slot >= MAXPLAYERS || target < 0 || target >= MAXPLAYERS ||
      slot == target || !playeringame[slot] || !playeringame[target])
    return;
  who = players[slot].mo;
  to = players[target].mo;
  if (!who || !to || players[slot].health <= 0)
    return;

  distance = who->radius + to->radius + 4 * FRACUNIT;
  for (i = 0; i < 8; i++)
  {
    const angle_t a = to->angle + ANG180 + (angle_t)i * ANG45;
    const fixed_t x = to->x + FixedMul(distance, finecosine[a >> ANGLETOFINESHIFT]);
    const fixed_t y = to->y + FixedMul(distance, finesine[a >> ANGLETOFINESHIFT]);
    const fixed_t oldx = who->x, oldy = who->y, oldz = who->z;
    player_t *player = &players[slot];

    if (!dopo_mp_fits(who, x, y) || !P_TeleportMove(who, x, y, FALSE))
      continue;

    who->z = who->floorz;
    player->viewz = who->z + player->viewheight;
    S_StartSound(P_SpawnMobj(oldx, oldy, oldz, g_mt_tfog), g_sfx_telept);
    S_StartSound(P_SpawnMobj(x, y, who->z, g_mt_tfog), g_sfx_telept);

    who->reactiontime = 18;
    who->angle = R_PointToAngle2(x, y, to->x, to->y);
    who->momx = who->momy = who->momz = 0;
    player->momx = player->momy = 0;
    R_ResetAfterTeleport(player);
    return;
  }
}
