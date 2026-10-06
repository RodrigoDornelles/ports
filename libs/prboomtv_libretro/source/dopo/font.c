/**
 * @brief Big menu font with a runtime color.
 *
 * Doom draws text with the Odamex big font embedded as DOPOFnnn lumps
 * (see scripts/prboomtv_font.cpp); Heretic and Hexen keep their own FONTB.
 * Glyphs sit on the red ramp, so any CR_* color translation applies.
 */

/**
 * @brief Text helpers for the big font.
 *
 * @patch src/m_menu.c 359
 */
#include "dopo_font_data.h"

/**
 * @brief Lump of a big font glyph, or -1 when the font lacks the character
 * (space included). The font is uppercase only.
 */
static int dopo_font_glyph(char c)
{
  char name[9];

  c = (char)toupper((unsigned char)c);
  if (c < DOPO_FONT_FIRST || c > DOPO_FONT_LAST)
    return -1;
  snprintf(name, sizeof(name), "DOPOF%03d", c);
  return W_CheckNumForName(name);
}

/**
 * @brief Lays text out like Heretic's FONTB: spaces advance DOPO_FONT_SPACE
 * and glyphs overlap by DOPO_FONT_OVERLAP. Draws when draw is set and
 * returns the width in pixels.
 */
static int dopo_font_layout(int x0, int y, const char *text, int cm, dbool draw)
{
  int x = 0;

  for (; *text; text++)
  {
    const int lump = dopo_font_glyph(*text);

    if (lump < 0)
    {
      x += DOPO_FONT_SPACE;
      continue;
    }
    if (draw)
      V_DrawNumPatch(x0 + x, y, 0, lump, cm, VPT_STRETCH | VPT_TRANS);
    x += R_NumPatchWidth(lump) - DOPO_FONT_OVERLAP;
  }
  return x + DOPO_FONT_OVERLAP;
}

/**
 * @brief Width of text in the big font of the running game.
 */
static int dopo_text_width(const char *text)
{
  return raven ? M_TextBWidth(text) : dopo_font_layout(0, 0, text, CR_DEFAULT, FALSE);
}

/**
 * @brief Draws text in the big font of the running game, in color cm.
 */
static void dopo_text(int x, int y, const char *text, int cm)
{
  if (raven)
    M_DrawTextBColor(x, y, text, cm);
  else
    dopo_font_layout(x, y, text, cm, TRUE);
}

/**
 * @brief Draws text in the big font centered on the screen.
 */
static void dopo_text_centered(int y, const char *text, int cm)
{
  dopo_text(160 - dopo_text_width(text) / 2, y, text, cm);
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
