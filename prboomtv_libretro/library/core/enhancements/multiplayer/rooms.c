/**
 * @brief Rooms (prboomtv/rooms.h): the lobby's room list read with jsmn, and
 * the requests that host, join and leave a room, all through the dopo
 * netplay calls (dopo/netplay.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "lprintf.h"
#include "prboomtv/change_game.h"
#include "dopo/netplay.h"
#include "prboomtv/rooms.h"
#include "prboomtv/version.h"

#define JSMN_STATIC
#include "jsmn.h"

/** @brief The core's name in the lobby (retro_system_info.library_name). */
#define DOPO_ROOMS_CORE "PrBoomTV"

/** @brief host_method of a room hosted through a relay. */
#define DOPO_ROOMS_RELAYED 3

static dopo_room_t dopo_rooms[DOPO_ROOMS_MAX];
static int dopo_rooms_n;
static unsigned dopo_rooms_gen;
static dopo_rooms_state_t dopo_rooms_status;
static dbool dopo_rooms_read; /* the list that arrived was read */

/* a room joined once the switch to its game is done */
static dbool dopo_join_pending;
static dopo_room_t dopo_join_room;

/* ------------------------------------------------------------------ */
/* JSON                                                                */
/* ------------------------------------------------------------------ */

/** @brief The token after a token and everything inside it. */
static int dopo_json_skip(const jsmntok_t *t, int count, int i)
{
  int left = 1;

  while (left > 0 && i < count)
    left += t[i++].size - 1;
  return i;
}

/** @brief The value of a key in the object at obj, or -1. */
static int dopo_json_get(const char *js, const jsmntok_t *t, int count, int obj, const char *key)
{
  const size_t len = strlen(key);
  int i = obj + 1, k;

  if (obj < 0 || t[obj].type != JSMN_OBJECT)
    return -1;
  for (k = 0; k < t[obj].size && i + 1 < count; k++)
  {
    if (t[i].type == JSMN_STRING && (size_t)(t[i].end - t[i].start) == len &&
        !memcmp(js + t[i].start, key, len))
      return i + 1;
    i = dopo_json_skip(t, count, i + 1);
  }
  return -1;
}

/**
 * @brief A string or number value into buf, unescaped; characters out of
 * ASCII (which the game's fonts lack) become '?'.
 */
static void dopo_json_text(const char *js, const jsmntok_t *t, int count, int obj,
                           const char *key, char *buf, size_t size)
{
  const int v = dopo_json_get(js, t, count, obj, key);
  size_t n = 0;
  int i;

  buf[0] = 0;
  if (v < 0 || (t[v].type != JSMN_STRING && t[v].type != JSMN_PRIMITIVE))
    return;
  for (i = t[v].start; i < t[v].end && n + 1 < size; i++)
  {
    unsigned char c = (unsigned char)js[i];

    if (c == '\\' && i + 1 < t[v].end)
    {
      c = (unsigned char)js[++i];
      if (c == 'u')
      {
        i += 4;
        c = '?';
      }
      else if (c == 'n' || c == 't' || c == 'r')
        c = ' ';
    }
    else if (c >= 0x80)
    {
      while (i + 1 < t[v].end && ((unsigned char)js[i + 1] & 0xC0) == 0x80)
        i++; /* the rest of a UTF-8 character */
      c = '?';
    }
    buf[n++] = (char)c;
  }
  buf[n] = 0;
}

/** @brief Whether a value is true (or a number other than 0). */
static dbool dopo_json_true(const char *js, const jsmntok_t *t, int count, int obj, const char *key)
{
  const int v = dopo_json_get(js, t, count, obj, key);

  return v >= 0 && t[v].type == JSMN_PRIMITIVE &&
         (js[t[v].start] == 't' || (js[t[v].start] >= '1' && js[t[v].start] <= '9'));
}

static long dopo_json_number(const char *js, const jsmntok_t *t, int count, int obj, const char *key)
{
  char buf[24];

  dopo_json_text(js, t, count, obj, key, buf, sizeof(buf));
  return strtol(buf, NULL, 10);
}

/* ------------------------------------------------------------------ */
/* Room list                                                           */
/* ------------------------------------------------------------------ */

/** @brief Games in the Change Game list, scanned before a search. */
static int dopo_rooms_games;

/** @brief The Change Game list's index of a game named name, or -1. */
static int dopo_rooms_find_game(const char *name)
{
  int i;

  for (i = 0; i < dopo_rooms_games; i++)
  {
    const char *file = dopo_games_name(i);
    const char *dot = strrchr(file, '.');
    const size_t len = dot ? (size_t)(dot - file) : strlen(file);

    if (strlen(name) == len && !strncasecmp(file, name, len))
      return i;
  }
  return -1;
}

