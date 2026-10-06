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

/**
 * @brief Moves the psprites, after Toggle Fire settles this tic's fire
 * button, so every weapon action sees it.
 *
 * @patch src/p_pspr.c 2519-2577
 */
void P_MovePsprites(player_t *player)
{
  int i;
  pspdef_t *psp  = player->psprites;
  psp_inter_t *old = psp_oldpos[player - players];

  /* Record where this tic starts from, so the renderer can interpolate
   * between the previous and current positions.  The state pointer lets
   * it tell continuous motion (bob, raise, lower) from the discontinuous
   * reposition a weapon change makes. */
  for (i = 0; i < NUMPSPRITES; i++)
  {
    old[i].sx    = psp[i].sx;
    old[i].sy    = psp[i].sy;
    old[i].state = psp[i].state;
  }

  dopo_toggle_fire_update(player);

  /* While a dialogue overlay has the player frozen, the weapon is put away:
   * slide it down off the screen and hold it there, leaving the state machine
   * untouched so it springs back up the moment the freeze lifts.  The flash
   * sprite tracks the weapon, as it does normally. */
  if ((player->cheats & CF_TOTALLYFROZEN) ||
      (P_ConversationIsActive() && player == &players[consoleplayer]))
  {
    pspdef_t *w = &player->psprites[ps_weapon];
    if (w->sy < WEAPONBOTTOM)
    {
      w->sy += LOWERSPEED;
      if (w->sy > WEAPONBOTTOM)
        w->sy = WEAPONBOTTOM;
    }
    player->psprites[ps_flash].sx = w->sx;
    player->psprites[ps_flash].sy = w->sy;
    return;
  }

  /* a null state means not active
   * drop tic count and possibly change state
   * a -1 tic count never changes */

  for (i=0; i<NUMPSPRITES; i++, psp++)
  {
     state_t *state = psp->state;

     if(state == 0) /* a null state means not active */
        continue;

     /* drop tic count and possibly change state */
     if (psp->tics != -1)
     {
        --psp->tics;
        if (!psp->tics)
           P_SetPsprite(player, i, psp->state->nextstate);
     }
  }

  player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
  player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
/* @endpatch */
