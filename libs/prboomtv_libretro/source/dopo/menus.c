/**
 * @brief Menu layout for the TV: short main menu and split options.
 *
 * Every game gets the Heretic/Hexen main menu: New Game, Game Files,
 * Settings, Quit Game, where New Game turns into Switch Weapon while a level
 * is played. Game Files holds New Game, Load Game, Save Game and Delete
 * Game. Settings leads to Dopo Options (patches, cheats and Read This!),
 * Game Options (the engine options, without Mouse Sensitivity and with the
 * game's Read This!) and Change Game. Every item is drawn as text in the
 * big font.
 */

/**
 * @brief Doom main menu.
 *
 * The first item is New Game or Switch Weapon, set by
 * dopo_main_menu_sync() on every open. M_Init still folds
 * MainMenu[readthis] into Quit Game for Doom II, so readthis aliases
 * quitdoom to keep that a no-op.
 *
 * @patch src/m_menu.c 375-413
 */
enum
{
  mainfirst = 0,
  gamefiles,
  options,
  quitdoom,
  main_end,
  readthis = quitdoom
} main_e;

static void dopo_switch_weapon(int choice);
static void dopo_options(int choice);
static void dopo_game_files(int choice);

menuitem_t MainMenu[]=
{
  {1,"",NULL,0,NULL},  /* New Game or Switch Weapon */
  {1,"",dopo_game_files,'g',"Game Files"},
  {1,"",dopo_options,'s',"Settings"},
  {1,"",M_QuitDOOM,'q',"Quit Game"}
};

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
 * @brief Heretic/Hexen main menu, with the same items as Doom's; info
 * (Read This!) moved to Game Options.
 *
 * @patch src/m_menu.c 488-505
 */
enum
{
  rv_first,
  rv_files,
  rv_options,
  rv_quit,
  rv_main_end
} raven_main_e;

menuitem_t RavenMainMenu[] =
{
  {1, "", NULL, 0, NULL},  /* new game or switch weapon */
  {1, "", dopo_game_files, 'g', "game files"},
  {1, "", dopo_options,    's', "settings"},
  {1, "", M_QuitDOOM,      'q', "quit game"}
};
/* @endpatch */

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
 * @brief Game Options: the engine options without Mouse Sensitivity, plus
 * the game's Read This!.
 *
 * @patch src/m_menu.c 1417-1454
 */
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

static void dopo_read_this(int choice);

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
  dopo_text_centered(15, "GAME OPTIONS", CR_DEFAULT);
  dopo_text(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT*messages,
            showMessages ? "On" : "Off", CR_GOLD);
}
/* @endpatch */

/**
 * @brief Options chooser, Dopo Options, Patchs and the Dopo Read This!.
 *
 * @patch src/m_menu.c 5589
 */
#define DOPO_TEXT_X       10
#define DOPO_LINE_HEIGHT  9

extern dbool dopo_action_button;
extern dbool dopo_auto_fist;
extern dbool dopo_toggle_fire;
extern int dopo_side_walk_ms;

/**
 * @brief Menu routine: title of Settings, the Dopo/Game Options chooser.
 */
static void dopo_draw_options(void)
{
  dopo_text_centered(15, "SETTINGS", CR_DEFAULT);
}

static void dopo_open_dopo_options(int choice);
static void dopo_open_game_options(int choice);
static void dopo_open_change_game(int choice);

static menuitem_t dopo_options_items[] =
{
  {1, "", dopo_open_dopo_options, 'd', "Dopo Options"},
  {1, "", dopo_open_game_options, 'g', "Game Options"},
  {1, "", dopo_open_change_game,  'c', "Change Game"},
};

static menu_t dopo_options_def =
{
  3,
  NULL,
  dopo_options_items,
  dopo_draw_options,
  80,64,
  0
};

/**
 * @brief Main menu routine: opens Settings.
 */
static void dopo_options(int choice)
{
  dopo_options_def.prevMenu = M_MainMenuDef();
  M_SetupNextMenu(&dopo_options_def);
}

