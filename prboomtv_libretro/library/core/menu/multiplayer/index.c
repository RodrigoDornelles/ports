/**
 * @brief Multiplayer: the lobby of a netgame session, opened as soon as
 * the session starts, where the host sets the mode, skill and level up
 * and starts the game; during the game, the same menu changes them for
 * everyone (Change Level). A two column menu, with the players below.
 * Outside a session, rooms are created and joined here when the frontend
 * can (menu/multiplayer/rooms.c), and the lobby can be left (Leave).
 */

/**
 * @brief Lobby and game setup menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"
#include "prboomtv/rooms.h"

static void dopo_lobby_draw(void);
static void dopo_lobby_mode(int choice);
static void dopo_lobby_skill(int choice);
static void dopo_lobby_level(int choice);
static void dopo_lobby_start(int choice);
static void dopo_lobby_leave(int choice);

static menuitem_t dopo_lobby_items[] =
{
  {2, "", dopo_lobby_mode,  'm', "Mode"},
  {2, "", dopo_lobby_skill, 's', "Skill"},
  {2, "", dopo_lobby_level, 'l', "Level"},
  {1, "", dopo_lobby_start, 'g', "Start"},
  {1, "", dopo_lobby_leave, 'q', "Leave"},
};

static menu_t dopo_lobby_def =
{
  4,
  NULL,
  dopo_lobby_items,
  dopo_lobby_draw,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

/**
 * @brief Points the menu at the lobby or at the game's setup.
 */
static void dopo_lobby_setup(void)
{
  const dbool game = dopo_mp_state() == DOPO_MP_GAME;

  dopo_lobby_items[3].alttext = game ? "Change Level" : "Start";
  dopo_lobby_def.prevMenu = game ? M_MainMenuDef() : NULL;
  /* leaving is the frontend's to do: only when it hosts rooms for us */
  dopo_lobby_def.numitems = dopo_rooms_available() ? 5 : 4;
}

void dopo_lobby_open(void)
{
  if (!menuactive)
    M_StartControlPanel();
  dopo_lobby_setup();
  dopo_menu_open(&dopo_lobby_def);
}

void dopo_multiplayer_open(int choice)
{
  if (dopo_mp_state() == DOPO_MP_OFF)
  {
    if (dopo_rooms_available())
      dopo_rooms_menu_open();
    else
      M_StartMessage(DOPO_MP_NO_SESSION, NULL, FALSE);
    return;
  }
  dopo_lobby_setup();
  dopo_menu_open(&dopo_lobby_def);
}

/**
 * @brief Item routines change a copy of the setup and send it; only the
 * host may.
 */
static dbool dopo_lobby_edit(dopo_mp_config_t *config)
{
  if (!dopo_mp_is_host())
  {
    M_StartMessage(DOPO_MP_HOST_ONLY, NULL, FALSE);
    return FALSE;
  }
  *config = *dopo_mp_config();
  return TRUE;
}

static void dopo_lobby_mode(int choice)
{
  dopo_mp_config_t c;

  if (dopo_lobby_edit(&c))
  {
    int k;

    /* only the modes some map of the wads is for */
    for (k = 0; k < 3; k++)
    {
      c.mode = (c.mode + (choice ? 1 : 2)) % 3;
      if (dopo_mp_mode_offered(c.mode))
        break;
    }
    dopo_mp_levels_fit(&c);
    dopo_mp_set_config(&c);
  }
}

static void dopo_lobby_skill(int choice)
{
  dopo_mp_config_t c;

  if (dopo_lobby_edit(&c))
  {
    c.skill = (c.skill + (choice ? 1 : 4)) % 5;
    dopo_mp_set_config(&c);
  }
}

/**
 * @brief Next or previous map of the wads that takes the mode.
 */
static void dopo_lobby_level(int choice)
{
  dopo_mp_config_t c;

  if (!dopo_lobby_edit(&c))
    return;
  dopo_mp_levels_step(&c, choice ? 1 : -1);
  dopo_mp_set_config(&c);
}

static void dopo_lobby_leave(int choice)
{
  dopo_rooms_leave();
  M_ClearMenus();
}

static void dopo_lobby_start(int choice)
{
  if (!dopo_mp_is_host())
  {
    M_StartMessage(DOPO_MP_HOST_ONLY, NULL, FALSE);
    return;
  }
  dopo_mp_start_game();
}

/**
 * @brief Title, the setup's values and the players, in the small font
 * under the items.
 */
static void dopo_lobby_draw(void)
{
  static const char *const modes[] = { "COOP", "DEATHMATCH", "ALTDEATH" };
  static const char *const skills[] = { "BABY", "EASY", "NORMAL", "HARD", "NIGHTMARE" };
  const dbool game = dopo_mp_state() == DOPO_MP_GAME;
  const dopo_mp_config_t *c = dopo_mp_config();
  const dopo_mp_slot_t *slots = dopo_mp_slots();
  const int self = dopo_mp_self_slot();
  int y, i;
  char level[16];

  if (dopo_mp_state() == DOPO_MP_OFF)
  {
    M_ClearMenus();
    return;
  }

  if (hexen || gamemode == commercial)
    snprintf(level, sizeof(level), "MAP%02d", c->map);
  else
    snprintf(level, sizeof(level), "E%dM%d", c->episode, c->map);

  dopo_menu_big(&dopo_lobby_def);
  dopo_menu_title(game ? "MULTIPLAYER" : "LOBBY");
  dopo_menu_value(&dopo_lobby_def, 0, modes[c->mode % 3], CR_GOLD);
  dopo_menu_value(&dopo_lobby_def, 1, skills[c->skill % 5], CR_GOLD);
  dopo_menu_value(&dopo_lobby_def, 2, level, CR_GOLD);

  /* the players in, as many rows as fit above the footer */
  y = dopo_lobby_def.y + LINEHEIGHT * dopo_lobby_def.numitems + 6;
  {
    const int avail = (200 - 16 - y) / (DOPO_MENU_SMALL_LINE + 1) - 1;
    int in = 0, shown = 0, fit;
    char line[32];

    for (i = 0; i < MAXPLAYERS; i++)
      in += slots[i].used;
    fit = in > avail ? avail - 1 : in;
    snprintf(line, sizeof(line), "PLAYERS %d/%d", in, c->players);
    M_WriteText(DOPO_MENU_LEFT, y, line, CR_GOLD);
    y += DOPO_MENU_SMALL_LINE + 1;
    for (i = 0; i < MAXPLAYERS; i++)
    {
      if (!slots[i].used)
        continue;
      if (shown++ == fit)
      {
        snprintf(line, sizeof(line), "+%d MORE", in - fit);
        M_WriteText(DOPO_MENU_LEFT, y, line, CR_GRAY);
        y += DOPO_MENU_SMALL_LINE + 1;
        break;
      }
      snprintf(line, sizeof(line), "%d  %s%s", i + 1, slots[i].name,
               i == 0 ? "  (HOST)" : "");
      M_WriteText(DOPO_MENU_LEFT, y, line, i == self ? CR_GREEN : CR_DEFAULT);
      y += DOPO_MENU_SMALL_LINE + 1;
    }
  }

  if (!dopo_mp_is_host())
    dopo_menu_small_centered(y + 6, game ? "ONLY THE HOST CHANGES THE LEVEL"
                                         : "WAITING FOR THE HOST TO START", CR_GRAY);
}

/* @endpatch */
