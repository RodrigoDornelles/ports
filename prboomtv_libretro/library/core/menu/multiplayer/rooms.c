/**
 * @brief Multiplayer outside a session, when the frontend hosts and joins
 * rooms for the core (prboomtv/rooms.h): Create Lobby and Game List.
 *
 * Game List is a server list: a full screen table, in the small font, of
 * the lobby's PrBoomTV rooms (name, game, server, players), scrolling with the
 * selection; a room that cannot be joined is gray, and the footer says
 * why. REFRESH, on top, asks the lobby again.
 */

/**
 * @brief Create Lobby and Game List menus.
 *
 * @patch src/m_menu.c 5589
 */
#include "prboomtv/menu.h"
#include "prboomtv/rooms.h"


/* ------------------------------------------------------------------ */
/* Multiplayer                                                         */
/* ------------------------------------------------------------------ */

static void dopo_rooms_root_draw(void);
static void dopo_create_open(int choice);
static void dopo_list_open(int choice);

static menuitem_t dopo_rooms_root_items[] =
{
  {1, "", dopo_create_open, 'c', "Create Lobby"},
  {1, "", dopo_list_open,   'g', "Game List"},
};

static menu_t dopo_rooms_root_def =
{
  2,
  NULL,
  dopo_rooms_root_items,
  dopo_rooms_root_draw,
  DOPO_MENU_SMALL_X, DOPO_MENU_SMALL_Y,
  0
};

static void dopo_rooms_root_draw(void)
{
  dopo_menu_small(&dopo_rooms_root_def);
  dopo_menu_title("MULTIPLAYER");
}

void dopo_rooms_menu_open(void)
{
  dopo_rooms_root_def.prevMenu = M_MainMenuDef();
  dopo_menu_open(&dopo_rooms_root_def);
}

/* ------------------------------------------------------------------ */
/* Create Lobby                                                        */
/* ------------------------------------------------------------------ */

/** @brief Relays to host through: none, then the libretro ones. */
static const char *const dopo_relay_handles[] = { NULL, "saopaulo", "nyc", "madrid", "singapore" };
static const char *const dopo_relay_names[] = { "OFF", "SAO PAULO", "NEW YORK", "MADRID", "SINGAPORE" };

static dbool dopo_create_listed = TRUE;
static int   dopo_create_relay = 1;  /* Sao Paulo */

static void dopo_create_toggle_listed(int choice)
{
  dopo_create_listed = !dopo_create_listed;
}

static void dopo_create_cycle_relay(int choice)
{
  static const int steps[] = { 0, 1, 2, 3, 4 };

  dopo_menu_cycle(&dopo_create_relay, steps, sizeof(steps) / sizeof(*steps), choice);
}

/** @brief Hosts the room; the lobby opens once the session starts. */
static void dopo_create_go(int choice)
{
  dopo_rooms_create(dopo_create_listed, dopo_relay_handles[dopo_create_relay]);
  M_ClearMenus();
}

static void dopo_create_draw(void);

static menuitem_t dopo_create_items[] =
{
  {1, "", dopo_create_toggle_listed, 'p', "Public"},
  {2, "", dopo_create_cycle_relay,   'r', "Relay"},
  {1, "", dopo_create_go,            'c', "Create"},
};

static menu_t dopo_create_def =
{
  3,
  &dopo_rooms_root_def,
  dopo_create_items,
  dopo_create_draw,
  DOPO_MENU_LEFT, DOPO_MENU_TOP,
  0
};

static void dopo_create_draw(void)
{
  static const dbool *const values[] = { &dopo_create_listed };

  dopo_menu_big(&dopo_create_def);
  dopo_menu_title("CREATE LOBBY");
  dopo_menu_toggles(&dopo_create_def, values, 1);
  dopo_menu_value(&dopo_create_def, 1, dopo_relay_names[dopo_create_relay], CR_GOLD);
  dopo_menu_small_centered(dopo_create_def.y + LINEHEIGHT * 3 + 8,
                           dopo_create_listed ? "LISTED IN THE LOBBY" : "NOT LISTED: SHARE THE RELAY SESSION",
                           CR_GRAY);
  if (!dopo_create_relay)
    dopo_menu_small_centered(dopo_create_def.y + LINEHEIGHT * 3 + 8 + DOPO_MENU_SMALL_LINE,
                             "WITHOUT RELAY THE PORT MUST BE OPEN", CR_GRAY);
}

