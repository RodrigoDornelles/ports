/**
 * @brief A player's options, for admins: promote to (or remove) admin,
 * kick, kill, under the player's name.
 */

/**
 * @brief Player options menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

static int dopo_player_slot;

static void dopo_player_draw(void);
static void dopo_player_admin(int choice);
static void dopo_player_kick(int choice);
static void dopo_player_kill(int choice);

static menuitem_t dopo_player_items[] =
{
  {1, "", dopo_player_admin, 'a', "Promote to Admin"},
  {1, "", dopo_player_kick,  'k', "Kick Player"},
  {1, "", dopo_player_kill,  'x', "Kill Player"},
};

static menu_t dopo_player_def =
{
  3,
  &dopo_scoreboard_def,
  dopo_player_items,
  dopo_player_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

void dopo_player_open(int slot)
{
  dopo_player_slot = slot;
  dopo_player_items[0].alttext =
    dopo_mp_slots()[slot].admin ? "Remove Admin" : "Promote to Admin";
  dopo_menu_open(&dopo_player_def);
}

static void dopo_player_draw(void)
{
  const dopo_mp_slot_t *slot = &dopo_mp_slots()[dopo_player_slot];

  /* the player left: back to the list */
  if (dopo_mp_state() != DOPO_MP_GAME || !slot->used)
  {
    M_SetupNextMenu(&dopo_scoreboard_def);
    return;
  }
  dopo_menu_small(&dopo_player_def);
  dopo_menu_title(slot->name);
}

/**
 * @brief The host can't lose admin nor be kicked; anyone can be killed.
 */
static dbool dopo_player_not_host(void)
{
  if (dopo_player_slot != 0)
    return TRUE;
  M_StartMessage("The host can't be changed.\n\nPress a key.", NULL, FALSE);
  return FALSE;
}

static void dopo_player_admin(int choice)
{
  if (!dopo_player_not_host())
    return;
  dopo_mp_admin(DOPO_MP_ADMIN_TOGGLE, dopo_player_slot);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

static void dopo_player_kick(int choice)
{
  if (!dopo_player_not_host())
    return;
  dopo_mp_admin(DOPO_MP_ADMIN_KICK, dopo_player_slot);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

static void dopo_player_kill(int choice)
{
  dopo_mp_admin(DOPO_MP_ADMIN_KILL, dopo_player_slot);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

/* @endpatch */