/**
 * @brief Where a room is played from: the region of its relay, by the
 * relay's address (southamerica-east1.relay.retroarch.com), or for a
 * room without one the host's country as the lobby has it.
 */
static void dopo_rooms_server(dopo_room_t *room, const char *country)
{
  static const struct { const char *prefix, *name; } regions[] =
  {
    { "southamerica-east1.", "SAO PAULO" },
    { "us-east1.",           "NEW YORK" },
    { "europe-west1.",       "MADRID" },
    { "asia-southeast1.",    "SINGAPORE" },
  };
  size_t i;

  if (!room->session[0])
  {
    snprintf(room->server, sizeof(room->server), "%s", country[0] ? country : "DIRECT");
    for (i = 0; room->server[i]; i++)
      if (room->server[i] >= 'a' && room->server[i] <= 'z')
        room->server[i] -= 'a' - 'A';
    return;
  }
  for (i = 0; i < sizeof(regions) / sizeof(*regions); i++)
    if (!strncasecmp(room->host, regions[i].prefix, strlen(regions[i].prefix)))
    {
      snprintf(room->server, sizeof(room->server), "%s", regions[i].name);
      return;
    }
  /* another relay: its first name */
  snprintf(room->server, sizeof(room->server), "%.*s", (int)strcspn(room->host, "."), room->host);
}

/** @brief A room of the lobby, from its "fields"; FALSE if not ours. */
static dbool dopo_rooms_take(const char *js, const jsmntok_t *t, int count, int fields,
                             dopo_room_t *room)
{
  char core[32];
  int port;

  dopo_json_text(js, t, count, fields, "core_name", core, sizeof(core));
  if (strcasecmp(core, DOPO_ROOMS_CORE))
    return FALSE;

  memset(room, 0, sizeof(*room));
  dopo_json_text(js, t, count, fields, "username", room->nick, sizeof(room->nick));
  dopo_json_text(js, t, count, fields, "game_name", room->game, sizeof(room->game));
  dopo_json_text(js, t, count, fields, "core_version", room->version, sizeof(room->version));
  room->players = (int)dopo_json_number(js, t, count, fields, "player_count");

  /* through its relay when it has one, else straight */
  dopo_json_text(js, t, count, fields, "mitm_session", room->session, sizeof(room->session));
  if (room->session[0] &&
      dopo_json_number(js, t, count, fields, "host_method") == DOPO_ROOMS_RELAYED)
  {
    dopo_json_text(js, t, count, fields, "mitm_ip", room->host, sizeof(room->host));
    port = (int)dopo_json_number(js, t, count, fields, "mitm_port");
  }
  else
  {
    room->session[0] = 0;
    dopo_json_text(js, t, count, fields, "ip", room->host, sizeof(room->host));
    port = (int)dopo_json_number(js, t, count, fields, "port");
  }
  room->port = (uint16_t)port;
  {
    char country[8];
    dopo_json_text(js, t, count, fields, "country", country, sizeof(country));
    dopo_rooms_server(room, country);
  }

  room->game_index = dopo_rooms_find_game(room->game);
  if (dopo_json_true(js, t, count, fields, "has_password"))
    room->block = DOPO_ROOM_PASSWORD;
  else if (strcmp(room->version, DOPO_VERSION))
    room->block = DOPO_ROOM_OTHER_VERSION;
  else if (room->game_index < 0)
    room->block = DOPO_ROOM_NO_GAME;
  else
    room->block = DOPO_ROOM_OK;
  return TRUE;
}