static void dopo_create_open(int choice)
{
  dopo_menu_open(&dopo_create_def);
}

/* ------------------------------------------------------------------ */
/* Game List                                                           */
/* ------------------------------------------------------------------ */

/* columns, on the whole screen: name, game and server from the left,
 * players right aligned */
#define DOPO_LIST_NAME_X     14
#define DOPO_LIST_GAME_X     110
#define DOPO_LIST_SERVER_X   190
#define DOPO_LIST_PLAYERS_R  308
#define DOPO_LIST_HEAD_Y     34
#define DOPO_LIST_ROW_Y      46
#define DOPO_LIST_ROW        (DOPO_MENU_SMALL_LINE + 1)
#define DOPO_LIST_ROWS       12
#define DOPO_LIST_FOOT_Y     (DOPO_LIST_ROW_Y + DOPO_LIST_ROWS * DOPO_LIST_ROW + 6)

static unsigned dopo_list_gen;  /* the list the items were built from */
static int      dopo_list_top;  /* first room on screen */
static menuitem_t dopo_list_items[DOPO_ROOMS_MAX + 1];

static void dopo_list_draw(void);
static void dopo_list_pick(int choice);
static void dopo_list_refresh(int choice);

/* item 0 is REFRESH, then a row per room */
menu_t dopo_list_def =
{
  0,
  &dopo_rooms_root_def,
  dopo_list_items,
  dopo_list_draw,
  DOPO_LIST_NAME_X, DOPO_LIST_ROW_Y,
  0
};

/** @brief The rows: REFRESH, then a room each; the selection stays. */
static void dopo_list_fill(void)
{
  const int total = dopo_rooms_count();
  int i;

  dopo_list_items[0] = (menuitem_t){ 1, "", dopo_list_refresh, 'r', NULL };
  for (i = 0; i < total; i++)
    dopo_list_items[i + 1] = (menuitem_t){ 1, "", dopo_list_pick, 0, NULL };
  dopo_list_def.numitems = total + 1;
  if (dopo_list_def.lastOn > total)
    dopo_list_def.lastOn = total;
  if (currentMenu == &dopo_list_def && itemOn > total)
    itemOn = total;
  dopo_list_gen = dopo_rooms_generation();
}

/** @brief What keeps a room out, as the footer says it. */
static const char *dopo_list_block(const dopo_room_t *room)
{
  switch (room->block)
  {
    case DOPO_ROOM_NO_GAME:       return "YOU DON'T HAVE THIS GAME";
    case DOPO_ROOM_OTHER_VERSION: return "ANOTHER PRBOOMTV VERSION";
    case DOPO_ROOM_PASSWORD:      return "ROOM WITH PASSWORD";
    default:                      return NULL;
  }
}

/** @brief Text cut to a width in the small font. */
static void dopo_list_fit(char *buf, size_t size, const char *text, int width)
{
  size_t n;

  snprintf(buf, size, "%s", text);
  for (n = strlen(buf); n > 0 && M_StringWidth(buf) > width; n--)
    buf[n - 1] = 0;
}

static void dopo_list_right(int x, int y, const char *text, int cm)
{
  M_WriteText(x - M_StringWidth(text), y, text, cm);
}

/**
 * @brief The whole screen: title, REFRESH, the column heads, the rooms in
 * view, and the footer (the list's state, or why the selected room
 * cannot be joined).
 */