/**
 * @brief Opens Game Options; M_Init points its back link at the main menu
 * on every load, so it is set here.
 */
static void dopo_open_game_options(int choice)
{
  OptionsDef.prevMenu = &dopo_options_def;
  M_SetupNextMenu(&OptionsDef);
}

/**
 * @brief Game Options routine: the game's Read This!, returning to Game
 * Options.
 */
static void dopo_read_this(int choice)
{
  ReadDef1.prevMenu = &OptionsDef;
  M_ReadThis(choice);
}

/**
 * @brief Menu routine: title of Dopo Options.
 */
static void dopo_draw_dopo_options(void)
{
  dopo_text_centered(15, "DOPO OPTIONS", CR_DEFAULT);
}

static void dopo_open_patchs(int choice);
static void dopo_open_cheats(int choice);
static void dopo_open_about(int choice);

static menuitem_t dopo_dopo_items[] =
{
  {1, "", dopo_open_patchs, 'p', "Patchs"},
  {1, "", dopo_open_cheats, 'c', "Cheats"},
  {1, "", dopo_open_about,  'r', "Read This!"},
};

static menu_t dopo_dopo_def =
{
  3,
  &dopo_options_def,
  dopo_dopo_items,
  dopo_draw_dopo_options,
  80,64,
  0
};

static void dopo_open_dopo_options(int choice)
{
  M_SetupNextMenu(&dopo_dopo_def);
}

/**
 * @brief Change Game: the games next to the running content, in pages of
 * DOPO_GAMES_PAGE, and a Change/Cancel confirmation; the switch itself
 * happens in libretro.c (dopo/change_game.c) at the next frame.
 */
extern int         dopo_games_scan(void);
extern const char *dopo_games_name(int i);
extern dbool       dopo_games_is_current(int i);
extern void        dopo_games_load(int i);

#define DOPO_GAMES_PAGE  8
#define DOPO_GAME_LABEL  17

static int  dopo_games_total;
static int  dopo_games_first;
static int  dopo_game_chosen;
static char dopo_game_labels[DOPO_GAMES_PAGE][DOPO_GAME_LABEL];
static char dopo_games_page_text[24];

static menuitem_t dopo_games_items[DOPO_GAMES_PAGE + 1];

/**
 * @brief Menu routine: Change Game title, the running game and the page.
 */
static void dopo_draw_games(void);

static menu_t dopo_games_def =
{
  0,
  &dopo_options_def,
  dopo_games_items,
  dopo_draw_games,
  40,40,
  0
};

static void dopo_games_pick(int choice);
static void dopo_games_next_page(int choice);

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

static void dopo_draw_games(void)
{
  int row;

  dopo_text_centered(15, "CHANGE GAME", CR_DEFAULT);
  if (!dopo_games_total)
  {
    M_WriteText(160 - M_StringWidth("NO WADS NEXT TO THIS GAME") / 2, 60,
                "NO WADS NEXT TO THIS GAME", CR_GRAY);
    return;
  }

  for (row = 0; row < DOPO_GAMES_PAGE && dopo_games_first + row < dopo_games_total; row++)
    if (dopo_games_is_current(dopo_games_first + row))
      dopo_text(dopo_games_def.x + 190, dopo_games_def.y + LINEHEIGHT*row, "CURRENT", CR_GOLD);

  if (dopo_games_page_text[0])
    M_WriteText(310 - M_StringWidth(dopo_games_page_text), 188, dopo_games_page_text, CR_GOLD);
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

/**
 * @brief Menu routine: the confirmation's title and the chosen game.
 */
static void dopo_draw_game_confirm(void)
{
  const char *name = dopo_games_name(dopo_game_chosen);

  dopo_text_centered(15, "CHANGE GAME", CR_DEFAULT);
  M_WriteText(160 - M_StringWidth(name) / 2, 40, name, CR_GOLD);
  M_WriteText(160 - M_StringWidth("UNSAVED PROGRESS WILL BE LOST") / 2, 52,
              "UNSAVED PROGRESS WILL BE LOST", CR_GRAY);
}

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
  dopo_draw_game_confirm,
  120,72,
  1  /* start on Cancel */
};

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
static void dopo_open_change_game(int choice)
{
  dopo_games_total = dopo_games_scan();
  dopo_games_first = 0;
  dopo_games_def.lastOn = 0;
  dopo_games_fill();
  M_SetupNextMenu(&dopo_games_def);
}

