/**
 * @brief Toggle Fire: on heavy weapons a tap starts firing nonstop and the
 * next tap stops it; running out of ammo stops it too, so the usual
 * automatic weapon switch happens.
 *
 * A weapon is heavy when its attack reaches A_ReFire within
 * DOPO_TOGGLE_FIRE_TICS, read from the game's own state tables: in Doom
 * the chaingun (8), chainsaw (8) and plasma rifle (3), but not the pistol
 * (14) or fist (17), and DEHACKED changes are followed. Switching weapons
 * or dying also stops it. Doom and Heretic only; off in demos and
 * netgames, where it would desync. Toggled from Dopo Options > Patchs.
 */

/**
 * @brief Toggle Fire state and helpers.
 *
 * @patch src/p_pspr.c 2519
 */
#define DOPO_TOGGLE_FIRE_TICS 8

/**
 * @brief Whether Toggle Fire is on; toggled from Dopo Options > Patchs and
 * not saved.
 */
dbool dopo_toggle_fire = TRUE;

static dbool        dopo_fire_held[MAXPLAYERS];     /* button down last tic */
static dbool        dopo_fire_latched[MAXPLAYERS];  /* firing nonstop */
static dbool        dopo_fire_suppress[MAXPLAYERS]; /* the stopping tap */
static weapontype_t dopo_fire_weapon[MAXPLAYERS];   /* weapon latched on */

/**
 * @brief Whether a weapon is heavy: its attack, in the current power level,
 * reaches A_ReFire within DOPO_TOGGLE_FIRE_TICS.
 */
static dbool dopo_fire_heavy(player_t *player, weapontype_t weapon)
{
   statenum_t s = P_WeaponLevelInfo(player)[weapon].atkstate;
   long tics = 0;
   int steps;

   for (steps = 0; steps < 32 && s != S_NULL; steps++)
   {
      const state_t *state = &states[s];

      if (state->action.arg0 == (arg0_t)A_ReFire)
         return tics <= DOPO_TOGGLE_FIRE_TICS;
      if (state->tics < 0)
         return FALSE;
      tics += state->tics;
      s = state->nextstate;
   }
   return FALSE;
}

/**
 * @brief Turns this tic's fire button into the toggled one: taps on a heavy
 * weapon latch or unlatch it, and the latch holds BT_ATTACK down.
 */
static void dopo_toggle_fire_update(player_t *player)
{
   const int i = player - players;
   const dbool held = (player->cmd.buttons & BT_ATTACK) != 0;
   const dbool tapped = held && !dopo_fire_held[i];

   dopo_fire_held[i] = held;

   if (!dopo_toggle_fire || hexen || demoplayback || netgame)
   {
      dopo_fire_latched[i] = FALSE;
      dopo_fire_suppress[i] = FALSE;
      return;
   }

   /* weapon change, no ammo left (the usual switch follows) or death */
   if (dopo_fire_latched[i] &&
       (player->readyweapon != dopo_fire_weapon[i] ||
        player->pendingweapon != WP_NOCHANGE ||
        P_GetAmmoLevel(player, player->readyweapon) <= 0 ||
        !player->health))
      dopo_fire_latched[i] = FALSE;

   if (tapped && dopo_fire_heavy(player, player->readyweapon))
   {
      if (dopo_fire_latched[i])
      {
         dopo_fire_latched[i] = FALSE;
         dopo_fire_suppress[i] = TRUE;
      }
      else if (player->pendingweapon == WP_NOCHANGE)
      {
         dopo_fire_latched[i] = TRUE;
         dopo_fire_weapon[i] = player->readyweapon;
      }
   }

   if (!held)
      dopo_fire_suppress[i] = FALSE;

   if (dopo_fire_latched[i])
      player->cmd.buttons |= BT_ATTACK;
   else if (dopo_fire_suppress[i])
      player->cmd.buttons &= ~BT_ATTACK;
}

/* @endpatch */
