/**
 * @brief Auto Fist: firing with an enemy close ahead punches right away,
 * keeps punching while the enemy stays close and the button is held, then
 * brings the weapon in hand back up.
 *
 * There is no swap animation on the way in: the fist replaces the weapon
 * already in its punch. On the way out the weapon is raised as usual. The
 * enemy check uses the weapons' autoaim spread; punching starts within
 * DOPO_FIST_RANGE and only stops past DOPO_FIST_RELEASE, so an enemy at the
 * edge does not make the weapons flicker. The chainsaw is left alone, and
 * the swap always goes to the fist even when a chainsaw is owned. Doom and
 * Heretic only (Hexen's first slot is not always melee); off in demos and
 * netgames, where it would desync. Toggled from Dopo Options > Patchs.
 */

/**
 * @brief Auto Fist state and helpers.
 *
 * @patch src/p_pspr.c 546
 */
#define DOPO_FIST_RANGE   (MELEERANGE * 3 / 2)
#define DOPO_FIST_RELEASE (MELEERANGE * 2)

/**
 * @brief Whether Auto Fist is on; toggled from Dopo Options > Patchs and
 * not saved.
 */
#include "prboomtv/patchs.h"

dbool dopo_auto_fist = TRUE;

static dbool        dopo_fist_active[MAXPLAYERS]; /* fist put in hand by us */
static weapontype_t dopo_fist_back[MAXPLAYERS];   /* weapon to return to */

/**
 * @brief Whether Auto Fist may act for this player right now.
 */
static dbool dopo_fist_allowed(const player_t *player)
{
   return dopo_auto_fist && !hexen && !demoplayback && !netgame
       && !player->chickenTics && player->weaponowned[WP_FIST];
}

/**
 * @brief Whether an enemy is within range ahead, with the weapons' autoaim
 * spread (P_BulletSlope) and friends skipped as they do.
 */
static dbool dopo_fist_enemy_within(player_t *player, fixed_t range)
{
   const uint64_t mask = mbf_features ? MF_FRIEND : 0;
   const angle_t angle = player->mo->angle;
   mobj_t *saved = linetarget;
   dbool found;

   P_AimLineAttack(player->mo, angle, range, mask);
   if (!linetarget)
      P_AimLineAttack(player->mo, angle + (1<<26), range, mask);
   if (!linetarget)
      P_AimLineAttack(player->mo, angle - (1<<26), range, mask);

   found = linetarget != NULL;
   linetarget = saved;
   return found;
}

/**
 * @brief Swaps straight to the fist mid-punch when the player fires with
 * an enemy close ahead. Returns TRUE when it did.
 */
static dbool dopo_fist_start(player_t *player)
{
   const int i = player - players;

   if (!dopo_fist_allowed(player) ||
       player->readyweapon == WP_FIST || player->readyweapon == WP_CHAINSAW ||
       !dopo_fist_enemy_within(player, DOPO_FIST_RANGE))
      return FALSE;

   dopo_fist_back[i] = player->readyweapon;
   dopo_fist_active[i] = TRUE;

   player->readyweapon = WP_FIST;
   player->pendingweapon = WP_NOCHANGE;
   player->refire = 0;
   player->attackdown = TRUE;
   player->psprites[ps_weapon].sx = FRACUNIT;
   player->psprites[ps_weapon].sy = WEAPONTOP;
   P_FireWeapon(player);
   return TRUE;
}

/**
 * @brief Where the weapon would fire: starts the auto fist, or once its
 * enemy is gone (or the button released) brings the previous weapon back
 * up. Returns TRUE when it took over the weapon.
 */
static dbool dopo_fist_update(player_t *player, dbool attack)
{
   const int i = player - players;

   /* something else (a weapon key, no ammo) swapped the fist away */
   if (dopo_fist_active[i] && player->readyweapon != WP_FIST)
      dopo_fist_active[i] = FALSE;

   if (!dopo_fist_active[i])
      return attack && dopo_fist_start(player);

   if (attack && dopo_fist_enemy_within(player, DOPO_FIST_RELEASE))
      return FALSE;

   dopo_fist_active[i] = FALSE;
   if (!player->weaponowned[dopo_fist_back[i]])
      return FALSE;

   player->readyweapon = dopo_fist_back[i];
   player->pendingweapon = dopo_fist_back[i];
   P_BringUpWeapon(player);
   return TRUE;
}

