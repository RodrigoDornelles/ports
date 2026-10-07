/**
 * @brief Rooms: hosting and joining netplay rooms from the core's own
 * menus (Multiplayer: Create Lobby, Game List), when the frontend speaks
 * the gecnd netplay calls (dopo/gecnd_netplay.h); RetroArch does not, so
 * there the rooms stay in its Netplay menu.
 *
 * The room list is the libretro lobby's, read with jsmn: PrBoomTV rooms of
 * every version, each with its host's name, game, version and players. A
 * room can be joined when it runs this version and its game is one of
 * the games next to the running one (Change Game): the core switches to
 * that game first, then joins.
 */
#ifndef DOPO_ROOMS_H
#define DOPO_ROOMS_H

#include <stdint.h>
#include "doomtype.h"

/** @brief Rooms kept from the list, at most. */
#define DOPO_ROOMS_MAX 64

/** @brief Where the room list is. */
typedef enum
{
  DOPO_ROOMS_NONE,    /* never asked for */
  DOPO_ROOMS_LOADING,
  DOPO_ROOMS_READY,
  DOPO_ROOMS_FAILED
} dopo_rooms_state_t;

/** @brief Why a room cannot be joined. */
typedef enum
{
  DOPO_ROOM_OK,
  DOPO_ROOM_NO_GAME,       /* its game is not next to ours */
  DOPO_ROOM_OTHER_VERSION, /* another PrBoomTV version */
  DOPO_ROOM_PASSWORD       /* rooms with a password: not yet */
} dopo_room_block_t;

/** @brief A room of the list. */
typedef struct
{
  char     nick[32];
  char     game[64];
  char     version[24];
  int      players;
  char     host[64];      /* where to connect: the host or its relay */
  uint16_t port;
  char     session[24];   /* relay session, "" when straight */
  char     server[16];    /* the relay's region, or the host's country */
  int      game_index;    /* in the Change Game list, -1 when not there */
  dopo_room_block_t block;
} dopo_room_t;

/**
 * @brief The frontend's environment call (libretro.c, through
 * multiplayer/interface/gecnd/environment.c).
 */
dbool dopo_environment(unsigned cmd, void *data);

/** @brief Whether the frontend hosts and joins rooms for the core. */
dbool dopo_rooms_available(void);

/** @brief Asks the lobby for the rooms again. */
void dopo_rooms_refresh(void);

/** @brief Where the list is; reads it once it arrives. */
dopo_rooms_state_t dopo_rooms_poll(void);

/** @brief Changes every time a new list is read. */
unsigned dopo_rooms_generation(void);

int dopo_rooms_count(void);
const dopo_room_t *dopo_rooms_get(int i);

/**
 * @brief Hosts a room for the running game: listed in the lobby or not,
 * through a relay (its handle) or straight (NULL).
 */
void dopo_rooms_create(dbool listed, const char *relay);

/**
 * @brief Joins a room, switching to its game first when it is another;
 * FALSE when the room cannot be joined (its block says why).
 */
dbool dopo_rooms_join(int i);

/** @brief Leaves the session, hosted or joined. */
void dopo_rooms_leave(void);

/** @brief Every frame, before the game runs (dopo/change_game.c). */
void dopo_rooms_frame(void);

#endif
