/**
 * @brief Ticcmd only input: everything a player does reaches the game
 * through their ticcmd, so every machine of a netgame (and every demo) runs
 * the same simulation from the same commands.
 *
 * Heretic and Hexen artifacts were used through a pending global read
 * only for the console player, so in a netgame the other players' uses
 * never happened and the local one happened on one machine only. They go
 * in cmd->arti now (as in vanilla Heretic and Hexen, whose demos already
 * carry it), next to the Hexen jump flag.
 */

/**
 * @brief Stages the ready artifact in the ticcmd instead of the pending
 * global.
 *
 * @patch src/g_game.c 460-464
 */
        /* used by P_PlayerThink for whoever owns this ticcmd; the low
         * bits hold the artifact, AFLAG_JUMP stays as it is */
        cmd->arti = (cmd->arti & ~AFLAG_MASK) |
                    (players[consoleplayer].readyArtifact & AFLAG_MASK);
/* @endpatch */

/**
 * @brief Rebuilding the ticcmd of a tic that has not run yet keeps the
 * artifact staged by an earlier build, as the key that staged it was
 * already consumed; the newer build wins when both staged one.
 *
 * @patch src/d_client.c 102
 */
	 if (!(cmd->arti & AFLAG_MASK))
	   cmd->arti |= prevcmd.arti & AFLAG_MASK;
/* @endpatch */

/**
 * @brief Uses the artifact in the ticcmd of any player.
 *
 * @patch src/p_user.c 1565-1575
 */
   /* Raven (Heretic/Hexen): use the artifact staged in this player's
    * ticcmd (G_BuildTiccmd), the same on every machine. */
   if (raven && (cmd->arti & AFLAG_MASK) &&
       (cmd->arti & AFLAG_MASK) < (hexen ? HEXEN_NUMARTIFACTS : NUMARTIFACTS))
      P_PlayerUseArtifact(player, cmd->arti & AFLAG_MASK);
/* @endpatch */