/** @brief Reads the lobby's list: an array of {"fields": {...}}. */
static void dopo_rooms_parse(const char *js, size_t size)
{
  jsmn_parser parser;
  jsmntok_t *t;
  int count, i, k;

  dopo_rooms_n = 0;
  dopo_rooms_gen++;
  dopo_rooms_games = dopo_games_scan();

  jsmn_init(&parser);
  count = jsmn_parse(&parser, js, size, NULL, 0);
  if (count <= 0 || !(t = malloc(sizeof(*t) * count)))
    return;
  jsmn_init(&parser);
  if (jsmn_parse(&parser, js, size, t, count) <= 0 || t[0].type != JSMN_ARRAY)
  {
    free(t);
    return;
  }

  for (i = 1, k = 0; k < t[0].size && i < count && dopo_rooms_n < DOPO_ROOMS_MAX; k++)
  {
    const int fields = dopo_json_get(js, t, count, i, "fields");

    if (fields >= 0 && dopo_rooms_take(js, t, count, fields, &dopo_rooms[dopo_rooms_n]))
      dopo_rooms_n++;
    i = dopo_json_skip(t, count, i);
  }
  free(t);
  lprintf(LO_INFO, "dopo_rooms: %d PrBoomTV rooms in the lobby\n", dopo_rooms_n);
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */
/* ------------------------------------------------------------------ */

dbool dopo_rooms_available(void)
{
  return dopo_environment(DOPO_ENVIRONMENT_NETPLAY_GET_LOBBY, NULL);
}

static void dopo_rooms_ask(bool refresh)
{
  struct dopo_netplay_lobby lobby = {0};

  lobby.refresh = refresh;
  if (!dopo_environment(DOPO_ENVIRONMENT_NETPLAY_GET_LOBBY, &lobby))
  {
    dopo_rooms_status = DOPO_ROOMS_FAILED;
    return;
  }
  switch (lobby.state)
  {
    case DOPO_NETPLAY_LOBBY_LOADING:
      dopo_rooms_status = DOPO_ROOMS_LOADING;
      dopo_rooms_read = FALSE;
      break;
    case DOPO_NETPLAY_LOBBY_READY:
      if (!dopo_rooms_read && lobby.json)
      {
        dopo_rooms_read = TRUE;
        dopo_rooms_parse(lobby.json, lobby.size);
      }
      dopo_rooms_status = DOPO_ROOMS_READY;
      break;
    case DOPO_NETPLAY_LOBBY_FAILED:
      dopo_rooms_status = DOPO_ROOMS_FAILED;
      break;
    default:
      dopo_rooms_status = DOPO_ROOMS_NONE;
      break;
  }
}

void dopo_rooms_refresh(void)
{
  dopo_rooms_read = FALSE;
  dopo_rooms_ask(true);
}

dopo_rooms_state_t dopo_rooms_poll(void)
{
  if (dopo_rooms_status == DOPO_ROOMS_LOADING)
    dopo_rooms_ask(false);
  return dopo_rooms_status;
}

unsigned dopo_rooms_generation(void)
{
  return dopo_rooms_gen;
}

int dopo_rooms_count(void)
{
  return dopo_rooms_n;
}

const dopo_room_t *dopo_rooms_get(int i)
{
  return i >= 0 && i < dopo_rooms_n ? &dopo_rooms[i] : NULL;
}

void dopo_rooms_create(dbool listed, const char *relay)
{
  struct dopo_netplay_room room = {0};
  char game[64];
  char *dot;

  /* listed under the running game's name, as RetroArch lists content */
  snprintf(game, sizeof(game), "%s", dopo_games_current());
  if ((dot = strrchr(game, '.')) != NULL)
    *dot = 0;
  room.game_name = game;
  room.listed = listed ? true : false;
  room.relay = relay;
  dopo_join_pending = FALSE;
  dopo_environment(DOPO_ENVIRONMENT_NETPLAY_POST_LOBBY, &room);
}

/** @brief Asks the frontend to join a room. */
static void dopo_rooms_connect(const dopo_room_t *room)
{
  struct dopo_netplay_join join = {0};

  join.host = room->host;
  join.port = room->port;
  join.mitm_session = room->session;
  lprintf(LO_INFO, "dopo_rooms: joining %s (%s) at %s:%u%s%s\n", room->nick, room->game,
          room->host, room->port, room->session[0] ? " session " : "", room->session);
  dopo_environment(DOPO_ENVIRONMENT_NETPLAY_CONNECT, &join);
}

dbool dopo_rooms_join(int i)
{
  const dopo_room_t *room = dopo_rooms_get(i);

  if (!room || room->block != DOPO_ROOM_OK)
    return FALSE;
  /* the list may have been read before a switch: look the game up again */
  dopo_join_room = *room;
  dopo_rooms_games = dopo_games_scan();
  dopo_join_room.game_index = dopo_rooms_find_game(room->game);
  if (dopo_join_room.game_index < 0)
    return FALSE;
  if (dopo_games_is_current(dopo_join_room.game_index))
  {
    dopo_join_pending = FALSE;
    dopo_rooms_connect(&dopo_join_room);
    return TRUE;
  }
  /* its game first: the switch happens at the next frame, then we join */
  dopo_join_pending = TRUE;
  dopo_games_load(dopo_join_room.game_index);
  return TRUE;
}

void dopo_rooms_leave(void)
{
  dopo_join_pending = FALSE;
  dopo_environment(DOPO_ENVIRONMENT_NETPLAY_DISCONNECT, NULL);
}

void dopo_rooms_frame(void)
{
  /* the switch to the room's game is done */
  if (dopo_join_pending && !dopo_games_switching())
  {
    dopo_join_pending = FALSE;
    dopo_rooms_connect(&dopo_join_room);
  }
}
