/**
 * @brief Dopo Options: Patchs, Cheats and the Dopo Read This!.
 */

/**
 * @brief Dopo Options menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

static void dopo_dopo_options_draw(void);

static menuitem_t dopo_dopo_options_items[] =
{
  {1, "", dopo_patchs_open, 'p', "Patchs"},
  {1, "", dopo_cheats_open, 'c', "Cheats"},
  {1, "", dopo_about_open,  'r', "Read This!"},
};

menu_t dopo_dopo_options_def =
{
  3,
  &dopo_settings_def,
  dopo_dopo_options_items,
  dopo_dopo_options_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

static void dopo_dopo_options_draw(void)
{
  dopo_menu_small(&dopo_dopo_options_def);
  dopo_menu_title("DOPO OPTIONS");
}

void dopo_dopo_options_open(int choice)
{
  dopo_menu_open(&dopo_dopo_options_def);
}

/* @endpatch */
