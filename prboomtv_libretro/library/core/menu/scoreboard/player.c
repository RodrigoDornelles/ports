/**
 * @brief A player's options, for admins: promote to (or remove) admin,
 * kick, kill, and teleport them to another player, chosen in a second
 * menu. Small menus, under the player's name.
 */

/**
 * @brief Player options and the Teleport To choice.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

static int dopo_player_slot;

static void dopo_player_draw(void);
static void dopo_player_admin(int choice);
static void dopo_player_kick(int choice);
static void dopo_player_kill(int choice);
static void dopo_player_teleport(int choice);

static menuitem_t dopo_player_items[] =
{
  {1, "", dopo_player_admin,    'a', "Promote to Admin"},
  {1, "", dopo_player_kick,     'k', "Kick Player"},
  {1, "", dopo_player_kill,     'x', "Kill Player"},
  {1, "", dopo_player_teleport, 't', "Teleport To"},
};

static menu_t dopo_player_def =
{
  4,
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

/**
 * @brief Whether the player is still in the game; if not, back to the
 * scoreboard.
 */
static dbool dopo_player_present(void)
{
  if (dopo_mp_state() == DOPO_MP_GAME && dopo_mp_slots()[dopo_player_slot].used)
    return TRUE;
  M_SetupNextMenu(&dopo_scoreboard_def);
  return FALSE;
}

static void dopo_player_draw(void)
{
  if (!dopo_player_present())
    return;
  dopo_menu_small(&dopo_player_def);
  dopo_menu_title(dopo_mp_slots()[dopo_player_slot].name);
}

/**
 * @brief The host can't lose admin nor be kicked; anyone can be killed or
 * teleported.
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
  dopo_mp_admin(DOPO_MP_ADMIN_TOGGLE, dopo_player_slot, -1);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

static void dopo_player_kick(int choice)
{
  if (!dopo_player_not_host())
    return;
  dopo_mp_admin(DOPO_MP_ADMIN_KICK, dopo_player_slot, -1);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

static void dopo_player_kill(int choice)
{
  dopo_mp_admin(DOPO_MP_ADMIN_KILL, dopo_player_slot, -1);
  M_SetupNextMenu(&dopo_scoreboard_def);
}

/* ------------------------------------------------------------------ */
/* Teleport To: the other players, the one picked is the destination   */
/* ------------------------------------------------------------------ */

static void dopo_teleport_draw(void);
static void dopo_teleport_pick(int choice);

static menuitem_t dopo_teleport_items[MAXPLAYERS];
static int        dopo_teleport_slots[MAXPLAYERS];  /* row -> slot */

static menu_t dopo_teleport_def =
{
  0,
  &dopo_player_def,
  dopo_teleport_items,
  dopo_teleport_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

/**
 * @brief One row per other player in the game, named after them.
 */
static void dopo_teleport_fill(void)
{
  const dopo_mp_slot_t *slots = dopo_mp_slots();
  int i, rows = 0;

  for (i = 0; i < MAXPLAYERS; i++)
    if (slots[i].used && i != dopo_player_slot)
    {
      dopo_teleport_items[rows] = (menuitem_t){1, "", dopo_teleport_pick, 0, slots[i].name};
      dopo_teleport_slots[rows++] = i;
    }
  dopo_teleport_def.numitems = rows;
  if (currentMenu == &dopo_teleport_def && itemOn >= rows)
    itemOn = rows ? rows - 1 : 0;
}

static void dopo_player_teleport(int choice)
{
  dopo_teleport_fill();
  if (!dopo_teleport_def.numitems)
  {
    M_StartMessage("Nobody else to go to.\n\nPress a key.", NULL, FALSE);
    return;
  }
  dopo_menu_open(&dopo_teleport_def);
}

static void dopo_teleport_draw(void)
{
  if (!dopo_player_present())
    return;
  dopo_teleport_fill();
  dopo_menu_small(&dopo_teleport_def);
  dopo_menu_title("TELEPORT TO");
}

/**
 * @brief Sends the player next to the one picked, and back to the game.
 */
static void dopo_teleport_pick(int choice)
{
  dopo_mp_admin(DOPO_MP_ADMIN_TELEPORT, dopo_player_slot, dopo_teleport_slots[choice]);
  M_ClearMenus();
}

/* @endpatch */
