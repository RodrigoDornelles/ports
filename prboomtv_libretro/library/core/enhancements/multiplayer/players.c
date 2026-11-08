/**
 * @brief Up to 32 players: the most RetroArch's netplay takes (MAX_CLIENTS)
 * and demos and saves keep room for (MIN_MAXPLAYERS). A room's host picks
 * how many it takes under that (enhancements/multiplayer/session.c).
 * TODO upstream: a PR making doomdef.h's MAXPLAYERS overridable (#ifndef)
 * would take this patch away.
 *
 * @patch src/doomdef.h 116-116
 */
#define MAXPLAYERS       32
/* @endpatch */

/**
 * @brief The intermission's frags, one per player like player_t's: wi_stuff.c
 * reads every player's.
 *
 * @patch src/d_player.h 313-313
 */
  int         frags[MAXPLAYERS];
/* @endpatch */

/**
 * @brief A player who gets no start from the map (they have four) or finds
 * every one taken spawns beside one, as GZDoom does: on the first free spot
 * in rings of 64 units around a start, coop's first then deathmatch's, in
 * that start's sector so as not to land behind a wall.
 *
 * @patch src/g_game.c 1549
 */
#include "p_maputl.h"

/* P_SpawnPlayer reborns the player of the start's type, so a player spawns
 * at another's start through a copy of it with their own */
static void dopo_spawn_at(int playernum, const mapthing_t *spot)
{
  mapthing_t at = *spot;

  at.type = playernum + 1;
  if (!at.options)
    at.options = 1;
  P_SpawnPlayer(playernum, &at);
}

void dopo_spawn_beside(int playernum)
{
  const mapthing_t *coop[MAXPLAYERS], *spots[MAXPLAYERS * 2];
  mobj_t *mo;
  int n = 0, starts = 0, k, ring, dx, dy;

  /* coop starts first, from a different one for each player, so they
   * spread; deathmatch's only past them */
  for (k = 0; k < MAXPLAYERS; k++)
    if (G_PlayerStart(k)->type)
      coop[starts++] = G_PlayerStart(k);
  for (k = 0; k < starts; k++)
    spots[n++] = coop[(playernum + k) % starts];
  for (k = 0; k < deathmatch_p - deathmatchstarts && n < MAXPLAYERS * 2; k++)
    spots[n++] = &deathmatchstarts[k];
  if (!n)
    I_Error("dopo_spawn_beside: no start for player %d", playernum + 1);

  dopo_spawn_at(playernum, spots[0]);
  mo = players[playernum].mo;
  for (k = 0; k < n; k++)
  {
    const mapthing_t *spot = spots[k];
    const fixed_t x0 = spot->x * FRACUNIT, y0 = spot->y * FRACUNIT;
    const sector_t *sector = R_PointInSubsector(x0, y0)->sector;

    for (ring = 0; ring <= 4; ring++)
      for (dy = -ring; dy <= ring; dy++)
        for (dx = -ring; dx <= ring; dx++)
        {
          const fixed_t x = x0 + dx * 64 * FRACUNIT, y = y0 + dy * 64 * FRACUNIT;

          if ((abs(dx) != ring && abs(dy) != ring)
              || R_PointInSubsector(x, y)->sector != sector
              || !P_CheckPosition(mo, x, y))
            continue;
          P_UnsetThingPosition(mo);
          mo->x = mo->PrevX = x;
          mo->y = mo->PrevY = y;
          P_SetThingPosition(mo);
          mo->z = mo->PrevZ = mo->floorz = tmfloorz;
          mo->ceilingz = tmceilingz;
          players[playernum].viewz = mo->z + players[playernum].viewheight;
          return;
        }
  }
}

/* @endpatch */

/**
 * @brief The first spawn of a level skips players not in the game, as
 * slots may have gaps.
 *
 * @patch src/g_game.c 1419-1420
 */
        if (players[i].mo && players[i].mo->x == mthing->x * FRACUNIT
              && players[i].mo->y == mthing->y * FRACUNIT)
/* @endpatch */

/**
 * @brief Deathmatch takes any number of spots, not one per player.
 *
 * @patch src/g_game.c 1554-1556
 */
   if (!selections)
   {
      dopo_spawn_beside(playernum);
      return;
   }
/* @endpatch */

/** @patch src/g_game.c 1570-1570 */
   if (G_PlayerStart(playernum)->type)
      P_SpawnPlayer (playernum, G_PlayerStart(playernum));
   else
      dopo_spawn_beside(playernum);
/* @endpatch */

/**
 * @brief Coop respawn: starts the map lacks are skipped, a player at
 * another's start is reborn as themselves (upstream reborned the start's
 * owner), and a player without a start spawns beside one.
 *
 * @patch src/g_game.c 1603-1620
 */
   if (G_PlayerStart(playernum)->type && G_CheckSpot (playernum, G_PlayerStart(playernum)) )
   {
      P_SpawnPlayer (playernum, G_PlayerStart(playernum));
      return;
   }

   /* try to spawn at one of the other players spots */
   for (i=0 ; i<MAXPLAYERS ; i++)
   {
      if (G_PlayerStart(i)->type && G_CheckSpot (playernum, G_PlayerStart(i)) )
      {
         dopo_spawn_at (playernum, G_PlayerStart(i));
         return;
      }
   }

   if (!G_PlayerStart(playernum)->type)
   {
      dopo_spawn_beside(playernum);
      return;
   }

   /* he's going to be inside something.  Too bad. */
   P_SpawnPlayer (playernum, G_PlayerStart(playernum));
