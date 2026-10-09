/**
 * @brief Scoreboard: every player of the netgame with kills, deaths and
 * ping, in the small font so a row stays short as the player count grows.
 * An admin picks a player for its options (menu/scoreboard/player.c).
 *
 * K is frags in deathmatch (as the intermission sums them) and monster
 * kills in coop; D is deaths of any kind, counted in the game tics.
 */

/**
 * @brief Scoreboard menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

/* on the menus' anchor: names from DOPO_MENU_LEFT, K/D
 * and ping right aligned, ping on DOPO_MENU_RIGHT */
#define DOPO_SCORE_X       DOPO_MENU_LEFT
#define DOPO_SCORE_KD_R    (DOPO_MENU_RIGHT - 60)
#define DOPO_SCORE_PING_R  DOPO_MENU_RIGHT
#define DOPO_SCORE_HEAD_Y  (DOPO_MENU_TOP - 6)
#define DOPO_SCORE_ROW_Y   (DOPO_MENU_TOP + 6)
#define DOPO_SCORE_ROW     (DOPO_MENU_SMALL_LINE + 1)

/**
 * @brief Writes text in the small font ending at x.
 */
static void dopo_score_right(int x, int y, const char *text, int cm)
{
  M_WriteText(x - M_StringWidth(text), y, text, cm);
}

static void dopo_score_draw(void);
static void dopo_score_pick(int choice);

static menuitem_t dopo_score_items[MAXPLAYERS];
static int        dopo_score_slots[MAXPLAYERS];  /* row -> slot */

menu_t dopo_scoreboard_def =
{
  0,
  NULL,
  dopo_score_items,
  dopo_score_draw,
  DOPO_SCORE_X, DOPO_SCORE_ROW_Y,
  0
};

dbool dopo_menu_draws_cursor(const menu_t *menu)
{
  return menu == &dopo_scoreboard_def || menu == &dopo_list_def;
}

/**
 * @brief One row per player in the game, in slot order; the cursor stays
 * on a row that still exists.
 */
static void dopo_score_refresh(void)
{
  const dopo_mp_slot_t *slots = dopo_mp_slots();
  int i, rows = 0;

  for (i = 0; i < MAXPLAYERS; i++)
    if (slots[i].used)
    {
      dopo_score_items[rows] = (menuitem_t){1, "", dopo_score_pick, 0, NULL};
      dopo_score_slots[rows++] = i;
    }
  dopo_scoreboard_def.numitems = rows;
  if (currentMenu == &dopo_scoreboard_def && itemOn >= rows)
    itemOn = rows ? rows - 1 : 0;
}

void dopo_scoreboard_open(int choice)
{
  dopo_scoreboard_def.prevMenu = M_MainMenuDef();
  dopo_score_refresh();
  dopo_menu_open(&dopo_scoreboard_def);
}

/**
 * @brief Frags in deathmatch (others killed, minus suicides), monsters
 * killed in coop.
 */
static int dopo_score_kills(int slot)
{
  int i, kills = 0;

  if (!deathmatch)
    return players[slot].killcount;
  for (i = 0; i < MAXPLAYERS; i++)
    kills += i == slot ? -players[slot].frags[i] : players[slot].frags[i];
  return kills;
}

static void dopo_score_draw(void)
{
  const dopo_mp_slot_t *slots = dopo_mp_slots();
  const int self = dopo_mp_self_slot();
  int row;

  if (dopo_mp_state() != DOPO_MP_GAME)
  {
    M_ClearMenus();
    return;
  }
  dopo_score_refresh();

  dopo_menu_title("SCOREBOARD");
  M_WriteText(DOPO_SCORE_X, DOPO_SCORE_HEAD_Y, "PLAYER", CR_GOLD);
  dopo_score_right(DOPO_SCORE_KD_R, DOPO_SCORE_HEAD_Y, "K/D", CR_GOLD);
  dopo_score_right(DOPO_SCORE_PING_R, DOPO_SCORE_HEAD_Y, "PING", CR_GOLD);

  for (row = 0; row < dopo_scoreboard_def.numitems; row++)
  {
    const int slot = dopo_score_slots[row];
    const int y = DOPO_SCORE_ROW_Y + row * DOPO_SCORE_ROW;
    const int cm = row == itemOn ? CR_GOLD : slot == self ? CR_GREEN : CR_DEFAULT;
    char name[DOPO_MP_NAME + 16], kd[16], ping[16];

    snprintf(name, sizeof(name), "%s%s", slots[slot].name,
             slot == 0 ? " (HOST)" : slots[slot].admin ? " (ADMIN)" : "");
    snprintf(kd, sizeof(kd), "%d/%d", dopo_score_kills(slot), dopo_mp_deaths(slot));
    if (slots[slot].ping < 0)
      snprintf(ping, sizeof(ping), "-");
    else
      snprintf(ping, sizeof(ping), "%dMS", slots[slot].ping);

    if (row == itemOn)
      M_WriteText(DOPO_SCORE_X - 10, y, ">", CR_GOLD);
    M_WriteText(DOPO_SCORE_X, y, name, cm);
    dopo_score_right(DOPO_SCORE_KD_R, y, kd, cm);
    dopo_score_right(DOPO_SCORE_PING_R, y, ping, cm);
  }
}

/**
 * @brief Item routine: an admin opens the player's options.
 */
static void dopo_score_pick(int choice)
{
  if (!dopo_mp_is_admin())
  {
    M_StartMessage(DOPO_MP_ADMIN_ONLY, NULL, FALSE);
    return;
  }
  dopo_player_open(dopo_score_slots[choice]);
}

/* @endpatch */