static void dopo_list_draw(void)
{
  const dopo_rooms_state_t state = dopo_rooms_poll();
  const int total = dopo_rooms_count();
  const int selected = itemOn - 1;  /* room, -1 on REFRESH */
  char text[64];
  int row;

  if (dopo_rooms_generation() != dopo_list_gen)
    dopo_list_fill();

  menuactive = mnact_full;
  M_DrawBackground(g_menu_flat, 0);
  dopo_text_centered(8, "GAME LIST", CR_DEFAULT);

  dopo_list_right(DOPO_LIST_PLAYERS_R, 14, itemOn == 0 ? "> REFRESH" : "REFRESH",
                  itemOn == 0 ? CR_GOLD : CR_GRAY);

  M_WriteText(DOPO_LIST_NAME_X, DOPO_LIST_HEAD_Y, "NAME", CR_GOLD);
  M_WriteText(DOPO_LIST_GAME_X, DOPO_LIST_HEAD_Y, "GAME", CR_GOLD);
  M_WriteText(DOPO_LIST_SERVER_X, DOPO_LIST_HEAD_Y, "SERVER", CR_GOLD);
  dopo_list_right(DOPO_LIST_PLAYERS_R, DOPO_LIST_HEAD_Y, "PLAYERS", CR_GOLD);

  /* the view follows the selection */
  if (selected >= 0 && selected < dopo_list_top)
    dopo_list_top = selected;
  else if (selected >= dopo_list_top + DOPO_LIST_ROWS)
    dopo_list_top = selected - DOPO_LIST_ROWS + 1;
  if (dopo_list_top > total - DOPO_LIST_ROWS)
    dopo_list_top = total > DOPO_LIST_ROWS ? total - DOPO_LIST_ROWS : 0;

  for (row = 0; row < DOPO_LIST_ROWS && dopo_list_top + row < total; row++)
  {
    const int i = dopo_list_top + row;
    const dopo_room_t *room = dopo_rooms_get(i);
    const int y = DOPO_LIST_ROW_Y + row * DOPO_LIST_ROW;
    const int cm = i == selected ? CR_GOLD : room->block == DOPO_ROOM_OK ? CR_DEFAULT : CR_GRAY;

    if (i == selected)
      M_WriteText(DOPO_LIST_NAME_X - 10, y, ">", CR_GOLD);
    dopo_list_fit(text, sizeof(text), room->nick, DOPO_LIST_GAME_X - DOPO_LIST_NAME_X - 6);
    M_WriteText(DOPO_LIST_NAME_X, y, text, cm);
    dopo_list_fit(text, sizeof(text), room->game, DOPO_LIST_SERVER_X - DOPO_LIST_GAME_X - 6);
    M_WriteText(DOPO_LIST_GAME_X, y, text, cm);
    dopo_list_fit(text, sizeof(text), room->server, DOPO_LIST_PLAYERS_R - DOPO_LIST_SERVER_X - 24);
    M_WriteText(DOPO_LIST_SERVER_X, y, text, cm);
    snprintf(text, sizeof(text), "%d/%d", room->players, MAXPLAYERS);
    dopo_list_right(DOPO_LIST_PLAYERS_R, y, text, cm);
  }

  if (state == DOPO_ROOMS_LOADING)
    dopo_menu_small_centered(DOPO_LIST_FOOT_Y, "LOOKING FOR ROOMS...", CR_GRAY);
  else if (state == DOPO_ROOMS_FAILED)
    dopo_menu_small_centered(DOPO_LIST_FOOT_Y, "THE LOBBY CAN'T BE REACHED", CR_RED);
  else if (!total)
    dopo_menu_small_centered(DOPO_LIST_FOOT_Y, "NO ROOMS RIGHT NOW", CR_GRAY);
  else if (selected >= 0 && dopo_list_block(dopo_rooms_get(selected)))
    dopo_menu_small_centered(DOPO_LIST_FOOT_Y, dopo_list_block(dopo_rooms_get(selected)), CR_GRAY);
  else
  {
    snprintf(text, sizeof(text), "%d ROOM%s", total, total == 1 ? "" : "S");
    dopo_menu_small_centered(DOPO_LIST_FOOT_Y, text, CR_GRAY);
  }
}

/** @brief Joins a room (switching to its game first); the lobby opens
 * once the session starts. */
static void dopo_list_pick(int choice)
{
  const int i = choice - 1;
  const dopo_room_t *room = dopo_rooms_get(i);
  static char message[96];

  if (!room)
    return;
  if (!dopo_rooms_join(i))
  {
    snprintf(message, sizeof(message), "%s.\n\nPress a key.",
             dopo_list_block(room) ? dopo_list_block(room) : "This room can't be joined");
    M_StartMessage(message, NULL, FALSE);
    return;
  }
  M_ClearMenus();
}

static void dopo_list_refresh(int choice)
{
  dopo_rooms_refresh();
}

/** @brief Opens the list, asking the lobby for the rooms. */
static void dopo_list_open(int choice)
{
  dopo_list_top = 0;
  dopo_rooms_refresh();
  dopo_list_fill();
  dopo_list_def.lastOn = 0;
  M_SetupNextMenu(&dopo_list_def);
}

/* @endpatch */
