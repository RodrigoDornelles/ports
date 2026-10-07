/**
 * @brief Patchs: each patch with its state, a two column menu
 * (library/core/patchs/).
 */

/**
 * @brief Patchs menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

static void dopo_toggle_action_button(int choice)
{
  dopo_action_button = !dopo_action_button;
}

static void dopo_toggle_auto_fist(int choice)
{
  dopo_auto_fist = !dopo_auto_fist;
}

static void dopo_toggle_toggle_fire(int choice)
{
  dopo_toggle_fire = !dopo_toggle_fire;
}

/**
 * @brief Side Walk's double tap window, OFF and 100ms to 300ms in 50ms
 * steps; right (and confirm) goes up, left goes down, both wrapping
 * around.
 */
static void dopo_adjust_side_walk(int choice)
{
  static const int steps[] = { 0, 100, 150, 200, 250, 300 };

  dopo_menu_cycle(&dopo_side_walk_ms, steps, sizeof(steps) / sizeof(*steps), choice);
}

static void dopo_patchs_draw(void);

static menuitem_t dopo_patchs_items[] =
{
  {1, "", dopo_toggle_action_button, 'a', "Action Button"},
  {1, "", dopo_toggle_auto_fist,     'f', "Auto Fist"},
  {1, "", dopo_toggle_toggle_fire,   't', "Toggle Fire"},
  {2, "", dopo_adjust_side_walk,     's', "Side Walk"},
};

static menu_t dopo_patchs_def =
{
  4,
  &dopo_dopo_options_def,
  dopo_patchs_items,
  dopo_patchs_draw,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

static void dopo_patchs_draw(void)
{
  static const dbool *const values[] =
  {
    &dopo_action_button, &dopo_auto_fist, &dopo_toggle_fire,
  };
  char side_walk[16];

  dopo_menu_big(&dopo_patchs_def);
  dopo_menu_title("PATCHS");
  dopo_menu_toggles(&dopo_patchs_def, values, 3);
  dopo_menu_format(side_walk, sizeof(side_walk), dopo_side_walk_ms, "MS");
  dopo_menu_value(&dopo_patchs_def, 3, side_walk, CR_GOLD);
}

void dopo_patchs_open(int choice)
{
  dopo_menu_open(&dopo_patchs_def);
}

/* @endpatch */