/**
 * @brief Draws an item's value in gold, at the same column on every row.
 */
static void dopo_draw_value(const menu_t *def, int item, const char *text)
{
  dopo_text(def->x + 180, def->y + LINEHEIGHT*item, text, CR_GOLD);
}

/**
 * @brief Moves value to the next (choice 1: right or confirm) or previous
 * (choice 0: left) of steps, wrapping around; an unknown value starts
 * from the first step.
 */
static void dopo_cycle(int *value, const int *steps, int count, int choice)
{
  int i = 0;

  while (i < count - 1 && steps[i] != *value)
    i++;
  *value = steps[(i + (choice ? 1 : count - 1)) % count];
}

/**
 * @brief Writes OFF for 0, or the value with its unit (200MS, 10DEG).
 */
static void dopo_format_value(char *buf, size_t size, int value, const char *unit)
{
  if (value)
    snprintf(buf, size, "%d%s", value, unit);
  else
    snprintf(buf, size, "OFF");
}

/**
 * @brief Draws ON/OFF for the first count items of a menu of toggles.
 */
static void dopo_draw_toggles(const menu_t *def, const dbool *const values[], int count)
{
  int i;

  for (i = 0; i < count; i++)
    dopo_draw_value(def, i, *values[i] ? "ON" : "OFF");
}

/**
 * @brief Menu routine: Patchs title and each patch's state.
 */
static void dopo_draw_patchs(void);

/**
 * @brief Patchs routine: turns the contextual action button on or off.
 */
static void dopo_toggle_action_button(int choice)
{
  dopo_action_button = !dopo_action_button;
}

/**
 * @brief Patchs routine: turns Auto Fist on or off.
 */
static void dopo_toggle_auto_fist(int choice)
{
  dopo_auto_fist = !dopo_auto_fist;
}

/**
 * @brief Patchs routine: turns Toggle Fire on or off.
 */
static void dopo_toggle_toggle_fire(int choice)
{
  dopo_toggle_fire = !dopo_toggle_fire;
}

/**
 * @brief Patchs routine: Side Walk's double tap window, OFF and 100ms to
 * 300ms in 50ms steps; right (and confirm) goes up, left goes down, both
 * wrapping around.
 */
static void dopo_adjust_side_walk(int choice)
{
  static const int steps[] = { 0, 100, 150, 200, 250, 300 };

  dopo_cycle(&dopo_side_walk_ms, steps, sizeof(steps) / sizeof(*steps), choice);
}

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
  &dopo_dopo_def,
  dopo_patchs_items,
  dopo_draw_patchs,
  60,64,
  0
};

static void dopo_draw_patchs(void)
{
  static const dbool *const values[] =
  {
    &dopo_action_button, &dopo_auto_fist, &dopo_toggle_fire,
  };
  char side_walk[16];

  dopo_format_value(side_walk, sizeof(side_walk), dopo_side_walk_ms, "MS");

  dopo_text_centered(15, "PATCHS", CR_DEFAULT);
  dopo_draw_toggles(&dopo_patchs_def, values, 3);
  dopo_draw_value(&dopo_patchs_def, 3, side_walk);
}

static void dopo_open_patchs(int choice)
{
  M_SetupNextMenu(&dopo_patchs_def);
}

/**
 * @brief Cheats, kept up every tic by dopo_cheats_apply() in libretro.c.
 */
extern dbool dopo_cheat_ammo;
extern dbool dopo_cheat_life;
extern dbool dopo_cheat_weapons;
extern dbool dopo_cheat_damage;
extern int dopo_cheat_aim_assist;
extern int dopo_cheat_trigger_assist;

