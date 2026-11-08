/**
 * @brief A dead player respawns with fire too, not only use, as in ZDoom:
 * use is the door key, which nobody guesses after dying. Fire must be
 * pressed again, not held from before the death; demos keep use only, so
 * they play back as recorded.
 *
 * @patch src/p_user.c 563-563
 */
   if (player->cmd.buttons & BT_USE ||
       (!demoplayback && player->cmd.buttons & BT_ATTACK && !player->attackdown))
/* @endpatch */

/** @patch src/p_user.c 571 */
   if (!demoplayback)
      player->attackdown = (player->cmd.buttons & BT_ATTACK) != 0;
/* @endpatch */
