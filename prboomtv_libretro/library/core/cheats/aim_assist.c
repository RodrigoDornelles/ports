/**
 * @brief Aim Assist and Trigger Assist cheats, toggled from Dopo Options >
 * Cheats and not saved.
 *
 * Each is OFF or an opening in degrees on each side of the crosshair, up
 * to 50 (45 covers the 4:3 screen).
 * Aim Assist: with a monster within its opening, the weapon actions of that
 * tic run facing it, so bullets, missiles and punches go its way; the view
 * itself does not turn. Trigger Assist: fires on its own while a monster is
 * within its opening.
 * Both skip friends and anything that is not a monster (barrels), and are
 * off in demos. In a netgame the host sets them for every player
 * (multiplayer/settings.c).
 */

/**
 * @brief Assist toggles and the target search.
 *
 * @patch src/p_pspr.c 2519
 */
#include "prboomtv/cheats.h"

static dbool  dopo_aim_assisted;
static angle_t dopo_aim_angle;

/**
 * @brief Whether the assists may act for this player now.
 */
static dbool dopo_assist_allowed(const player_t *player)
{
   return !demoplayback && player->mo && player->health > 0;
}

/**
 * @brief Monster closest to the crosshair within degrees on each side, in
 * range and in sight, or NULL. Walks MBF's list of hostile monsters, so
 * friends never count, and checks sight only for the best candidates.
 */
static mobj_t *dopo_assist_target(player_t *player, int degrees)
{
   const mobj_t *self = player->mo;
   thinker_t *cap = &thinkerclasscap[th_enemies];
   thinker_t *th;
   mobj_t *best = NULL;
   angle_t best_off = (angle_t)degrees * ANG1 + 1;

   for (th = cap->cnext; th != cap; th = th->cnext)
   {
      mobj_t *mo = (mobj_t *)th;
      angle_t off;

      if (mo->health <= 0 || !(mo->flags & MF_SHOOTABLE) ||
          !(mo->flags & (MF_COUNTKILL | MF_ISMONSTER)) ||
          P_AproxDistance(mo->x - self->x, mo->y - self->y) > MISSILERANGE)
         continue;

      off = R_PointToAngle2(self->x, self->y, mo->x, mo->y) - self->angle;
      if (off > ANG180)
         off = 0 - off;
      if (off < best_off && P_CheckSight(player->mo, mo))
      {
         best = mo;
         best_off = off;
      }
   }
   return best;
}

/**
 * @brief Trigger Assist: holds fire this tic while a monster is in the
 * crosshair.
 */
static void dopo_trigger_assist_update(player_t *player)
{
   if (!dopo_cheats.trigger_assist || !dopo_assist_allowed(player) ||
       player->pendingweapon != WP_NOCHANGE)
      return;

   if (dopo_assist_target(player, dopo_cheats.trigger_assist))
      player->cmd.buttons |= BT_ATTACK;
}

/**
 * @brief Aim Assist: while the weapon is firing, turns the player toward a
 * monster near the crosshair for this tic's weapon actions. Returns the
 * angle to restore afterwards.
 */
static angle_t dopo_aim_assist_begin(player_t *player)
{
   const angle_t angle = player->mo ? player->mo->angle : 0;
   mobj_t *target;

   dopo_aim_assisted = FALSE;
   if (!dopo_cheats.aim_assist || !dopo_assist_allowed(player) ||
       !((player->cmd.buttons & BT_ATTACK) || player->attackdown))
      return angle;

   target = dopo_assist_target(player, dopo_cheats.aim_assist);
   if (!target)
      return angle;

   dopo_aim_angle = R_PointToAngle2(player->mo->x, player->mo->y, target->x, target->y);
   dopo_aim_assisted = TRUE;
   player->mo->angle = dopo_aim_angle;
   return angle;
}

/**
 * @brief Gives the player's view angle back, unless a weapon action turned
 * the player on its own (the chainsaw pulls toward what it saws).
 */
static void dopo_aim_assist_end(player_t *player, angle_t angle)
{
   if (dopo_aim_assisted && player->mo->angle == dopo_aim_angle)
      player->mo->angle = angle;
   dopo_aim_assisted = FALSE;
}

/* @endpatch */