static void dopo_toggle_cheat_ammo(int choice)    { dopo_cheat_ammo = !dopo_cheat_ammo; }
static void dopo_toggle_cheat_life(int choice)    { dopo_cheat_life = !dopo_cheat_life; }
static void dopo_toggle_cheat_weapons(int choice) { dopo_cheat_weapons = !dopo_cheat_weapons; }
static void dopo_toggle_cheat_damage(int choice)  { dopo_cheat_damage = !dopo_cheat_damage; }
/**
 * @brief Cheats routines: the assists' openings, OFF and then degrees on
 * each side of the crosshair, the same steps for both; 45 covers the 4:3
 * screen and 50 a bit past it.
 */
static const int dopo_assist_steps[] = { 0, 5, 10, 15, 20, 30, 40, 50 };

static void dopo_adjust_cheat_aim(int choice)
{
  dopo_cycle(&dopo_cheat_aim_assist, dopo_assist_steps,
             sizeof(dopo_assist_steps) / sizeof(*dopo_assist_steps), choice);
}

static void dopo_adjust_cheat_trigger(int choice)
{
  dopo_cycle(&dopo_cheat_trigger_assist, dopo_assist_steps,
             sizeof(dopo_assist_steps) / sizeof(*dopo_assist_steps), choice);
}

/**
 * @brief Menu routine: Cheats title and each cheat's state.
 */
static void dopo_draw_cheats(void);

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
  &dopo_dopo_def,
  dopo_cheats_items,
  dopo_draw_cheats,
  60,64,
  0
};

static void dopo_draw_cheats(void)
{
  static const dbool *const values[] =
  {
    &dopo_cheat_ammo, &dopo_cheat_life, &dopo_cheat_damage, &dopo_cheat_weapons,
  };
  char aim[16], trigger[16];

  dopo_format_value(aim, sizeof(aim), dopo_cheat_aim_assist, "DEG");
  dopo_format_value(trigger, sizeof(trigger), dopo_cheat_trigger_assist, "DEG");

  dopo_text_centered(15, "CHEATS", CR_DEFAULT);
  dopo_draw_toggles(&dopo_cheats_def, values, 4);
  dopo_draw_value(&dopo_cheats_def, 4, aim);
  dopo_draw_value(&dopo_cheats_def, 5, trigger);
}

static void dopo_open_cheats(int choice)
{
  M_SetupNextMenu(&dopo_cheats_def);
}

/**
 * @brief Body of the Dopo Read This!, one entry per screen line, drawn in
 * the small font as is: no wrapping, so each line must fit the screen
 * (~300px from DOPO_TEXT_X). A line starting with '#' is a heading, drawn
 * in gold without the '#'; an empty string leaves a blank line.
 */
static const char *const dopo_about_lines[] =
{
  "Ported to the television, where remotes",
  "and gamepads have few and stiff buttons.",
  "",
  "#LEGAL",
  "PrBoomTV is a free software",
  "GNU GPL version 2.",
  "",
  "Based on libretro-prboom",
  "GNU GPL version 2.",
  "",
  "Font from Odamex, (c) The Odamex Team",
  "GNU GPL version 2.",
  "",
  "#SOURCE CODE",
  "http://github.com/rodrigodornelles/ports",
};

/**
 * @brief Menu routine: the about page, full screen.
 */
static void dopo_draw_about(void)
{
  size_t i;
  int y = 42;

  menuactive = mnact_full;
  M_DrawBackground(g_menu_flat, 0);

  dopo_text_centered(8, "Rodrigo Dornelles Ports", CR_DEFAULT);
  M_WriteText(160 - M_StringWidth("PrBoomTV, a Doom engine core for libretro") / 2,
              26, "PrBoomTV, a Doom engine core for libretro", CR_GOLD);

  for (i = 0; i < sizeof(dopo_about_lines) / sizeof(*dopo_about_lines); i++, y += DOPO_LINE_HEIGHT)
  {
    const char *line = dopo_about_lines[i];

    if (line[0] == '#')
      M_WriteText(DOPO_TEXT_X, y, line + 1, CR_GOLD);
    else
      M_WriteText(DOPO_TEXT_X, y, line, CR_GRAY);
  }
}

