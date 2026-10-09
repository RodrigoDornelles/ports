/**
 * @brief Cheats toggled from Dopo Options > Cheats: infinite ammo,
 * infinite life and all weapons, kept up for every player at the start of
 * every tic a level is played (never during demos). In a netgame the host
 * sets them for everyone (multiplayer/settings.c).
 */
#include "doomstat.h"
#include "d_player.h"
#include "prboomtv/cheats.h"

/**
 * @brief Gives every weapon the game has, with the same rules as IDFA
 * (cheat_fa) for Doom and Heretic, and the class's four slots for Hexen.
 */
static void dopo_cheat_give_weapons(player_t *player)
{
   int i;

   if (hexen)
   {
      for (i = WP_FIRST; i <= (int)WP_FOURTH; i++)
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
 * @brief Applies the cheats that are on to every player in the game.
 * Infinite life is god mode (CF_GODMODE, as IDDQD), cleared once when
 * turned off.
 */
void dopo_cheats_tic(void)
{
   static dbool life_was_on;
   int i;

   if (gamestate != GS_LEVEL || demoplayback)
      return;

   for (i = 0; i < MAXPLAYERS; i++)
   {
      player_t *player = &players[i];

      if (!playeringame[i] || !player->mo)
         continue;

      if (dopo_cheats.life)
         player->cheats |= CF_GODMODE;
      else if (life_was_on)
         player->cheats &= ~CF_GODMODE;

      if (dopo_cheats.weapons)
         dopo_cheat_give_weapons(player);
      if (dopo_cheats.ammo)
         dopo_cheat_fill_ammo(player);
   }
   life_was_on = dopo_cheats.life;
}
