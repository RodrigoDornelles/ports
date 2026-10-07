/**
 * @brief Change Game: the games next to the running content, in pages of
 * DOPO_GAMES_PAGE, and a Change/Cancel confirmation; the switch itself
 * happens in libretro.c (dopo/change_game.c) at the next frame.
 *
 * The running game is marked CURRENT on the right of its row.
 */

/**
 * @brief Change Game menus.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

#define DOPO_GAMES_PAGE  8
#define DOPO_GAME_LABEL  17

static int  dopo_games_total;
static int  dopo_games_first;
static int  dopo_game_chosen;
static char dopo_game_labels[DOPO_GAMES_PAGE][DOPO_GAME_LABEL];
static char dopo_games_page_text[24];

static menuitem_t dopo_games_items[DOPO_GAMES_PAGE + 1];

static void dopo_games_draw(void);
static void dopo_games_pick(int choice);
static void dopo_games_next_page(int choice);

static menu_t dopo_games_def =
{
  0,
  &dopo_settings_def,
  dopo_games_items,
  dopo_games_draw,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

/**
 * @brief Builds the current page: one item per game, named after its file
 * without extension, then Next Page when the list is longer than a page.
 */
static void dopo_games_fill(void)
{
  int n = 0;
  int i;

  for (i = dopo_games_first; i < dopo_games_total && n < DOPO_GAMES_PAGE; i++, n++)
  {
    char *label = dopo_game_labels[n];
    char *dot;

    snprintf(label, DOPO_GAME_LABEL, "%s", dopo_games_name(i));
    if ((dot = strrchr(label, '.')) != NULL)
      *dot = 0;
    dopo_games_items[n] = (menuitem_t){ 1, "", dopo_games_pick, 0, label };
  }

  if (dopo_games_total > DOPO_GAMES_PAGE)
  {
    dopo_games_items[n++] = (menuitem_t){ 1, "", dopo_games_next_page, 'n', "Next Page" };
    snprintf(dopo_games_page_text, sizeof(dopo_games_page_text), "PAGE %d/%d",
             dopo_games_first / DOPO_GAMES_PAGE + 1,
             (dopo_games_total + DOPO_GAMES_PAGE - 1) / DOPO_GAMES_PAGE);
  }
  else
    dopo_games_page_text[0] = 0;

  dopo_games_def.numitems = n;
  if (dopo_games_def.lastOn >= n)
    dopo_games_def.lastOn = 0;
}

/**
 * @brief Title, the running game marked CURRENT and the page.
 */
static void dopo_games_draw(void)
{
  int row;

  dopo_menu_big(&dopo_games_def);
  dopo_menu_title("CHANGE GAME");
  if (!dopo_games_total)
  {
    dopo_menu_small_centered(60, "NO WADS NEXT TO THIS GAME", CR_GRAY);
    return;
  }

  for (row = 0; row < DOPO_GAMES_PAGE && dopo_games_first + row < dopo_games_total; row++)
    if (dopo_games_is_current(dopo_games_first + row))
      /* small, so a long name still fits beside it */
      M_WriteText(DOPO_MENU_RIGHT - M_StringWidth("CURRENT"),
                  dopo_games_def.y + LINEHEIGHT*row + 4, "CURRENT", CR_GOLD);

  if (dopo_games_page_text[0])
    M_WriteText(DOPO_MENU_RIGHT - M_StringWidth(dopo_games_page_text), 188,
                dopo_games_page_text, CR_GOLD);
}

static void dopo_games_next_page(int choice)
{
  dopo_games_first += DOPO_GAMES_PAGE;
  if (dopo_games_first >= dopo_games_total)
    dopo_games_first = 0;
  dopo_games_def.lastOn = 0;
  dopo_games_fill();
  M_SetupNextMenu(&dopo_games_def);
}

static void dopo_game_confirm_draw(void);
static void dopo_game_confirm(int choice);
static void dopo_game_cancel(int choice);

static menuitem_t dopo_game_confirm_items[] =
{
  {1, "", dopo_game_confirm, 'c', "Change"},
  {1, "", dopo_game_cancel,  'n', "Cancel"},
};

static menu_t dopo_game_confirm_def =
{
  2,
  &dopo_games_def,
  dopo_game_confirm_items,
  dopo_game_confirm_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  1  /* start on Cancel */
};

/**
 * @brief The confirmation's title and the chosen game.
 */
static void dopo_game_confirm_draw(void)
{
  dopo_menu_small(&dopo_game_confirm_def);
  dopo_menu_title("CHANGE GAME");
  dopo_menu_small_centered(40, dopo_games_name(dopo_game_chosen), CR_GOLD);
  dopo_menu_small_centered(52, "UNSAVED PROGRESS WILL BE LOST", CR_GRAY);
}

static void dopo_games_pick(int choice)
{
  dopo_game_chosen = dopo_games_first + choice;
  dopo_game_confirm_def.lastOn = 1;
  M_SetupNextMenu(&dopo_game_confirm_def);
}

/**
 * @brief Asks for the switch and closes the menu; the game changes at the
 * next frame.
 */
static void dopo_game_confirm(int choice)
{
  dopo_games_load(dopo_game_chosen);
  M_ClearMenus();
}

static void dopo_game_cancel(int choice)
{
  M_SetupNextMenu(&dopo_games_def);
}

/**
 * @brief Settings routine: lists the games and opens Change Game.
 */
void dopo_change_game_open(int choice)
{
  if (dopo_menu_netgame_blocked())
    return;
  dopo_games_total = dopo_games_scan();
  dopo_games_first = 0;
  dopo_games_fill();
  dopo_menu_open(&dopo_games_def);
}

/* @endpatch */