/**
 * @brief Read This! routine: any confirm goes back to Dopo Options.
 */
static void dopo_about_close(int choice);

static menuitem_t dopo_about_items[] =
{
  {1, "", dopo_about_close, 0, NULL},
};

static menu_t dopo_about_def =
{
  1,
  &dopo_dopo_def,
  dopo_about_items,
  dopo_draw_about,
  360,175,  /* skull and Raven arrow both land off screen */
  0
};

static void dopo_about_close(int choice)
{
  M_SetupNextMenu(&dopo_dopo_def);
}

static void dopo_open_about(int choice)
{
  M_SetupNextMenu(&dopo_about_def);
}

/**
 * @brief Menu routine: New Game, coming back to the menu it was opened
 * from.
 *
 * M_NewGame opens the episode, class or skill menu, whose back link M_Init
 * points at the main menu; it is set to the opener on every call.
 */
static void dopo_new_game_from(menu_t *back)
{
  const menu_t *before = currentMenu;

  M_NewGame(0);
  if (currentMenu != before)
    currentMenu->prevMenu = back;
}

static void dopo_main_new_game(int choice)
{
  dopo_new_game_from(M_MainMenuDef());
}

/**
 * @brief Menu routine: title of Game Files.
 */
static void dopo_draw_game_files(void)
{
  dopo_text_centered(15, "GAME FILES", CR_DEFAULT);
}

static void dopo_files_new_game(int choice);
static void dopo_files_load_game(int choice);
static void dopo_files_save_game(int choice);
static void dopo_files_delete_game(int choice);

static menuitem_t dopo_files_items[] =
{
  {1, "", dopo_files_new_game,    'n', "New Game"},
  {1, "", dopo_files_load_game,   'l', "Load Game"},
  {1, "", dopo_files_save_game,   's', "Save Game"},
  {1, "", dopo_files_delete_game, 'd', "Delete Game"},
};

static menu_t dopo_files_def =
{
  4,
  NULL,
  dopo_files_items,
  dopo_draw_game_files,
  80,64,
  0
};

static void dopo_files_new_game(int choice)
{
  dopo_new_game_from(&dopo_files_def);
}

/**
 * @brief Load and Save come back to Game Files; M_Init points them at the
 * main menu (or Raven's files menu) on every load.
 */
static void dopo_files_load_game(int choice)
{
  LoadDef.prevMenu = &dopo_files_def;
  M_LoadGame(choice);
}

static void dopo_files_save_game(int choice)
{
  SaveDef.prevMenu = &dopo_files_def;
  M_SaveGame(choice);
}

/**
 * @brief Delete Game: the save slots, as on the Load Game screen, and a
 * Delete/Cancel confirmation that works with a single confirm button.
 */
static int dopo_delete_slot;

static void dopo_delete_select(int choice);

static menuitem_t dopo_delete_items[] =
{
  {1, "", dopo_delete_select, '1', NULL},
  {1, "", dopo_delete_select, '2', NULL},
  {1, "", dopo_delete_select, '3', NULL},
  {1, "", dopo_delete_select, '4', NULL},
  {1, "", dopo_delete_select, '5', NULL},
  {1, "", dopo_delete_select, '6', NULL},
  {1, "", dopo_delete_select, '7', NULL},
  {1, "", dopo_delete_select, '8', NULL},
};

/**
 * @brief Menu routine: Delete Game title and the slots.
 */
static void dopo_draw_delete(void);

static menu_t dopo_delete_def =
{
  load_end,
  &dopo_files_def,
  dopo_delete_items,
  dopo_draw_delete,
  80,34,
  0
};

static void dopo_draw_delete(void)
{
  int i;

  dopo_text_centered(LOADGRAPHIC_Y, "DELETE GAME", CR_DEFAULT);
  for (i = 0; i < load_end; i++)
  {
    M_DrawSaveLoadBorder(dopo_delete_def.x, dopo_delete_def.y + LINEHEIGHT*i);
    M_WriteText(dopo_delete_def.x, dopo_delete_def.y + LINEHEIGHT*i,
                savegamestrings[i], CR_DEFAULT);
  }
}

