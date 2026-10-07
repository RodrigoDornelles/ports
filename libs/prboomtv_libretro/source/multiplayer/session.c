/**
 * @brief Multiplayer session: the lobby (slots, names, setup), the
 * packets between machines and the start and end of a netgame.
 *
 * The transport (multiplayer/interface/) calls the dopo_mp_on_* events;
 * the game starts and ends in dopo_mp_frame(), at the top of TryRunTics,
 * never in the middle of a tic.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "doomstat.h"
#include "d_main.h"
#include "g_game.h"
#include "st_stuff.h"
#include "lprintf.h"
#include "dopo/cheats.h"
#include "dopo/multiplayer.h"

void M_StartMessage(const char *string, void *routine, dbool input);
void M_ClearMenus(void);
extern dbool quit_pressed;

/* START: type, complevel, mode, skill, episode, map, players, cheats,
 * then the G_WriteOptions block */
#define DOPO_START_CHEATS  7
#define DOPO_START_OPTIONS (DOPO_START_CHEATS + 6)
#define DOPO_START_SIZE    (DOPO_START_OPTIONS + GAME_OPTION_SIZE)

/* TIC: type, tic, slot, then the ticcmd */
#define DOPO_TIC_CMD  6
#define DOPO_TIC_SIZE (DOPO_TIC_CMD + 10)

/* ROSTER: type, then per slot: used, admin, client id, name */
#define DOPO_ROSTER_SLOT (4 + DOPO_MP_NAME + 1)
#define DOPO_ROSTER_SIZE (1 + MAXPLAYERS * DOPO_ROSTER_SLOT)

/* KILL: type, tic, slot; TELEPORT: type, tic, slot, target;
 * LEVEL: type, tic, mode, skill, episode, map */
#define DOPO_KILL_SIZE     6
#define DOPO_TELEPORT_SIZE 7
#define DOPO_LEVEL_SIZE    9

/* PINGS: type, then per slot the ping in ms (0xFFFF unknown) */
#define DOPO_PINGS_SIZE (1 + MAXPLAYERS * 2)

/** @brief Frames between two pings of the host. */
#define DOPO_PING_FRAMES 35

/** @brief Events stamped for a tic that may wait at once. */
#define DOPO_EVENTS 16

static dopo_mp_state_t  dopo_state;
static uint16_t         dopo_self;
static dopo_mp_send_t   dopo_send;
static dopo_mp_slot_t   dopo_slots[MAXPLAYERS];
static dopo_mp_config_t dopo_config = { DOPO_MP_COOP, sk_medium, 1, 1 };

static dbool   dopo_start_pending;
static uint8_t dopo_start_packet[DOPO_START_SIZE];
static dbool   dopo_end_pending;

/**
 * @brief Events (KILL, TELEPORT, LEVEL packets) waiting for their tic, in
 * the order they came.
 */
static struct
{
  int     tic;
  uint8_t packet[DOPO_LEVEL_SIZE];
} dopo_events[DOPO_EVENTS];
static int dopo_events_count;

static int   dopo_deaths[MAXPLAYERS];
static dbool dopo_was_dead[MAXPLAYERS];

/* ------------------------------------------------------------------ */
/* Bytes                                                               */
/* ------------------------------------------------------------------ */

static void dopo_put16(uint8_t *p, unsigned v)
{
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}

