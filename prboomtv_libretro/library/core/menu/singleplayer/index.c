/**
 * @brief Singleplayer: New Game, Load Game, Save Game and Delete Game.
 */

/**
 * @brief Singleplayer menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

static void dopo_singleplayer_draw(void);
static void dopo_singleplayer_new(int choice);
static void dopo_singleplayer_load(int choice);
static void dopo_singleplayer_save(int choice);

static menuitem_t dopo_singleplayer_items[] =
{
  {1, "", dopo_singleplayer_new,  'n', "New Game"},
  {1, "", dopo_singleplayer_load, 'l', "Load Game"},
  {1, "", dopo_singleplayer_save, 's', "Save Game"},
  {1, "", dopo_delete_open,       'd', "Delete Game"},
};

menu_t dopo_singleplayer_def =
{
  4,
  NULL,
  dopo_singleplayer_items,
  dopo_singleplayer_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

static void dopo_singleplayer_draw(void)
{
  dopo_menu_small(&dopo_singleplayer_def);
  dopo_menu_title("SINGLEPLAYER");
}

void dopo_singleplayer_open(int choice)
{
  dopo_singleplayer_def.prevMenu = M_MainMenuDef();
  dopo_menu_open(&dopo_singleplayer_def);
}

/**
 * @brief New Game, coming back here: M_NewGame opens the episode, class
 * or skill menu, whose back link M_Init points at the main menu, so it is
 * set on every call.
 */
static void dopo_singleplayer_new(int choice)
{
  const menu_t *before = currentMenu;

  if (dopo_menu_netgame_blocked())
    return;
  M_NewGame(0);
  if (currentMenu != before)
    currentMenu->prevMenu = &dopo_singleplayer_def;
}

/**
 * @brief Load and Save come back here; M_Init points them at the main
 * menu (or Raven's files menu) on every load.
 */
static void dopo_singleplayer_load(int choice)
{
  if (dopo_menu_netgame_blocked())
    return;
  LoadDef.prevMenu = &dopo_singleplayer_def;
  M_LoadGame(choice);
}

static void dopo_singleplayer_save(int choice)
{
  SaveDef.prevMenu = &dopo_singleplayer_def;
  M_SaveGame(choice);
}

/* @endpatch */
