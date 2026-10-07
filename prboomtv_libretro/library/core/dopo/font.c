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
