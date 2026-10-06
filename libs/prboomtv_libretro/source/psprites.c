/**
 * @brief Weapon sprites: the single upstream function that ticks the
 * player's weapon states, shared by Toggle Fire (toggle_fire.c), Trigger
 * Assist and Aim Assist (aim_assist.c).
 */

/**
 * @brief Moves the psprites. Toggle Fire and Trigger Assist first settle
 * this tic's fire button, then the weapon actions run with Aim Assist's
 * angle, so every one of them sees both.
 *
 * @patch src/p_pspr.c 2519-2577
 */
void P_MovePsprites(player_t *player)
{
  int i;
  angle_t aimed;
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

  /* this tic's fire button: the player's taps first, then the assist */
  dopo_toggle_fire_update(player);
  dopo_trigger_assist_update(player);

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

  aimed = dopo_aim_assist_begin(player);
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
  dopo_aim_assist_end(player, aimed);

  player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
  player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
/* @endpatch */