static void dopo_put32(uint8_t *p, unsigned long v)
{
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static unsigned dopo_get16(const uint8_t *p)
{
  return (unsigned)p[0] << 8 | p[1];
}

static unsigned long dopo_get32(const uint8_t *p)
{
  return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 |
         (unsigned long)p[2] << 8 | p[3];
}

static void dopo_put_cheats(uint8_t *p, const dopo_cheats_t *c)
{
  p[0] = c->ammo != 0;
  p[1] = c->life != 0;
  p[2] = c->weapons != 0;
  p[3] = c->damage != 0;
  p[4] = (uint8_t)c->aim_assist;
  p[5] = (uint8_t)c->trigger_assist;
}

static void dopo_get_cheats(const uint8_t *p, dopo_cheats_t *c)
{
  c->ammo           = p[0];
  c->life           = p[1];
  c->weapons        = p[2];
  c->damage         = p[3];
  c->aim_assist     = p[4];
  c->trigger_assist = p[5];
}

static void dopo_put_cmd(uint8_t *p, const ticcmd_t *cmd)
{
  p[0] = (uint8_t)cmd->forwardmove;
  p[1] = (uint8_t)cmd->sidemove;
  dopo_put16(p + 2, (uint16_t)cmd->angleturn);
  dopo_put16(p + 4, (uint16_t)cmd->consistancy);
  p[6] = cmd->chatchar;
  p[7] = cmd->buttons;
  p[8] = cmd->arti;
  p[9] = cmd->lookfly;
}

static void dopo_get_cmd(const uint8_t *p, ticcmd_t *cmd)
{
  cmd->forwardmove = (signed char)p[0];
  cmd->sidemove    = (signed char)p[1];
  cmd->angleturn   = (signed short)dopo_get16(p + 2);
  cmd->consistancy = (short)dopo_get16(p + 4);
  cmd->chatchar    = p[6];
  cmd->buttons     = p[7];
  cmd->arti        = p[8];
  cmd->lookfly     = p[9];
}

/* ------------------------------------------------------------------ */
/* Slots                                                               */
/* ------------------------------------------------------------------ */

static int dopo_slot_of(uint16_t client)
{
  int i;

  for (i = 0; i < MAXPLAYERS; i++)
    if (dopo_slots[i].used && dopo_slots[i].client == client)
      return i;
  return -1;
}

/**
 * @brief Copies a name the frontend or a client gave, keeping printable
 * ASCII only; an empty name becomes "PLAYER n".
 */
static void dopo_set_name(int slot, const char *name, size_t len)
{
  char *out = dopo_slots[slot].name;
  size_t i, n = 0;

  for (i = 0; name && i < len && name[i] && n < DOPO_MP_NAME; i++)
    if (name[i] >= ' ' && name[i] <= '~')
      out[n++] = name[i];
  out[n] = '\0';
  if (!n)
    snprintf(out, DOPO_MP_NAME + 1, "PLAYER %d", (slot + 1) & 0xFF);
}

static void dopo_send_roster(void)
{
  uint8_t p[DOPO_ROSTER_SIZE];
  int i;

  memset(p, 0, sizeof(p));
  p[0] = DOPO_MP_ROSTER;
  for (i = 0; i < MAXPLAYERS; i++)
  {
    uint8_t *s = p + 1 + i * DOPO_ROSTER_SLOT;

    s[0] = dopo_slots[i].used;
    s[1] = dopo_slots[i].admin;
    dopo_put16(s + 2, dopo_slots[i].client);
    memcpy(s + 4, dopo_slots[i].name, DOPO_MP_NAME + 1);
  }
  dopo_send(DOPO_MP_ALL, p, sizeof(p));
}

static void dopo_send_config(void)
{
  uint8_t p[5];

  p[0] = DOPO_MP_CONFIG;
  p[1] = (uint8_t)dopo_config.mode;
  p[2] = (uint8_t)dopo_config.skill;
  p[3] = (uint8_t)dopo_config.episode;
  p[4] = (uint8_t)dopo_config.map;
  dopo_send(DOPO_MP_ALL, p, sizeof(p));
}

/* ------------------------------------------------------------------ */
/* Events stamped for a tic                                            */
/* ------------------------------------------------------------------ */

static void dopo_queue_event(const uint8_t *p, size_t len)
{
  if (dopo_events_count == DOPO_EVENTS || len > DOPO_LEVEL_SIZE)
    return;
  dopo_events[dopo_events_count].tic = (int)dopo_get32(p + 1);
  memcpy(dopo_events[dopo_events_count].packet, p, len);
  dopo_events_count++;
}

/**
 * @brief Host: stamps an event with the next tic it builds a ticcmd for,
 * which no machine runs before having the host's ticcmd, and sends it.
 */
static void dopo_host_event(uint8_t *p, size_t len)
{
  dopo_put32(p + 1, (unsigned long)dopo_mp_lockstep_next_build());
  dopo_queue_event(p, len);
  dopo_send(DOPO_MP_ALL, p, len);
}

static void dopo_apply_event(const uint8_t *p)
{
  if (p[0] == DOPO_MP_KILL)
    dopo_mp_kill(p[5]);
  else if (p[0] == DOPO_MP_TELEPORT)
    dopo_mp_teleport(p[5], p[6]);
  else if (p[0] == DOPO_MP_LEVEL)
  {
    deathmatch = p[5];
    G_InitNew((skill_t)p[6], p[7], p[8]);
  }
}

void dopo_mp_session_tic(int tic)
{
  int i, kept = 0;

  for (i = 0; i < dopo_events_count; i++)
    if (dopo_events[i].tic <= tic)
      dopo_apply_event(dopo_events[i].packet);
    else
      dopo_events[kept++] = dopo_events[i];
  dopo_events_count = kept;

  for (i = 0; i < MAXPLAYERS; i++)
  {
    const dbool dead = playeringame[i] && players[i].playerstate == PST_DEAD;

    if (dead && !dopo_was_dead[i])
      dopo_deaths[i]++;
    dopo_was_dead[i] = dead;
  }
}

int dopo_mp_deaths(int slot)
{
  return slot >= 0 && slot < MAXPLAYERS ? dopo_deaths[slot] : 0;
}

/* ------------------------------------------------------------------ */
/* Admin and ping                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Host: does an admin action already allowed.
 */
static void dopo_admin_do(int action, int slot, int target)
{
  uint8_t p[DOPO_TELEPORT_SIZE];

  if (slot < 0 || slot >= MAXPLAYERS || !dopo_slots[slot].used)
    return;

  switch (action)
  {
    case DOPO_MP_ADMIN_TOGGLE:
      if (slot != 0)
      {
        dopo_slots[slot].admin = !dopo_slots[slot].admin;
        dopo_send_roster();
      }
      break;

    case DOPO_MP_ADMIN_KICK:
      if (slot != 0)
      {
        p[0] = DOPO_MP_KICK;
        dopo_send(dopo_slots[slot].client, p, 1);
      }
      break;

    case DOPO_MP_ADMIN_KILL:
      if (dopo_state == DOPO_MP_GAME)
      {
        p[0] = DOPO_MP_KILL;
        p[5] = (uint8_t)slot;
        dopo_host_event(p, DOPO_KILL_SIZE);
      }
      break;

    case DOPO_MP_ADMIN_TELEPORT:
      if (dopo_state == DOPO_MP_GAME && target >= 0 && target < MAXPLAYERS &&
          target != slot && dopo_slots[target].used)
      {
        p[0] = DOPO_MP_TELEPORT;
        p[5] = (uint8_t)slot;
        p[6] = (uint8_t)target;
        dopo_host_event(p, DOPO_TELEPORT_SIZE);
      }
      break;
  }
}

dbool dopo_mp_is_admin(void)
{
  const int self = dopo_mp_self_slot();

  return dopo_state == DOPO_MP_OFF || (self >= 0 && dopo_slots[self].admin);
}

void dopo_mp_admin(dopo_mp_admin_t action, int slot, int target)
{
  uint8_t p[4];

  if (dopo_state == DOPO_MP_OFF || !dopo_mp_is_admin())
    return;
  if (dopo_mp_is_host())
  {
    dopo_admin_do(action, slot, target);
    return;
  }
  p[0] = DOPO_MP_ADMIN;
  p[1] = (uint8_t)action;
  p[2] = (uint8_t)slot;
  p[3] = (uint8_t)target;
  dopo_send(DOPO_MP_HOST, p, sizeof(p));
}

static unsigned long dopo_now_ms(void)
{
  struct timespec t;

  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long)t.tv_sec * 1000UL + (unsigned long)(t.tv_nsec / 1000000L);
}

