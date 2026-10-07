/**
 * @brief Game Options: PrBoom's options menu without Mouse Sensitivity,
 * plus the game's Read This!. It keeps PrBoom's own layout, as its
 * submenus do.
 */

/**
 * @brief Read This! returns to the menu it was opened from.
 *
 * @patch src/m_menu.c 642-645
 */
void M_FinishReadThis(int choice)
{
  M_SetupNextMenu(ReadDef1.prevMenu);
}
/* @endpatch */

/**
 * @brief Game Options items.
 *
 * @patch src/m_menu.c 1417-1454
 */
#include "dopo/menu.h"

enum
{
  general, // killough 10/98
  // killough 4/6/98: move setup to be a sub-menu of OPTIONs
  setup,                                                    // phares 3/21/98
  endgame,
  messages,
  scrnsize,
  soundvol,
  gamereadthis,
  opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
  {1,"",M_General,'g',"General"},
  {1,"",M_Setup,'s',"Setup"},
  {1,"",M_EndGame,'e',"End Game"},
  {1,"",M_ChangeMessages,'m',"Messages"},
  {2,"",M_SizeDisplay,'s',"Screen Size"},
  {1,"",M_Sound,'s',"Sound Volume"},
  {1,"",dopo_read_this,'r',"Read This!"},
};

menu_t OptionsDef =
{
  opt_end,
  &MainDef,
  OptionsMenu,
  M_DrawOptions,
  60,37,
  0
};
/* @endpatch */

/**
 * @brief Game Options title and the Messages state, in the big font.
 *
 * Upstream also drew the old detail HIGH/LOW graphic on the Screen Size
 * row, a leftover of the removed detail item; it is gone.
 *
 * @patch src/m_menu.c 1463-1494
 */
void M_DrawOptions(void)
{
  dopo_text_centered(DOPO_MENU_TITLE_Y, "GAME OPTIONS", CR_DEFAULT);
  dopo_text(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT*messages,
            showMessages ? "On" : "Off", CR_GOLD);
}
/* @endpatch */

/**
 * @brief Opening Game Options and its Read This!.
 *
 * @patch src/m_menu.c 5589
 */

/**
 * @brief Settings routine: opens Game Options; M_Init points its back
 * link at the main menu on every load, so it is set here.
 */
void dopo_game_options_open(int choice)
{
  OptionsDef.prevMenu = &dopo_settings_def;
  dopo_menu_open(&OptionsDef);
}

/**
 * @brief Game Options routine: the game's Read This!, returning to Game
 * Options.
 */
void dopo_read_this(int choice)
{
  ReadDef1.prevMenu = &OptionsDef;
  M_ReadThis(choice);
}

/* @endpatch */