/**
 * @brief Whether the auto fist should stop re-punching: its enemy left.
 */
static dbool dopo_fist_leaving(player_t *player)
{
   const int i = player - players;

   return dopo_fist_active[i] && player->readyweapon == WP_FIST &&
          !dopo_fist_enemy_within(player, DOPO_FIST_RELEASE);
}

/* @endpatch */

/**
 * @brief Weapon ready: Auto Fist may punch, or bring the weapon back, where
 * the weapon would otherwise fire.
 *
 * @patch src/p_pspr.c 546-612
 */
void A_WeaponReady(player_t *player, pspdef_t *psp)
{
   if (player->chickenTics)
   {                            /* change to the chicken beak */
      P_ActivateBeak(player);
      return;
   }

   /* get out of attack state */
   if (hexen)
   {
      if (player->mo->state >= &states[PStateAttack[player->class]]
            && player->mo->state <= &states[PStateAttackEnd[player->class]])
         P_SetMobjState(player->mo, PStateNormal[player->class]);
   }
   else if (player->mo->state == &states[g_s_play_atk1]
         || player->mo->state == &states[g_s_play_atk2] )
      P_SetMobjState(player->mo, g_s_play);

   if (player->readyweapon == WP_CHAINSAW && psp->state == &states[S_SAW])
      S_StartSound(player->mo, sfx_sawidl);

   // check for change
   //  if player is dead, put the weapon away

   if (player->pendingweapon != WP_NOCHANGE || !player->health)
   {
      // change weapon (pending weapon should already be validated)
      statenum_t newstate = hexen
         ? WeaponInfo[player->readyweapon][player->class].downstate
         : weaponinfo[player->readyweapon].downstate;
      P_SetPsprite(player, ps_weapon, newstate);
      return;
   }

   if (dopo_fist_update(player, (player->cmd.buttons & BT_ATTACK) != 0))
      return;

   // check for fire
   //  the missile launcher and bfg do not auto fire

   if (player->cmd.buttons & BT_ATTACK)
   {
      /* MBF21: a weapon flagged WPF_NOAUTOFIRE won't refire while the
       * button is held.  Below complevel 21, keep the hardcoded
       * missile-launcher / BFG behaviour exactly.  (Inert on hexen: its
       * weapon numbers never match and its weaponinfo flags are 0.) */
      dbool noautofire = mbf21_features
        ? (weaponinfo[player->readyweapon].flags & WPF_NOAUTOFIRE) != 0
        : (player->readyweapon == WP_MISSILE || player->readyweapon == WP_BFG);
      if (!player->attackdown || !noautofire)
      {
         player->attackdown = true;
         P_FireWeapon(player);
         return;
      }
   }
   else
      player->attackdown = false;

   /* Hexen: the pig snout doesn't bob. */
   if(!player->morphTics)
   {
      /* bob the weapon based on movement speed */
      int angle = (128*leveltime) & FINEMASK;
      psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
      angle &= FINEANGLES/2-1;
      psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
   }
}
/* @endpatch */

/**
 * @brief Re-fire: holding the trigger with an enemy close starts the auto
 * fist, and the auto fist stops punching once the enemy is gone.
 *
 * @patch src/p_pspr.c 620-639
 */
void A_ReFire(player_t *player, pspdef_t *psp)
{
  /* check for fire
   *  (if a weaponchange is pending, let it go through instead) */

  if ( (player->cmd.buttons & BT_ATTACK)
       && player->pendingweapon == WP_NOCHANGE && player->health
       && dopo_fist_start(player))
    return;

  if ( (player->cmd.buttons & BT_ATTACK)
       && player->pendingweapon == WP_NOCHANGE && player->health
       && !dopo_fist_leaving(player))
    {
      player->refire++;
      P_FireWeapon(player);
    }
  else
    {
      player->refire = 0;
      if (hexen)
         P_CheckMana(player);
      else
         P_CheckAmmo(player);
    }
}
/* @endpatch */