/**
 * @brief Host: asks every client for a round trip, and sends everyone the
 * pings measured so far.
 */
static void dopo_host_ping(void)
{
  uint8_t ping[5], pings[DOPO_PINGS_SIZE];
  int i;

  ping[0] = DOPO_MP_PING;
  dopo_put32(ping + 1, dopo_now_ms());
  dopo_send(DOPO_MP_ALL, ping, sizeof(ping));

  pings[0] = DOPO_MP_PINGS;
  for (i = 0; i < MAXPLAYERS; i++)
    dopo_put16(pings + 1 + i * 2,
               dopo_slots[i].used && dopo_slots[i].ping >= 0 ?
               (unsigned)dopo_slots[i].ping : 0xFFFF);
  dopo_send(DOPO_MP_ALL, pings, sizeof(pings));
}

/* ------------------------------------------------------------------ */
/* Session                                                             */
/* ------------------------------------------------------------------ */

dopo_mp_state_t dopo_mp_state(void)
{
  return dopo_state;
}

dbool dopo_mp_is_host(void)
{
  return dopo_state == DOPO_MP_OFF || dopo_self == DOPO_MP_HOST;
}

const dopo_mp_slot_t *dopo_mp_slots(void)
{
  return dopo_slots;
}

int dopo_mp_self_slot(void)
{
  return dopo_slot_of(dopo_self);
}

