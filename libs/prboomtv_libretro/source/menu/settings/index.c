/**
 * @brief Settings: Change Game, Dopo Options and Game Options.
 */

/**
 * @brief Settings menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

static void dopo_settings_draw(void);

static menuitem_t dopo_settings_items[] =
{
  {1, "", dopo_change_game_open,  'c', "Change Game"},
  {1, "", dopo_dopo_options_open, 'd', "Dopo Options"},
  {1, "", dopo_game_options_open, 'g', "Game Options"},
};

menu_t dopo_settings_def =
{
  3,
  NULL,
  dopo_settings_items,
  dopo_settings_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

static void dopo_settings_draw(void)
{
  dopo_menu_small(&dopo_settings_def);
  dopo_menu_title("SETTINGS");
}

void dopo_settings_open(int choice)
{
  dopo_settings_def.prevMenu = M_MainMenuDef();
  dopo_menu_open(&dopo_settings_def);
}

/* @endpatch */
