/**
 * @brief Multiplayer: Doom's own lockstep netgame, carried by a transport
 * (the RetroArch netpacket interface first, peer to peer later).
 *
 * Every machine runs the whole game from the same ticcmds, so anything
 * that changes the game goes either in a player's ticcmd or in a packet
 * stamped with the tic it applies to; what a machine only shows (view,
 * HUD, sounds) stays local.
 *
 * Tics are counted from the start of the game (tic 0), the same on every
 * machine, whatever gametic each one had before.
 *
 * Topology: a star. Clients only talk to the host, which relays the
 * ticcmds of each player to the others.
 */
#ifndef DOPO_MULTIPLAYER_H
#define DOPO_MULTIPLAYER_H

#include <stddef.h>
#include <stdint.h>
#include "doomtype.h"
#include "d_ticcmd.h"

/**
 * @brief Packet types. Every packet starts with its type (one byte); the
 * ones that belong to a tic follow it with the tic (four bytes, big
 * endian), and every machine applies them right before running that tic.
 */
typedef enum
{
  DOPO_MP_HELLO = 1, /* client to host: name and core version */
  DOPO_MP_ROSTER,    /* host to all: slots, client ids and names */
  DOPO_MP_CONFIG,    /* host to all: mode, skill and level being set up */
  DOPO_MP_START,     /* host to all: the game starts, with its options */
  DOPO_MP_TIC,       /* a player's ticcmd for a tic */
  DOPO_MP_CHEATS,    /* host to all: cheats from a tic on */
  DOPO_MP_LEAVE,     /* host to all: a player is gone from a tic on */
  DOPO_MP_ADMIN,     /* admin to host: an action on a player (and target) */
  DOPO_MP_KICK,      /* host to a client: leave (the core exits) */
  DOPO_MP_KILL,      /* host to all: a player dies at a tic */
  DOPO_MP_LEVEL,     /* host to all: mode, skill and level change at a tic */
  DOPO_MP_PING,      /* host to a client, echoed back: round trip time */
  DOPO_MP_PINGS,     /* host to all: every player's ping */
  DOPO_MP_TELEPORT,  /* host to all: a player goes next to another at a tic */
  DOPO_MP_REJECT     /* host to a client: other core version (the host's) */
} dopo_mp_packet_t;

/** @brief Longest core version in a packet, without the terminator. */
#define DOPO_MP_VERSION 23

/** @brief Admin actions on a player (DOPO_MP_ADMIN). */
typedef enum
{
  DOPO_MP_ADMIN_TOGGLE = 1, /* promote to admin, or remove admin */
  DOPO_MP_ADMIN_KICK,
  DOPO_MP_ADMIN_KILL,
  DOPO_MP_ADMIN_TELEPORT  /* the player goes next to the target player */
} dopo_mp_admin_t;

/** @brief Longest player name, without the terminator. */
#define DOPO_MP_NAME 15

/** @brief Ticcmds a machine may build ahead of the tic it runs. */
#define DOPO_MP_LEAD 4

/** @brief Destination of a packet for every other machine. */
#define DOPO_MP_ALL 0xFFFF

/** @brief Client id of the host. */
#define DOPO_MP_HOST 0

/**
 * @brief Message shown when the menu refuses something a netgame cannot
 * do.
 */
#define DOPO_MP_NETGAME_BLOCKED \
  "You can't do that in a netgame.\n\nPress a key."

/**
 * @brief Message shown when someone other than the host tries to change
 * what the host controls.
 */
#define DOPO_MP_HOST_ONLY \
  "Only the host can change this.\n\nPress a key."

/**
 * @brief Message shown when someone who is not an admin picks an admin
 * action.
 */
#define DOPO_MP_ADMIN_ONLY \
  "Only an admin can do this.\n\nPress a key."

/**
 * @brief Message shown by Multiplayer when there is no session.
 */
#define DOPO_MP_NO_SESSION \
  "Host or join a room from the\nNetplay menu of RetroArch.\n\nPress a key."

/**
 * @brief Where the session is: no session, the waiting room (lobby), or a
 * game being played.
 */
typedef enum
{
  DOPO_MP_OFF,
  DOPO_MP_LOBBY,
  DOPO_MP_GAME
} dopo_mp_state_t;

/** @brief Game modes, as deathmatch holds them. */
enum
{
  DOPO_MP_COOP,
  DOPO_MP_DEATHMATCH,
  DOPO_MP_ALTDEATH
};

/**
 * @brief What the host sets up in the lobby.
 */
typedef struct
{
  int mode;    /* DOPO_MP_COOP, DOPO_MP_DEATHMATCH or DOPO_MP_ALTDEATH */
  int skill;   /* skill_t */
  int episode; /* 1 on games without episodes */
  int map;
} dopo_mp_config_t;

/**
 * @brief A player slot (player 1 to 4); slot 0 is always the host.
 */
typedef struct
{
  dbool    used;
  dbool    admin;  /* the host always is */
  uint16_t client;
  int      ping;   /* round trip to the host in ms, -1 unknown */
  char     name[DOPO_MP_NAME + 1];
} dopo_mp_slot_t;

/* ------------------------------------------------------------------ */
/* Transport: implemented by multiplayer/interface/<transport>/        */
/* ------------------------------------------------------------------ */

