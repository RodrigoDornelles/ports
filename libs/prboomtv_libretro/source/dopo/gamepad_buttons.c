/**
 * @brief Gamepad buttons: the single upstream function that turns every
 * button change into a key event, shared by the Action Button
 * (patchs/action_button.c) and Side Walk (patchs/side_walk.c) patches.
 */

/**
 * @brief Posts gamepad button changes as key events.
 *
 * In game, while dopo_action_button is on, DOPO_ACTION_BUTTON resolves to
 * use or fire whatever the layout binds it to (patchs/action_button.c),
 * and turn buttons may walk sideways (patchs/side_walk.c); in the menu
 * every button keeps the layout's menu key.
 *
 * @patch libretro/libretro.c 3399-3426
 */
static void process_gamepad_buttons(int16_t ret, unsigned num_buttons, action_lut_t action_lut[])
{
   unsigned i;
   bool new_input[MAX_BUTTON_BINDS];

   for (i = 0; i < num_buttons; i++)
   {
      event_t event = {0};
      new_input[i]  = ret & (1 << i);

      if(new_input[i] && !old_input[i])
      {
         event.type = ev_keydown;
         if (i == DOPO_ACTION_BUTTON && dopo_action_button && !menuactive)
         {
            dopo_action_latched = dopo_action_use_ahead() ? key_use : key_fire;
            event.data1 = dopo_action_latched;
         }
         else if (!menuactive)
            event.data1 = dopo_side_walk_press(action_lut[i].gamekey);
         else
            event.data1 = *action_lut[i].menukey;
      }

      if(!new_input[i] && old_input[i])
      {
         event.type = ev_keyup;
         if (i == DOPO_ACTION_BUTTON && dopo_action_latched != -1)
         {
            event.data1 = dopo_action_latched;
            dopo_action_latched = -1;
         }
         else if (!dopo_side_walk_release(action_lut[i].gamekey, &event.data1))
            event.data1 = *((menuactive)? action_lut[i].menukey : action_lut[i].gamekey);
      }

      if(event.type == ev_keydown || event.type == ev_keyup)
         D_PostEvent(&event);

      old_input[i] = new_input[i];
   }
}
/* @endpatch */
