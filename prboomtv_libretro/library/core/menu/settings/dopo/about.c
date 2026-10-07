/**
 * @brief The Dopo Read This!: a full screen page with the port, its legal
 * notes and where its source code is.
 */

/**
 * @brief Read This! page.
 *
 * @patch src/m_menu.c 5589
 */
#include "dopo/menu.h"

#define DOPO_ABOUT_X 10

/**
 * @brief Body, one entry per screen line, drawn in the small font as is:
 * no wrapping, so each line must fit the screen (~300px from
 * DOPO_ABOUT_X). A line starting with '#' is a heading, drawn in gold
 * without the '#'; an empty string leaves a blank line.
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

static void dopo_about_draw(void)
{
  size_t i;
  int y = 42;

  menuactive = mnact_full;
  M_DrawBackground(g_menu_flat, 0);

  dopo_text_centered(8, "Rodrigo Dornelles Ports", CR_DEFAULT);
  dopo_menu_small_centered(26, "PrBoomTV, a Doom engine core for libretro", CR_GOLD);

  for (i = 0; i < sizeof(dopo_about_lines) / sizeof(*dopo_about_lines); i++, y += DOPO_MENU_SMALL_LINE)
  {
    const char *line = dopo_about_lines[i];

    if (line[0] == '#')
      M_WriteText(DOPO_ABOUT_X, y, line + 1, CR_GOLD);
    else
      M_WriteText(DOPO_ABOUT_X, y, line, CR_GRAY);
  }
}

/**
 * @brief Any confirm goes back to Dopo Options.
 */
static void dopo_about_close(int choice)
{
  M_SetupNextMenu(&dopo_dopo_options_def);
}

static menuitem_t dopo_about_items[] =
{
  {1, "", dopo_about_close, 0, NULL},
};

static menu_t dopo_about_def =
{
  1,
  &dopo_dopo_options_def,
  dopo_about_items,
  dopo_about_draw,
  360,175,  /* skull and Raven arrow both land off screen */
  0
};

void dopo_about_open(int choice)
{
  dopo_menu_open(&dopo_about_def);
}

/* @endpatch */