const dopo_mp_config_t *dopo_mp_config(void)
{
  return &dopo_config;
}

void dopo_mp_set_config(const dopo_mp_config_t *config)
{
  if (dopo_state == DOPO_MP_OFF || !dopo_mp_is_host())
    return;
  dopo_config = *config;
  dopo_send_config();
}

void dopo_mp_start_game(void)
{
  uint8_t p[DOPO_LEVEL_SIZE];

  if (!dopo_mp_is_host())
    return;
  if (dopo_state == DOPO_MP_LOBBY)
  {
    dopo_start_pending = TRUE;
    return;
  }
  if (dopo_state != DOPO_MP_GAME)
    return;

  p[0] = DOPO_MP_LEVEL;
  p[5] = (uint8_t)dopo_config.mode;
  p[6] = (uint8_t)dopo_config.skill;
  p[7] = (uint8_t)dopo_config.episode;
  p[8] = (uint8_t)dopo_config.map;
  dopo_host_event(p, DOPO_LEVEL_SIZE);
}

/**
 * @brief Host: writes the START packet from the lobby setup and the
 * game's defaults, the options every machine will run with.
 */
static void dopo_build_start(void)
{
  uint8_t *p = dopo_start_packet;
  int i;

  G_ReloadDefaults();
  memset(p, 0, DOPO_START_SIZE);
  p[0] = DOPO_MP_START;
  p[1] = (uint8_t)compatibility_level;
  p[2] = (uint8_t)dopo_config.mode;
  p[3] = (uint8_t)dopo_config.skill;
  p[4] = (uint8_t)dopo_config.episode;
  p[5] = (uint8_t)dopo_config.map;
  for (i = 0; i < MAXPLAYERS; i++)
    if (dopo_slots[i].used)
      p[6] |= 1 << i;
  dopo_put_cheats(p + DOPO_START_CHEATS, dopo_cheats_wanted());
  G_WriteOptions(p + DOPO_START_OPTIONS);
}

/**
 * @brief Every machine: starts the netgame of the START packet, at tic 0.
 */
