/**
 * @brief Main menu, set from the game state every time it opens:
 *
 *   title screen:   Singleplayer, Multiplayer, Settings, Exit
 *   playing alone:  Change Weapon, Singleplayer, Multiplayer, Settings, Exit
 *   netgame:        Change Weapon, Scoreboard, Multiplayer, Settings, Exit
 *
 * Every game gets it, in its own big font, under the game's logo on the
 * anchor every Dopo menu shares (include/prboomtv/menu.h).
 */

/**
 * @brief Doom main menu, room for its longest form.
 *
 * M_Init still folds MainMenu[readthis] into Quit Game for Doom II, so
 * readthis aliases quitdoom to keep that a no-op.
 *
 * @patch src/m_menu.c 375-413
 */
#include "prboomtv/menu.h"

enum
{
  mainfirst = 0,
  quitdoom = 4,
  main_end,
  readthis = quitdoom
} main_e;

menuitem_t MainMenu[main_end];

menu_t MainDef =
{
  main_end,       // number of menu items
  NULL,           // previous menu screen
  MainMenu,       // table that defines menu items
  M_DrawMainMenu, // drawing routine
  97,64,          // initial cursor position
  0               // last menu item the user was on
};
/* @endpatch */

/**
 * @brief The Doom logo (M_DOOM), centered on the screen by the box it is
 * drawn in (its offsets counted), so the small menu column lines up under
 * it in any WAD. Vanilla's x=94 left the stock logo 4.5px off center.
 *
 * Not on the title page: its art already carries the game's logo, which
 * M_DOOM only matches in some WADs; the logo shows once the page gives
 * way to the demos, or in a game.
 *
 * @patch src/m_menu.c 464-473
 */
  else if (W_CheckNumForName("M_DOOM") >= 0 && !dopo_title_page())
  {
    const rpatch_t *logo = R_CachePatchName("M_DOOM");
    const int x = (320 - logo->width) / 2 + logo->leftoffset;

    R_UnlockPatchName("M_DOOM");
    V_DrawNamePatch(x, 2, 0, "M_DOOM", CR_DEFAULT, VPT_STRETCH);
  }
/* @endpatch */

/**
 * @brief Whether the title page is on screen.
 *
 * @patch src/d_main.c 475
 */
dbool dopo_title_page(void)
{
  return gamestate == GS_DEMOSCREEN && pagename &&
         (!strcmp(pagename, "TITLEPIC") || !strcmp(pagename, "TITLE"));
}
/* @endpatch */

/**
 * @brief Heretic/Hexen main menu, the same as Doom's; info (Read This!)
 * moved to Game Options.
 *
 * @patch src/m_menu.c 488-505
 */
enum
{
  rv_first,
  rv_main_end = 5
} raven_main_e;

menuitem_t RavenMainMenu[rv_main_end];
/* @endpatch */

/**
 * @brief Main menu items from the game state.
 *
 * @patch src/m_menu.c 5589
 */

/**
 * @brief Appends an item to the main menu, labelled for the game (Raven
 * menus are lower case).
 */
static void dopo_main_item(menuitem_t *items, int *count, void (*routine)(int),
                           char key, const char *label, const char *raven_label)
{
  items[(*count)++] = (menuitem_t){1, "", routine, key, raven ? raven_label : label};
}

/**
 * @brief Sets the main menu from scratch, since M_Init restores and
 * shrinks MainDef on each content load: the items of the game state, in
 * the same place whatever their count, the cursor on the first one.
 */
static void dopo_main_menu_sync(void)
{
  const dbool playing = gamestate == GS_LEVEL && !demoplayback;
  const dbool online = dopo_mp_state() == DOPO_MP_GAME;
  menu_t *def = raven ? &RavenMainDef : &MainDef;
  menuitem_t *items = raven ? RavenMainMenu : MainMenu;
  int count = 0;

  if (playing)
    dopo_main_item(items, &count, dopo_change_weapon_open, 'w', "Change Weapon", "change weapon");
  if (online)
    dopo_main_item(items, &count, dopo_scoreboard_open, 'b', "Scoreboard", "scoreboard");
  else
    dopo_main_item(items, &count, dopo_singleplayer_open, 'p', "Singleplayer", "singleplayer");
  dopo_main_item(items, &count, dopo_multiplayer_open, 'm', "Multiplayer", "multiplayer");
  dopo_main_item(items, &count, dopo_settings_open, 's', "Settings", "settings");
  dopo_main_item(items, &count, M_QuitDOOM, 'e', "Exit", "exit");

  def->menuitems = items;
  def->numitems = count;
  dopo_menu_small(def);
  def->lastOn = 0;
}

/* @endpatch */

/**
 * @brief Opens the control panel with the main menu synced to the game
 * state.
 *
 * @patch src/m_menu.c 5589-5610
 */
void M_StartControlPanel (void)
{
  // intro might call this repeatedly

  if (menuactive)
    return;

  //jff 3/24/98 make default skill menu choice follow -skill or defaultskill
  //from command line or config file
  //
  // killough 10/98:
  // Fix to make "always floating" with menu selections, and to always follow
  // defaultskill, instead of -skill.

  NewDef.lastOn = defaultskill - 1;

  default_verify = 0;                  // killough 10/98
  menuactive = mnact_float;
  dopo_main_menu_sync();
  currentMenu = M_MainMenuDef();  // JDC (Heretic/Hexen use the Raven menu)
  itemOn = currentMenu->lastOn;   // JDC
  print_warning_about_changes = FALSE;   // killough 11/98
}
/* @endpatch */