/**
 * @brief Sends a packet to a client id (the host is DOPO_MP_HOST) or to
 * DOPO_MP_ALL; reliable and in order.
 */
typedef void (*dopo_mp_send_t)(uint16_t to, const void *buf, size_t len);

/**
 * @brief A session started: this machine is the host (self is
 * DOPO_MP_HOST) or a client already connected to it.
 */
void dopo_mp_on_start(uint16_t self, const char *name, dopo_mp_send_t send);

/**
 * @brief Host only: a client connected; returns FALSE to drop it.
 */
dbool dopo_mp_on_connected(uint16_t client);

/** @brief Host only: a client left. */
void dopo_mp_on_disconnected(uint16_t client);

/** @brief A packet arrived from a client id (DOPO_MP_HOST for the host). */
void dopo_mp_on_receive(const void *buf, size_t len, uint16_t from);

/** @brief The session ended. */
void dopo_mp_on_stop(void);

/** @brief Once per frame, between frames. */
void dopo_mp_on_poll(void);

/* ------------------------------------------------------------------ */
/* Session (multiplayer/session.c)                                     */
/* ------------------------------------------------------------------ */

/** @brief Where the session is. */
dopo_mp_state_t dopo_mp_state(void);

/**
 * @brief Whether this machine controls the game: always outside a
 * session, only the host inside one.
 */
dbool dopo_mp_is_host(void);

/** @brief The player slots, for the lobby. */
const dopo_mp_slot_t *dopo_mp_slots(void);

/** @brief This machine's slot, or -1 before the host gave one. */
int dopo_mp_self_slot(void);

/**
 * @brief Whether this machine may use the admin actions: always outside
 * a session, the host and the players it promoted inside one.
 */
dbool dopo_mp_is_admin(void);

/**
 * @brief Asks for an admin action on a player (target: the other player
 * of DOPO_MP_ADMIN_TELEPORT, unused otherwise): done right away on the
 * host, sent to it by another admin.
 */
void dopo_mp_admin(dopo_mp_admin_t action, int slot, int target);

/**
 * @brief Deaths of a player since the netgame started, counted in the
 * game tics so every machine has the same.
 */
int dopo_mp_deaths(int slot);

/** @brief The lobby setup, as the host last sent it. */
const dopo_mp_config_t *dopo_mp_config(void);

/** @brief Host only: changes the lobby setup and sends it to everyone. */
void dopo_mp_set_config(const dopo_mp_config_t *config);

/**
 * @brief Host only: starts the game for everyone in the lobby, or during
 * a game changes the mode, skill and level for everyone at the next tic.
 */
void dopo_mp_start_game(void);

/**
 * @brief Runs in TryRunTics before anything else: starts or ends a game
 * waiting to, outside the tic loop.
 */
void dopo_mp_frame(void);

/**
 * @brief Sends this machine's ticcmd for a tic (the host also relays the
 * other players' ones).
 */
void dopo_mp_send_tic(int slot, int tic, const ticcmd_t *cmd);

/** @brief Host only: sends cheats stamped for a tic. */
void dopo_mp_send_cheats(int tic);

/**
 * @brief Runs right before every game tic: applies what was stamped for
 * this tic, then keeps the cheats up. tic is the game tic counted from
 * the start of the netgame, or -1 outside one.
 */
void dopo_mp_tic_begin(int tic);

/**
 * @brief The session's part of a game tic of a netgame: the events
 * stamped for it (kills, level changes) and the deaths count.
 */
void dopo_mp_session_tic(int tic);

/* ------------------------------------------------------------------ */
/* Actions inside a game tic (multiplayer/actions.c)                   */
/* ------------------------------------------------------------------ */

/** @brief Kills a player, god mode and invulnerability included. */
void dopo_mp_kill(int slot);

/**
 * @brief Teleports a player next to another, facing them, on a free spot
 * (no telefrag); nothing happens without one.
 */
void dopo_mp_teleport(int slot, int target);

/* ------------------------------------------------------------------ */
/* Lockstep (multiplayer/lockstep.c, in d_client.c)                    */
/* ------------------------------------------------------------------ */

/** @brief Forgets every ticcmd: a new game starts at tic 0. */
void dopo_mp_lockstep_reset(void);

/** @brief Starts counting tics of a game that begins now. */
void dopo_mp_lockstep_begin(void);

/** @brief Stores a player's ticcmd for a tic. */
void dopo_mp_lockstep_store(int slot, int tic, const ticcmd_t *cmd);

/**
 * @brief A player is gone from a tic on: nobody waits for their
 * ticcmds anymore.
 */
void dopo_mp_lockstep_leave(int slot, int tic);

/** @brief Next tic this machine builds a ticcmd for. */
int dopo_mp_lockstep_next_build(void);

/** @brief Next tic this machine runs. */
int dopo_mp_lockstep_next_run(void);

/** @brief Tics from a player this machine has, from 0 on, with no gap. */
int dopo_mp_lockstep_received(int slot);

/**
 * @brief Forgets the consistency checks of earlier games (g_game.c), so
 * every machine starts comparing from the same values.
 */
void dopo_mp_consistancy_reset(void);

/* ------------------------------------------------------------------ */
/* Lobby menu (menu/multiplayer/index.c, in m_menu.c)                  */
/* ------------------------------------------------------------------ */

/** @brief Opens the lobby menu (menu/multiplayer/index.c). */
void dopo_lobby_open(void);

#endif
