/**
 * @brief Switch Weapon: lists the collected weapons with their ammo and
 * raises the chosen one.
 *
 * Opened from the main menu (see dopo/menus.c) while a level is played.
 * Names are drawn in the big font of the game, ammo in gold.
 */

/**
 * @brief Switch Weapon submenu.
 *
 * @patch src/m_menu.c 5589
 */
#define DOPO_WEAPON_HEXEN_SLOTS 4
#define DOPO_WEAPON_AMMO_RIGHT  288

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
  48,40,
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

  dopo_text_centered(15, "SWITCH WEAPON", CR_DEFAULT);

  for (i = 0; i < dopo_weapon_def.numitems; i++)
  {
    char buf[16];
    const char *ammo = dopo_weapon_ammo(dopo_weapon_slots[i], buf, sizeof(buf));

    if (ammo)
      dopo_text(DOPO_WEAPON_AMMO_RIGHT - dopo_text_width(ammo),
                dopo_weapon_def.y + i * LINEHEIGHT, ammo, CR_GOLD);
  }
}

/**
 * @brief Item routine: raises the chosen weapon the way a weapon key does,
 * leaving the actual swap to A_WeaponReady, and returns to the game.
 */
static void dopo_choose_weapon(int choice)
{
  player_t *player = &players[consoleplayer];
  weapontype_t weapon = dopo_weapon_slots[choice];

  if (!player->morphTics && !player->chickenTics &&
      weapon != player->readyweapon)
    player->pendingweapon = weapon;

  M_ClearMenus();
}

/**
 * @brief Appends a weapon to the submenu when the player owns it, placing
 * the cursor on the weapon in hand.
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
  if (weapon == player->readyweapon)
    dopo_weapon_def.lastOn = n;
  dopo_weapon_def.numitems++;
}

/**
 * @brief Main menu routine: lists the collected weapons for the current
 * game and opens the submenu.
 */
static void dopo_switch_weapon(int choice)
{
  player_t *player = &players[consoleplayer];
  size_t i;

  if (gamestate != GS_LEVEL || !player->mo)
    return;

  dopo_weapon_def.numitems = 0;
  dopo_weapon_def.lastOn = 0;
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
    M_SetupNextMenu(&dopo_weapon_def);
}

/* @endpatch */
