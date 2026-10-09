/**
 * @file protocol.h
 * @brief The Dornelles Ports multiplayer protocol: what every core's
 * session sends between machines (packet types, client ids, limits),
 * whatever the game and whatever carries the packets.
 *
 * Topology: a star. Clients only talk to the host, which relays what
 * every player sends to the others.
 */
#ifndef DOPO_PROTOCOL_H
#define DOPO_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

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

/** @brief Destination of a packet for every other machine. */
#define DOPO_MP_ALL 0xFFFF

/** @brief Client id of the host. */
#define DOPO_MP_HOST 0

/**
 * @brief Sends a packet to a client id (the host is DOPO_MP_HOST) or to
 * DOPO_MP_ALL; reliable and in order.
 */
typedef void (*dopo_mp_send_t)(uint16_t to, const void *buf, size_t len);

#endif
