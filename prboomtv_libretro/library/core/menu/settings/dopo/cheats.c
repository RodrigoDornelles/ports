/**
 * @brief Cheats: each cheat with its state, a two column menu
 * (library/core/cheats/). The values go through multiplayer/settings.c, so in a
 * netgame only the host changes them, for everyone at the same tic.
 */

/**
 * @brief Cheats menu.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

/**
 * @brief Each routine edits a copy of the cheats asked for last and asks
 * for it; in a netgame only the host may.
 */
static dbool dopo_cheats_edit(dopo_cheats_t *cheats)
{
  if (!dopo_mp_is_host())
  {
    M_StartMessage(DOPO_MP_HOST_ONLY, NULL, FALSE);
    return FALSE;
  }
  *cheats = *dopo_cheats_wanted();
  return TRUE;
}

static void dopo_toggle_cheat_ammo(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    c.ammo = !c.ammo;
    dopo_cheats_request(&c);
  }
}

static void dopo_toggle_cheat_life(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    c.life = !c.life;
    dopo_cheats_request(&c);
  }
}

static void dopo_toggle_cheat_weapons(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    c.weapons = !c.weapons;
    dopo_cheats_request(&c);
  }
}

static void dopo_toggle_cheat_damage(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    c.damage = !c.damage;
    dopo_cheats_request(&c);
  }
}

/**
 * @brief The assists' openings, OFF and then degrees on each side of the
 * crosshair, the same steps for both; 45 covers the 4:3 screen and 50 a
 * bit past it.
 */
static const int dopo_assist_steps[] = { 0, 5, 10, 15, 20, 30, 40, 50 };

static void dopo_adjust_cheat_aim(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    dopo_menu_cycle(&c.aim_assist, dopo_assist_steps,
                    sizeof(dopo_assist_steps) / sizeof(*dopo_assist_steps), choice);
    dopo_cheats_request(&c);
  }
}

static void dopo_adjust_cheat_trigger(int choice)
{
  dopo_cheats_t c;

  if (dopo_cheats_edit(&c))
  {
    dopo_menu_cycle(&c.trigger_assist, dopo_assist_steps,
                    sizeof(dopo_assist_steps) / sizeof(*dopo_assist_steps), choice);
    dopo_cheats_request(&c);
  }
}

static void dopo_cheats_draw(void);

static menuitem_t dopo_cheats_items[] =
{
  {1, "", dopo_toggle_cheat_ammo,    'a', "Infinite Ammo"},
  {1, "", dopo_toggle_cheat_life,    'l', "Infinite Life"},
  {1, "", dopo_toggle_cheat_damage,  'd', "Infinite Damage"},
  {1, "", dopo_toggle_cheat_weapons, 'w', "All Weapons"},
  {2, "", dopo_adjust_cheat_aim,     'i', "Aim Assist"},
  {2, "", dopo_adjust_cheat_trigger, 't', "Trigger Assist"},
};

static menu_t dopo_cheats_def =
{
  6,
  &dopo_dopo_options_def,
  dopo_cheats_items,
  dopo_cheats_draw,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

static void dopo_cheats_draw(void)
{
  const dopo_cheats_t *cheats = dopo_cheats_wanted();
  const dbool *const values[] =
  {
    &cheats->ammo, &cheats->life, &cheats->damage, &cheats->weapons,
  };
  char aim[16], trigger[16];

  dopo_menu_big(&dopo_cheats_def);
  dopo_menu_title("CHEATS");
  dopo_menu_toggles(&dopo_cheats_def, values, 4);
  dopo_menu_format(aim, sizeof(aim), cheats->aim_assist, "DEG");
  dopo_menu_format(trigger, sizeof(trigger), cheats->trigger_assist, "DEG");
  dopo_menu_value(&dopo_cheats_def, 4, aim, CR_GOLD);
  dopo_menu_value(&dopo_cheats_def, 5, trigger, CR_GOLD);
}

void dopo_cheats_open(int choice)
{
  dopo_menu_open(&dopo_cheats_def);
}

/* @endpatch */
