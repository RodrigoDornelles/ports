/**
 * @brief Contextual action button shared by every gamepad layout.
 *
 * RETRO_DEVICE_ID_JOYPAD_A confirms in the menu and, in game, uses when
 * something usable is within reach or fires otherwise.
 */

/**
 * @brief Game headers needed to find a usable line ahead of the player.
 *
 * @patch libretro/libretro.c 64
 */
#include "../src/p_map.h"
#include "../src/p_maputl.h"
#include "../src/p_conversation.h"
#include "../src/map_format.h"
/* @endpatch */

/**
 * @brief Single place that picks the active gamepad layout.
 *
 * The action button is applied here to all layouts instead of being
 * repeated in each table.
 *
 * @patch libretro/libretro.c 1225
 */
#define DOPO_ACTION_BUTTON RETRO_DEVICE_ID_JOYPAD_A

/**
 * @brief Returns the layout for a gamepad device in the running game,
 * labelling DOPO_ACTION_BUTTON as the action button.
 */
static gamepad_layout_t *dopo_gamepad_layout(unsigned device)
{
	extern dbool heretic, hexen;
	gamepad_layout_t *gp;
	struct retro_input_descriptor *desc;

	if (device == RETROPAD_MODERN)
		gp = hexen ? &gp_hexen_modern : heretic ? &gp_heretic_modern : &gp_modern;
	else
		gp = hexen ? &gp_hexen_classic : heretic ? &gp_heretic_classic : &gp_classic;

	for (desc = gp->desc; desc->description; desc++)
		if (desc->device == RETRO_DEVICE_JOYPAD && desc->id == DOPO_ACTION_BUTTON)
			desc->description = "Fire / Use";

	return gp;
}

/* @endpatch */

/**
 * @brief Gamepad devices announce their descriptors through
 * dopo_gamepad_layout().
 *
 * @patch libretro/libretro.c 1225-1276
 */
void retro_set_controller_port_device(unsigned port, unsigned device)
{
	extern dbool heretic, hexen;

	if (port)
		return;

	switch (device)
	{
		case RETROPAD_CLASSIC:
		case RETROPAD_MODERN:
			doom_devices[port] = device;
			environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
			           dopo_gamepad_layout(device)->desc);
			break;
		case RETRO_DEVICE_KEYBOARD:
			doom_devices[port] = RETRO_DEVICE_KEYBOARD;
			if (hexen)
				environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
				           (void *)kbd_hexen_desc);
			else if (heretic)
				environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
				           (void *)kbd_heretic_desc);
			else
				environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
				           gp_classic.desc);
			break;
		default:
			if (log_cb)
				log_cb(RETRO_LOG_ERROR, "Invalid libretro controller device, using default: RETROPAD_CLASSIC\n");
			doom_devices[port] = RETROPAD_CLASSIC;
			environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
			           dopo_gamepad_layout(RETROPAD_CLASSIC)->desc);
	}
}
/* @endpatch */

/**
 * @brief Decides whether the action button uses or fires.
 *
 * The choice is made once on press and held until release, so walking
 * into a door while firing keeps firing.
 *
 * @patch libretro/libretro.c 3399
 */
static int dopo_action_latched = -1;
static dbool dopo_action_use_found;

/**
 * @brief P_PathTraverse callback that stops at the first use target,
 * like PTR_UseTraverse but without side effects.
 */
static dbool dopo_action_use_traverse(intercept_t *in)
{
   line_t *li = in->d.line;

   if (li->special &&
       (map_format.hexen ? (GET_SPAC(li->flags) == SPAC_USE) : TRUE))
   {
      dopo_action_use_found = TRUE;
      return FALSE;
   }

   P_LineOpeningAt(li, trace.x + FixedMul(trace.dx, in->frac),
                   trace.y + FixedMul(trace.dy, in->frac));
   return openrange > 0;
}

/**
 * @brief Whether pressing use now would reach something usable.
 */
static dbool dopo_action_use_ahead(void)
{
   mobj_t *mo = players[consoleplayer].mo;
   int angle;
   fixed_t x2, y2;

   if (gamestate != GS_LEVEL || !mo)
      return FALSE;

   if (P_ConversationIsActive())
      return TRUE;

   angle = mo->angle >> ANGLETOFINESHIFT;
   x2 = mo->x + (USERANGE>>FRACBITS)*finecosine[angle];
   y2 = mo->y + (USERANGE>>FRACBITS)*finesine[angle];

   dopo_action_use_found = FALSE;
   P_PathTraverse(mo->x, mo->y, x2, y2, PT_ADDLINES, dopo_action_use_traverse);
   return dopo_action_use_found;
}

/* @endpatch */

/**
 * @brief Posts gamepad button changes as key events.
 *
 * In game, DOPO_ACTION_BUTTON resolves to use or fire whatever the layout
 * binds it to; in the menu it keeps the layout's confirm key.
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
         if (i == DOPO_ACTION_BUTTON && !menuactive)
         {
            dopo_action_latched = dopo_action_use_ahead() ? key_use : key_fire;
            event.data1 = dopo_action_latched;
         }
         else
            event.data1 = *((menuactive)? action_lut[i].menukey : action_lut[i].gamekey);
      }

      if(!new_input[i] && old_input[i])
      {
         event.type = ev_keyup;
         if (i == DOPO_ACTION_BUTTON && dopo_action_latched != -1)
         {
            event.data1 = dopo_action_latched;
            dopo_action_latched = -1;
         }
         else
            event.data1 = *((menuactive)? action_lut[i].menukey : action_lut[i].gamekey);
      }

      if(event.type == ev_keydown || event.type == ev_keyup)
         D_PostEvent(&event);

      old_input[i] = new_input[i];
   }
}
/* @endpatch */
