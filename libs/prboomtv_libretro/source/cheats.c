/**
 * @brief Cheats toggled from Dopo Options > Cheats: infinite ammo,
 * infinite life and all weapons, kept up every tic while a level is
 * played (never during demos or netgames, which would desync).
 */

/**
 * @brief Cheat toggles, read by the Cheats menu, and their per-tic upkeep.
 *
 * @patch libretro/libretro.c 3742
 */
dbool dopo_cheat_ammo;
dbool dopo_cheat_life;
dbool dopo_cheat_weapons;

/**
 * @brief Gives every weapon the game has, with the same rules as IDFA
 * (cheat_fa) for Doom and Heretic, and the class's four slots for Hexen.
 */
static void dopo_cheat_give_weapons(player_t *player)
{
   int i;

   if (hexen)
   {
      for (i = WP_FIRST; i <= WP_FOURTH; i++)
         player->weaponowned[i] = TRUE;
      return;
   }

   for (i = 0; i < NUMWEAPONS; i++)
      if (!(((i == WP_PLASMA || i == WP_BFG) && gamemode == shareware) ||
            (i == WP_SUPERSHOTGUN && gamemode != commercial)))
         player->weaponowned[i] = TRUE;
}

/**
 * @brief Keeps every ammo type (Hexen: both manas) at its maximum.
 */
static void dopo_cheat_fill_ammo(player_t *player)
{
   int i;

   if (hexen)
   {
      player->mana[MANA_1] = player->maxmana;
      player->mana[MANA_2] = player->maxmana;
      return;
   }

   for (i = 0; i < NUMAMMO; i++)
      player->ammo[i] = player->maxammo[i];
}

/**
 * @brief Applies the cheats that are on to the console player. Infinite
 * life is god mode (CF_GODMODE, as IDDQD), cleared once when turned off.
 */
static void dopo_cheats_apply(void)
{
   static dbool life_was_on;
   player_t *player = &players[consoleplayer];

   if (gamestate != GS_LEVEL || demoplayback || netgame || !player->mo)
      return;

   if (dopo_cheat_life)
      player->cheats |= CF_GODMODE;
   else if (life_was_on)
      player->cheats &= ~CF_GODMODE;
   life_was_on = dopo_cheat_life;

   if (dopo_cheat_weapons)
      dopo_cheat_give_weapons(player);
   if (dopo_cheat_ammo)
      dopo_cheat_fill_ammo(player);
}

/* @endpatch */

/**
 * @brief Polls input, then keeps the cheats up, once per tic.
 *
 * @patch libretro/libretro.c 3742-3748
 */
void I_StartTic(void)
{
   if (!input_poll_cb)
      return;
   input_poll_cb();
   process_input();
   dopo_cheats_apply();
}
/* @endpatch */