static void dopo_begin_game(const uint8_t *p)
{
  const int self = dopo_mp_self_slot();
  dopo_cheats_t cheats;
  int i;

  if (self < 0 || !(p[6] & (1 << self)))
    return;

  G_ReloadDefaults();  /* also stops the title demos */
  compatibility_level = p[1];
  G_ReadOptions(p + DOPO_START_OPTIONS);

  netgame = TRUE;
  deathmatch = p[2];
  for (i = 0; i < MAXPLAYERS; i++)
    playeringame[i] = (p[6] >> i) & 1;
  consoleplayer = displayplayer = self;

  dopo_get_cheats(p + DOPO_START_CHEATS, &cheats);
  dopo_cheats_at(-1, &cheats);

  dopo_events_count = 0;
  memset(dopo_deaths, 0, sizeof(dopo_deaths));
  memset(dopo_was_dead, 0, sizeof(dopo_was_dead));

  advancedemo = FALSE;
  gameaction = ga_nothing;
  M_ClearMenus();
  dopo_mp_consistancy_reset();
  G_InitNew((skill_t)p[3], p[4], p[5]);
  dopo_mp_lockstep_begin();
  dopo_state = DOPO_MP_GAME;
  lprintf(LO_INFO, "dopo_mp: game started as player %d\n", self + 1);
}

/**
 * @brief Every machine: back to a game of one, on the title screen.
 */
static void dopo_end_game(void)
{
  int i;

  netgame = FALSE;
  deathmatch = 0;
  for (i = 1; i < MAXPLAYERS; i++)
    playeringame[i] = FALSE;
  consoleplayer = displayplayer = 0;
  D_StartTitle();
  M_StartMessage("The netgame ended.\n\nPress a key.", NULL, FALSE);
}

void dopo_mp_frame(void)
{
  if (dopo_end_pending)
  {
    dopo_end_pending = FALSE;
    dopo_end_game();
  }

  if (!dopo_start_pending)
    return;
  dopo_start_pending = FALSE;

  if (dopo_mp_is_host())
  {
    dopo_build_start();
    dopo_mp_lockstep_reset();
    dopo_send(DOPO_MP_ALL, dopo_start_packet, DOPO_START_SIZE);
  }
  dopo_begin_game(dopo_start_packet);
}

void dopo_mp_send_tic(int slot, int tic, const ticcmd_t *cmd)
{
  uint8_t p[DOPO_TIC_SIZE];

  if (!dopo_send)
    return;
  p[0] = DOPO_MP_TIC;
  dopo_put32(p + 1, (unsigned long)tic);
  p[5] = (uint8_t)slot;
  dopo_put_cmd(p + DOPO_TIC_CMD, cmd);
  dopo_send(dopo_mp_is_host() ? DOPO_MP_ALL : DOPO_MP_HOST, p, sizeof(p));
}

void dopo_mp_send_cheats(int tic)
{
  uint8_t p[11];

  if (!dopo_send || !dopo_mp_is_host())
    return;
  p[0] = DOPO_MP_CHEATS;
  dopo_put32(p + 1, (unsigned long)tic);
  dopo_put_cheats(p + 5, dopo_cheats_wanted());
  dopo_send(DOPO_MP_ALL, p, sizeof(p));
}

/* ------------------------------------------------------------------ */
/* Transport events                                                    */
/* ------------------------------------------------------------------ */

void dopo_mp_on_start(uint16_t self, const char *name, dopo_mp_send_t send)
{
  int i;

  memset(dopo_slots, 0, sizeof(dopo_slots));
  dopo_self = self;
  dopo_send = send;
  dopo_state = DOPO_MP_LOBBY;
  dopo_start_pending = FALSE;
  dopo_config.skill = defaultskill - 1;
  lprintf(LO_INFO, "dopo_mp: session started as %s (client %u)\n",
          self == DOPO_MP_HOST ? "host" : "client", self);

  for (i = 0; i < MAXPLAYERS; i++)
    dopo_slots[i].ping = -1;

  if (self == DOPO_MP_HOST)
  {
    dopo_slots[0].used = TRUE;
    dopo_slots[0].admin = TRUE;
    dopo_slots[0].client = DOPO_MP_HOST;
    dopo_slots[0].ping = 0;
    dopo_set_name(0, name, DOPO_MP_NAME);
  }
  else
  {
    uint8_t p[1 + DOPO_MP_NAME + 1];

    memset(p, 0, sizeof(p));
    p[0] = DOPO_MP_HELLO;
    if (name)
      strncpy((char *)p + 1, name, DOPO_MP_NAME);
    dopo_send(DOPO_MP_HOST, p, sizeof(p));
  }
}