/* @endpatch */

/** @patch src/p_setup.c 79 */
void dopo_spawn_beside(int playernum);
/* @endpatch */

/**
 * @brief Coop players past the map's starts spawn beside one.
 *
 * @patch src/p_setup.c 3357-3359
 */
      for (i=0; i<MAXPLAYERS; i++)
         if (playeringame[i] && !players[i].mo)
            dopo_spawn_beside(i);
/* @endpatch */

/**
 * @brief Four player colours, as Doom has (green, indigo, brown, red): a
 * player past the fourth takes the colour, and the name, of their number
 * modulo four.
 *
 * @patch src/am_map.c 1424-1424
 */
          : mapcolor_plyr[i % 4], //jff 1/6/98 use default color
/* @endpatch */

/** @patch src/r_draw.c 6402-6402 */
   for (i=0; i<4; i++)
/* @endpatch */

/** @patch src/r_draw.c 6417 */
   for (i=4; i<MAXPLAYERS; i++)
      playernumtotrans[i] = playernumtotrans[i % 4];
/* @endpatch */

/** @patch src/g_game.c 1010-1010 */
  if (!demoplayback && mapcolor_plyr[consoleplayer % 4] != mapcolor_me) {
/* @endpatch */

/** @patch src/g_game.c 1081-1081 */
              doom_printf ("%s is turbo!", player_names[i % 4]);
/* @endpatch */

/** @patch src/g_game.c 1312-1312 */
  mapcolor_plyr[pn % 4] = cl;
/* @endpatch */

/** @patch src/hu_stuff.c 1434-1434 */
                                      player_names[i % 4],
/* @endpatch */

/** @patch src/st_stuff.c 453-454 */
                 displayplayer % 4 ? CR_LIMIT+displayplayer % 4 : CR_DEFAULT,
                 displayplayer % 4 ? (VPT_TRANS | VPT_STRETCH) : VPT_STRETCH);
/* @endpatch */

/**
 * @brief The netgame and deathmatch screens lay out players 1 to 4; with a
 * player past them, each machine shows its own player's stats, as in a game
 * of one (the scoreboard has everyone's).
 *
 * @patch src/wi_stuff.c 50
 */
static dbool WI_Crowded(void)
{
  int i;

  for (i = 4; i < MAXPLAYERS; i++)
    if (playeringame[i])
      return TRUE;
  return FALSE;
}
/* @endpatch */

/** @patch src/wi_stuff.c 2050-2051 */
         if (deathmatch && !WI_Crowded()) WI_updateDeathmatchStats();
         else if (netgame && !WI_Crowded()) WI_updateNetgameStats();
/* @endpatch */

/** @patch src/wi_stuff.c 2133-2135 */
         if (deathmatch && !WI_Crowded())
           WI_drawDeathmatchStats();
         else if (netgame && !WI_Crowded())
/* @endpatch */

/** @patch src/wi_stuff.c 2198-2200 */
  if (deathmatch && !WI_Crowded())
    WI_initDeathmatchStats();
  else if (netgame && !WI_Crowded())
/* @endpatch */

/**
 * @brief Heretic's netgame screens lay out players 1 to 4 and Hexen's
 * deathmatch tally 1 to 8: with a player past them, a game of one's screen.
 *
 * @patch src/heretic/in_lude.c 257-257
 */
  gametype = netgame ? (deathmatch ? IN_DEATHMATCH : IN_COOPERATIVE) : IN_SINGLE;
  {
    int i;

    for (i = 4; i < MAXPLAYERS; i++)
      if (playeringame[i])
        gametype = IN_SINGLE;
  }
/* @endpatch */

/** @patch src/hexen/in_lude.c 133-133 */
  int crowded = 0;

  for (playercount = 8; playercount < MAXPLAYERS; playercount++)
    crowded |= playeringame[playercount];
  if (!deathmatch || crowded)
/* @endpatch */

/**
 * @brief Saves keep the player record of four players (frags[4]), so the
 * saves made before load and the ones made now load on older builds; the
 * frags against players past the fourth, a netgame's only, are not kept.
 *
 * @patch src/p_saveg.c 65
 */
#include <stddef.h>

#define DOPO_PLAYER_KEPT (offsetof(player_t, frags) + 4 * sizeof(int))
#define DOPO_PLAYER_TAIL (offsetof(player_t, frags) + MAXPLAYERS * sizeof(int))
#define DOPO_PLAYER_RECORD (DOPO_PLAYER_KEPT + sizeof(player_t) - DOPO_PLAYER_TAIL)
/* @endpatch */

/** @patch src/p_saveg.c 91-92 */
        memcpy(save_p, &tmp, DOPO_PLAYER_KEPT);
        memcpy(save_p + DOPO_PLAYER_KEPT, (uint8_t *)&tmp + DOPO_PLAYER_TAIL,
               sizeof(player_t) - DOPO_PLAYER_TAIL);
        save_p += DOPO_PLAYER_RECORD;
/* @endpatch */

/** @patch src/p_saveg.c 110-111 */
        memset(&players[i], 0, sizeof(player_t));
        memcpy(&players[i], save_p, DOPO_PLAYER_KEPT);
        memcpy((uint8_t *)&players[i] + DOPO_PLAYER_TAIL, save_p + DOPO_PLAYER_KEPT,
               sizeof(player_t) - DOPO_PLAYER_TAIL);
        save_p += DOPO_PLAYER_RECORD;
/* @endpatch */
