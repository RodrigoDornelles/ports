/**
 * @brief Change Weapon: lists the collected weapons with their ammo and
 * raises the chosen one, through the ticcmd.
 *
 * Opened from the main menu while a level is played. A two column menu:
 * names in the big font of the game, ammo in gold.
 */

/**
 * @brief Change Weapon submenu.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

#define DOPO_WEAPON_HEXEN_SLOTS 4

/**
 * @brief A weapon and its menu name, listed in weapon slot order.
 */
typedef struct
{
  weapontype_t weapon;
  const char *name;
} dopo_weapon_name_t;

static const dopo_weapon_name_t dopo_doom_weapons[] =
{
  { WP_FIST,         "Fist" },
  { WP_CHAINSAW,     "Chainsaw" },
  { WP_PISTOL,       "Pistol" },
  { WP_SHOTGUN,      "Shotgun" },
  { WP_SUPERSHOTGUN, "Super Shotgun" },
  { WP_CHAINGUN,     "Chaingun" },
  { WP_MISSILE,      "Rocket Launcher" },
  { WP_PLASMA,       "Plasma Rifle" },
  { WP_BFG,          "BFG 9000" },
};

/* heretic_weaponinfo rows: staff, wand, crossbow, blaster, skull rod,
 * phoenix, mace, gauntlets */
static const dopo_weapon_name_t dopo_heretic_weapons[] =
{
  { 0, "staff" },
  { 7, "gauntlets" },
  { 1, "elven wand" },
  { 2, "crossbow" },
  { 3, "dragon claw" },
  { 4, "hellstaff" },
  { 5, "phoenix rod" },
  { 6, "firemace" },
};

static const char *const dopo_hexen_weapons[NUMCLASSES][DOPO_WEAPON_HEXEN_SLOTS] =
{
  [PCLASS_FIGHTER] = { "gauntlets", "axe", "hammer", "quietus" },
  [PCLASS_CLERIC]  = { "mace", "serpent staff", "firestorm", "wraithverge" },
  [PCLASS_MAGE]    = { "wand", "frost shards", "arc of death", "bloodscourge" },
};

static menuitem_t   dopo_weapon_items[NUMWEAPONS];
static weapontype_t dopo_weapon_slots[NUMWEAPONS];

static void dopo_draw_weapons(void);

static menu_t dopo_weapon_def =
{
  0,
  NULL,
  dopo_weapon_items,
  dopo_draw_weapons,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

/**
 * @brief Writes the ammo left for a weapon into buf, or returns NULL for
 * a weapon that needs none.
 */
static const char *dopo_weapon_ammo(weapontype_t weapon, char *buf, size_t size)
{
  player_t *player = &players[consoleplayer];

  if (hexen)
  {
    int mana = WeaponInfo[weapon][player->class].mana;

    if (mana == MANA_BOTH)
      snprintf(buf, size, "%d/%d", player->mana[MANA_1], player->mana[MANA_2]);
    else if (mana == MANA_1 || mana == MANA_2)
      snprintf(buf, size, "%d", player->mana[mana]);
    else
      return NULL;
  }
  else
  {
    ammotype_t ammo = weaponinfo[weapon].ammo;

    if (ammo == AM_NOAMMO)
      return NULL;
    snprintf(buf, size, "%d", player->ammo[ammo]);
  }
  return buf;
}

/**
 * @brief Menu routine: title, and each weapon's ammo right aligned on its
 * row.
 */
static void dopo_draw_weapons(void)
{
  int i;

  dopo_menu_big(&dopo_weapon_def);
  dopo_menu_title("CHANGE WEAPON");

  for (i = 0; i < dopo_weapon_def.numitems; i++)
  {
    char buf[16];
    const char *ammo = dopo_weapon_ammo(dopo_weapon_slots[i], buf, sizeof(buf));

    if (ammo)
      dopo_menu_value(&dopo_weapon_def, i, ammo, CR_GOLD);
  }
}

/**
 * @brief Weapon chosen in the submenu, sent by the next ticcmd.
 */
weapontype_t dopo_weapon_request = WP_NOCHANGE;

/**
 * @brief Item routine: raises the chosen weapon the way a weapon key does,
 * through the ticcmd (so a netgame changes it on every machine), leaving
 * the actual swap to A_WeaponReady, and returns to the game.
 */
static void dopo_choose_weapon(int choice)
{
  weapontype_t weapon = dopo_weapon_slots[choice];

  if (weapon != players[consoleplayer].readyweapon)
    dopo_weapon_request = weapon;

  M_ClearMenus();
}

/**
 * @brief Appends a weapon to the submenu when the player owns it.
 */
static void dopo_add_weapon(weapontype_t weapon, const char *name)
{
  player_t *player = &players[consoleplayer];
  int n = dopo_weapon_def.numitems;

  if (!player->weaponowned[weapon])
    return;

  /* P_PlayerThink never raises these in shareware, even if cheated */
  if (!raven && gamemode == shareware && (weapon == WP_PLASMA || weapon == WP_BFG))
    return;

  dopo_weapon_items[n] = (menuitem_t){ 1, "", dopo_choose_weapon, 0, name };
  dopo_weapon_slots[n] = weapon;
  dopo_weapon_def.numitems++;
}

/**
 * @brief Main menu routine: lists the collected weapons for the current
 * game and opens the submenu.
 */
void dopo_change_weapon_open(int choice)
{
  player_t *player = &players[consoleplayer];
  size_t i;

  if (gamestate != GS_LEVEL || !player->mo)
    return;

  dopo_weapon_def.numitems = 0;
  dopo_weapon_def.prevMenu = M_MainMenuDef();

  if (hexen)
  {
    if (player->class > PCLASS_NULL && player->class < PCLASS_PIG)
      for (i = 0; i < DOPO_WEAPON_HEXEN_SLOTS; i++)
        dopo_add_weapon((weapontype_t)i, dopo_hexen_weapons[player->class][i]);
  }
  else if (heretic)
  {
    for (i = 0; i < sizeof(dopo_heretic_weapons) / sizeof(*dopo_heretic_weapons); i++)
      dopo_add_weapon(dopo_heretic_weapons[i].weapon, dopo_heretic_weapons[i].name);
  }
  else
  {
    for (i = 0; i < sizeof(dopo_doom_weapons) / sizeof(*dopo_doom_weapons); i++)
      dopo_add_weapon(dopo_doom_weapons[i].weapon, dopo_doom_weapons[i].name);
  }

  if (dopo_weapon_def.numitems)
    dopo_menu_open(&dopo_weapon_def);
}

/* @endpatch */

/**
 * @brief Turns the weapon chosen in the submenu into a weapon change of
 * the ticcmd, over any weapon key of the same tic; P_PlayerThink still
 * skips it while morphed.
 *
 * @patch src/g_game.c 580
 */
  if (dopo_weapon_request != WP_NOCHANGE)
  {
    newweapon = dopo_weapon_request;
    dopo_weapon_request = WP_NOCHANGE;
  }

/* @endpatch */

/**
 * @brief The weapon request, for G_BuildTiccmd.
 *
 * @patch src/g_game.c 311
 */
#include "prboomtv/change_weapon.h"

/* @endpatch */
