/**
 * @brief Delete Game: the save slots, where the Load Game screen has
 * them, and a Delete/Cancel confirmation that works with a single
 * confirm button.
 */

/**
 * @brief Delete Game menus.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"

static int dopo_delete_slot;

static void dopo_delete_draw(void);
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

static menu_t dopo_delete_def =
{
  load_end,
  &dopo_singleplayer_def,
  dopo_delete_items,
  dopo_delete_draw,
  0, 0,
  0
};

/**
 * @brief Title and the slots, at the Load Game screen's position.
 */
static void dopo_delete_draw(void)
{
  int i;

  dopo_delete_def.x = LoadDef.x;
  dopo_delete_def.y = LoadDef.y;
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

void dopo_delete_open(int choice)
{
  dopo_delete_refresh();
  dopo_menu_open(&dopo_delete_def);
}

static void dopo_delete_confirm_draw(void);
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
  dopo_delete_confirm_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  1  /* start on Cancel */
};

/**
 * @brief The confirmation's title and the save's name.
 */
static void dopo_delete_confirm_draw(void)
{
  dopo_menu_small(&dopo_delete_confirm_def);
  dopo_menu_title("DELETE GAME");
  dopo_menu_small_centered(40, savegamestrings[dopo_delete_slot], CR_GOLD);
}

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

  dopo_delete_open(0);
}

static void dopo_delete_cancel(int choice)
{
  M_SetupNextMenu(&dopo_delete_def);
}

/* @endpatch */