/**
 * @brief Re-reads the slots; empty ones cannot be picked.
 */
static void dopo_delete_refresh(void)
{
  int i;

  M_ReadSaveStrings();
  for (i = 0; i < load_end; i++)
    dopo_delete_items[i].status = LoadMenue[i].status;
}

/**
 * @brief Menu routine: the confirmation's title and the save's name.
 */
static void dopo_draw_delete_confirm(void)
{
  const char *name = savegamestrings[dopo_delete_slot];

  dopo_text_centered(LOADGRAPHIC_Y, "DELETE GAME", CR_DEFAULT);
  M_WriteText(160 - M_StringWidth(name) / 2, 40, name, CR_GOLD);
}

static void dopo_delete_confirm(int choice);
static void dopo_delete_cancel(int choice);

static menuitem_t dopo_delete_confirm_items[] =
{
  {1, "", dopo_delete_confirm, 'd', "Delete"},
  {1, "", dopo_delete_cancel,  'c', "Cancel"},
};

static menu_t dopo_delete_confirm_def =
{
  2,
  &dopo_delete_def,
  dopo_delete_confirm_items,
  dopo_draw_delete_confirm,
  120,64,
  1  /* start on Cancel */
};

static void dopo_delete_select(int choice)
{
  dopo_delete_slot = choice;
  dopo_delete_confirm_def.lastOn = 1;
  M_SetupNextMenu(&dopo_delete_confirm_def);
}

/**
 * @brief Removes the save file, forgetting it as the quicksave slot, and
 * goes back to the refreshed slot list.
 */
static void dopo_delete_confirm(int choice)
{
  char name[PATH_MAX+1];

  G_SaveGameName(name, sizeof(name), dopo_delete_slot, FALSE);
  filestream_delete(name);
  if (quickSaveSlot == dopo_delete_slot)
    quickSaveSlot = -1;

  dopo_delete_refresh();
  M_SetupNextMenu(&dopo_delete_def);
}

static void dopo_delete_cancel(int choice)
{
  M_SetupNextMenu(&dopo_delete_def);
}

static void dopo_files_delete_game(int choice)
{
  dopo_delete_refresh();
  M_SetupNextMenu(&dopo_delete_def);
}

/**
 * @brief Main menu routine: opens Game Files.
 */
static void dopo_game_files(int choice)
{
  dopo_files_def.prevMenu = M_MainMenuDef();
  M_SetupNextMenu(&dopo_files_def);
}

/**
 * @brief Sets the first main menu item: Switch Weapon while a level is
 * played, New Game otherwise.
 *
 * The rest of the menu is set from scratch on every open, since M_Init
 * restores and shrinks MainDef on each content load, and recentred around
 * the upstream layout, 8px per item. When the first item changes the
 * cursor starts on it.
 */
static void dopo_main_menu_sync(void)
{
  const dbool playing = gamestate == GS_LEVEL && !demoplayback;
  menu_t *def = raven ? &RavenMainDef : &MainDef;
  menuitem_t *first = raven ? &RavenMainMenu[rv_first] : &MainMenu[mainfirst];
  /* item count and position upstream gave the same menu */
  const int upstream_count = raven ? 5 : 6;
  const int upstream_y = raven ? 56 : 64;
  const menuitem_t item = playing
    ? (menuitem_t){1, "", dopo_switch_weapon, 'w', raven ? "switch weapon" : "Switch Weapon"}
    : (menuitem_t){1, "", dopo_main_new_game, 'n', raven ? "new game" : "New Game"};

  if (first->routine != item.routine)
    def->lastOn = 0;
  *first = item;

  def->menuitems = raven ? RavenMainMenu : MainMenu;
  def->numitems = raven ? rv_main_end : main_end;
  def->y = upstream_y + (upstream_count - def->numitems) * 8;
  if (def->lastOn >= def->numitems)
    def->lastOn = def->numitems - 1;
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