/**
 * @brief Host: the slot of a client, given a free one the first time;
 * -1 when the game already started or every slot is taken. A client's
 * HELLO may come before the frontend reports it connected.
 */
static int dopo_host_join(uint16_t client)
{
  int i = dopo_slot_of(client);

  if (i >= 0 || dopo_state != DOPO_MP_LOBBY)
    return i;
  for (i = 0; i < MAXPLAYERS; i++)
    if (!dopo_slots[i].used)
    {
      dopo_slots[i].used = TRUE;
      dopo_slots[i].admin = FALSE;
      dopo_slots[i].client = client;
      dopo_slots[i].ping = -1;
      dopo_set_name(i, NULL, 0);
      dopo_send_roster();
      dopo_send_config();
      return i;
    }
  return -1;
}

dbool dopo_mp_on_connected(uint16_t client)
{
  if (dopo_host_join(client) < 0)
    return FALSE;
  /* a HELLO that came first got a slot, but the frontend only delivers
   * to the client from now on */
  dopo_send_roster();
  dopo_send_config();
  return TRUE;
}

void dopo_mp_on_disconnected(uint16_t client)
{
  const int slot = dopo_slot_of(client);

  if (slot < 0)
    return;

  /* freed first: a send to the client that is gone may report it again */
  dopo_slots[slot].used = FALSE;
  dopo_slots[slot].admin = FALSE;

  if (dopo_state == DOPO_MP_GAME)
  {
    /* everyone runs the tics the host has from this player, then goes on
     * without them */
    const int tic = dopo_mp_lockstep_received(slot);
    uint8_t p[6];

    p[0] = DOPO_MP_LEAVE;
    dopo_put32(p + 1, (unsigned long)tic);
    p[5] = (uint8_t)slot;
    dopo_mp_lockstep_leave(slot, tic);
    dopo_send(DOPO_MP_ALL, p, sizeof(p));
  }
  dopo_send_roster();
}

