/**
 * @brief Menu layout shared by every Dopo menu, and the menu drawer.
 *
 * See include/prboomtv/menu.h for the small and big menu anchors.
 */

/**
 * @brief Layout helpers.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

/**
 * @brief x of the small menu column: the widest item the main menu can
 * have (in the running game's font) centered under the logo.
 */
static int dopo_menu_small_x(void)
{
  static const char *const labels[] =
  {
    "Change Weapon", "Singleplayer", "Multiplayer", "Scoreboard", "Settings", "Exit",
  };
  static const char *const raven_labels[] =
  {
    "change weapon", "singleplayer", "multiplayer", "scoreboard", "settings", "exit",
  };
  size_t i;
  int width = 0;

  for (i = 0; i < sizeof(labels) / sizeof(*labels); i++)
  {
    const int w = dopo_text_width(raven ? raven_labels[i] : labels[i]);

    if (w > width)
      width = w;
  }
  return 160 - width / 2;
}

void dopo_menu_small(menu_t *menu)
{
  menu->x = dopo_menu_small_x();
  menu->y = DOPO_MENU_SMALL_Y;
}

void dopo_menu_big(menu_t *menu)
{
  menu->x = DOPO_MENU_LEFT;
  menu->y = DOPO_MENU_TOP;
}

void dopo_menu_open(menu_t *menu)
{
  menu->lastOn = 0;
  M_SetupNextMenu(menu);
}

void dopo_menu_title(const char *title)
{
  dopo_text(dopo_menu_small_x(), DOPO_MENU_TITLE_Y, title, CR_DEFAULT);
}

void dopo_menu_value(const menu_t *menu, int item, const char *text, int cm)
{
  dopo_text(DOPO_MENU_RIGHT - dopo_text_width(text),
            menu->y + LINEHEIGHT * item, text, cm);
}

void dopo_menu_toggles(const menu_t *menu, const dbool *const values[], int count)
{
  int i;

  for (i = 0; i < count; i++)
    dopo_menu_value(menu, i, *values[i] ? "ON" : "OFF", CR_GOLD);
}

void dopo_menu_small_centered(int y, const char *text, int cm)
{
  M_WriteText(160 - M_StringWidth(text) / 2, y, text, cm);
}

void dopo_menu_cycle(int *value, const int *steps, int count, int choice)
{
  int i = 0;

  while (i < count - 1 && steps[i] != *value)
    i++;
  *value = steps[(i + (choice ? 1 : count - 1)) % count];
}

void dopo_menu_format(char *buf, size_t size, int value, const char *unit)
{
  if (value)
    snprintf(buf, size, "%d%s", value, unit);
  else
    snprintf(buf, size, "OFF");
}

dbool dopo_menu_netgame_blocked(void)
{
  if (!netgame && dopo_mp_state() == DOPO_MP_OFF)
    return FALSE;
  M_StartMessage(DOPO_MP_NETGAME_BLOCKED, NULL, FALSE);
  return TRUE;
}

/* @endpatch */

/**
 * @brief Menu drawer whose text path uses the big font of the game.
 *
 * @patch src/m_menu.c 5694-5800
 */
void M_Drawer (void)
{
  inhelpscreens = FALSE;

  // Horiz. & Vertically center string and print it.
  // killough 9/29/98: simplified code, removed 40-character width limit
  if (messageToPrint)
    {
      /* The message text is constant while displayed, and M_Drawer runs
       * every frame, so walk it line by line without copying it to a
       * writable buffer -- M_WriteText only needs a start pointer and a
       * length-bounded view, so no per-frame strdup/free is needed. */
      const char *p = messageString;
      int y = 100 - M_StringHeight(messageString)/2;

      while (*p)
      {
        const char *line = p;
        char        tmp[128];
        int         len  = 0;

        while (p[len] && p[len] != '\n')
          len++;

        if (len > (int)sizeof(tmp) - 1)
          len = (int)sizeof(tmp) - 1;
        memcpy(tmp, line, len);
        tmp[len] = 0;

        M_WriteText(160 - M_StringWidth(tmp)/2, y, tmp, CR_DEFAULT);
        y += hu_font[0].height;

        p += len;
        if (*p == '\n')
          p++;
      }
    }
  else
    if (menuactive)
      {
  int x,y,max,i;
  int lumps_missing = 0;

  menuactive = mnact_float; // Boom-style menu drawers will set mnact_full

  if (currentMenu->routine)
    currentMenu->routine();     // call Draw routine

  // DRAW MENU

  x = currentMenu->x;
  y = currentMenu->y;
  max = currentMenu->numitems;

  /* An item that has a text label (alttext) but no drawable patch -- either
   * an empty patch name or a name whose lump is absent -- forces the whole
   * menu onto the text path.  ZDoom episodes/skills defined with `name` but
   * no `picname` (e.g. ZDCMP2's `episode ZDCMP2 { name = "ZDCMP2" }`) reach
   * M_AddEpisode with an empty patch name; without this they were neither
   * counted as missing nor drawn as a patch, so the entry rendered blank. */
  for (i = 0; i < max; i++)
    if (currentMenu->menuitems[i].alttext &&
        (!currentMenu->menuitems[i].name[0] ||
         W_CheckNumForName(currentMenu->menuitems[i].name) < 0))
      lumps_missing++;

  if (lumps_missing == 0)
    for (i=0;i<max;i++)
    {
      if (currentMenu->menuitems[i].name[0])
        V_DrawNamePatch(x,y,0,currentMenu->menuitems[i].name,
            CR_DEFAULT, VPT_STRETCH);
      y += LINEHEIGHT;
    }
  else
    for (i = 0; i < max; i++)
    {
      const char *alttext = currentMenu->menuitems[i].alttext;
      if (alttext)
        dopo_text(x, y, alttext, CR_DEFAULT);  /* FONTB or Odamex */
      y += LINEHEIGHT;
    }

  // DRAW SKULL

  /* some menus (Scoreboard) mark their own selection */
  if (dopo_menu_draws_cursor(currentMenu))
    return;

  // CPhipps - patch drawing updated
  /* The Doom skull cursor lumps (M_SKULL1/2) do not exist in Heretic or
   * Hexen, which use the blinking arrow selector M_SLCTR1/2 instead.  Pick
   * the right cursor for the game; whichSkull already provides the blink
   * phase. */
  if (raven)
    {
      const char *selName = whichSkull ? "M_SLCTR1" : "M_SLCTR2";
      if (W_CheckNumForName(selName) >= 0)
        V_DrawNamePatch(x - 28, currentMenu->y - 1 + itemOn*LINEHEIGHT, 0,
            selName, CR_DEFAULT, VPT_STRETCH);
    }
  else if (W_CheckNumForName(skullName[whichSkull]) >= 0)
    V_DrawNamePatch(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,0,
        skullName[whichSkull], CR_DEFAULT, VPT_STRETCH);
      }
}
/* @endpatch */