void dopo_mp_on_receive(const void *buf, size_t len, uint16_t from)
{
  const uint8_t *p = buf;
  const dbool host = dopo_mp_is_host();
  int i;

  if (!len || dopo_state == DOPO_MP_OFF)
    return;

  switch (p[0])
  {
    case DOPO_MP_HELLO:
      if (host && (i = dopo_host_join(from)) >= 0)
      {
        dopo_set_name(i, (const char *)p + 1, len - 1);
        dopo_send_roster();
      }
      break;

    case DOPO_MP_ROSTER:
      if (!host && len >= DOPO_ROSTER_SIZE)
        for (i = 0; i < MAXPLAYERS; i++)
        {
          const uint8_t *s = p + 1 + i * DOPO_ROSTER_SLOT;

          dopo_slots[i].used = s[0];
          dopo_slots[i].admin = s[1];
          dopo_slots[i].client = dopo_get16(s + 2);
          dopo_set_name(i, (const char *)s + 4, DOPO_MP_NAME);
        }
      break;

    case DOPO_MP_CONFIG:
      if (!host && len >= 5)
      {
        dopo_config.mode    = p[1];
        dopo_config.skill   = p[2];
        dopo_config.episode = p[3];
        dopo_config.map     = p[4];
      }
      break;

    case DOPO_MP_START:
      if (!host && len >= DOPO_START_SIZE && dopo_state == DOPO_MP_LOBBY)
      {
        memcpy(dopo_start_packet, p, DOPO_START_SIZE);
        dopo_mp_lockstep_reset();
        dopo_start_pending = TRUE;
      }
      break;

    case DOPO_MP_TIC:
      if (len >= DOPO_TIC_SIZE)
      {
        ticcmd_t cmd;
        int slot = p[5];

        /* the host trusts the sender, not the slot it wrote */
        if (host && (slot = dopo_slot_of(from)) < 0)
          break;
        dopo_get_cmd(p + DOPO_TIC_CMD, &cmd);
        dopo_mp_lockstep_store(slot, (int)dopo_get32(p + 1), &cmd);

        if (host)
        {
          uint8_t relay[DOPO_TIC_SIZE];

          memcpy(relay, p, DOPO_TIC_SIZE);
          relay[5] = (uint8_t)slot;
          for (i = 1; i < MAXPLAYERS; i++)
            if (dopo_slots[i].used && dopo_slots[i].client != from)
              dopo_send(dopo_slots[i].client, relay, sizeof(relay));
        }
      }
      break;

    case DOPO_MP_CHEATS:
      if (!host && len >= 11)
      {
        dopo_cheats_t cheats;

        dopo_get_cheats(p + 5, &cheats);
        dopo_cheats_at((int)dopo_get32(p + 1), &cheats);
      }
      break;

    case DOPO_MP_LEAVE:
      if (!host && len >= 6 && p[5] < MAXPLAYERS)
        dopo_mp_lockstep_leave(p[5], (int)dopo_get32(p + 1));
      break;

    case DOPO_MP_ADMIN:
      if (host && len >= 4 && (i = dopo_slot_of(from)) >= 0 &&
          dopo_slots[i].admin)
        dopo_admin_do(p[1], p[2], p[3]);
      break;

    case DOPO_MP_KICK:
      if (!host)
      {
        lprintf(LO_INFO, "dopo_mp: kicked by an admin\n");
        quit_pressed = TRUE;
      }
      break;

    case DOPO_MP_KILL:
      if (!host && len >= DOPO_KILL_SIZE)
        dopo_queue_event(p, DOPO_KILL_SIZE);
      break;

    case DOPO_MP_TELEPORT:
      if (!host && len >= DOPO_TELEPORT_SIZE)
        dopo_queue_event(p, DOPO_TELEPORT_SIZE);
      break;

    case DOPO_MP_LEVEL:
      if (!host && len >= DOPO_LEVEL_SIZE)
      {
        dopo_config.mode    = p[5];
        dopo_config.skill   = p[6];
        dopo_config.episode = p[7];
        dopo_config.map     = p[8];
        dopo_queue_event(p, DOPO_LEVEL_SIZE);
      }
      break;

    case DOPO_MP_PING:
      if (!host)
        dopo_send(DOPO_MP_HOST, p, len);
      else if (len >= 5 && (i = dopo_slot_of(from)) >= 0)
      {
        /* both 32 bit, so the subtraction wraps right */
        const unsigned long rtt =
          (dopo_now_ms() - dopo_get32(p + 1)) & 0xFFFFFFFFUL;

        dopo_slots[i].ping = rtt > 9999 ? 9999 : (int)rtt;
      }
      break;

    case DOPO_MP_PINGS:
      if (!host && len >= DOPO_PINGS_SIZE)
        for (i = 0; i < MAXPLAYERS; i++)
        {
          const unsigned ping = dopo_get16(p + 1 + i * 2);

          dopo_slots[i].ping = ping == 0xFFFF ? -1 : (int)ping;
        }
      break;
  }
}

void dopo_mp_on_stop(void)
{
  lprintf(LO_INFO, "dopo_mp: session stopped\n");
  if (dopo_state == DOPO_MP_GAME)
    dopo_end_pending = TRUE;
  dopo_state = DOPO_MP_OFF;
  dopo_send = NULL;
  dopo_start_pending = FALSE;
}

void dopo_mp_on_poll(void)
{
  static int frames;

  if (dopo_state != DOPO_MP_OFF && dopo_mp_is_host() &&
      ++frames % DOPO_PING_FRAMES == 0)
    dopo_host_ping();
  if (dopo_state == DOPO_MP_LOBBY && !menuactive)
    dopo_lobby_open();
}
