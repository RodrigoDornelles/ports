/**
 * @brief The dopo netplay calls (dopo/netplay.h) in the browser, so a
 * core lists, hosts and joins rooms from its own menus as it does with
 * the dopo frontend.
 *
 * The core gets the lobby it knows: rooms.js hands over a room list in
 * the libretro lobby's format, made of what hosts announce over Trystero
 * instead, each room's address being its host's peer id; connecting to
 * it is a WebRTC channel to that host (peer.js). Requests are taken
 * during retro_run and carried out before the next one, so the core's
 * netpacket start() and stop() come between frames.
 */

/**
 * @brief The calls and the requests they leave for the next frame.
 *
 * @patch runloop.c 1350
 */
#ifdef __EMSCRIPTEN__
#include "dopo/netplay.h"

/* rooms.js */
extern bool        dopo_lobby_join(void);
extern int         dopo_lobby_state(bool refresh);
extern const char *dopo_lobby_json(size_t *size);
/* peer.js */
extern void        dopo_peer_host(unsigned port, bool listed, const char *game,
      const char *nick, const char *core, const char *version);

enum dopo_request_kind
{
   DOPO_REQUEST_NONE = 0,
   DOPO_REQUEST_HOST,
   DOPO_REQUEST_JOIN,
   DOPO_REQUEST_LEAVE
};

static struct
{
   enum dopo_request_kind kind;
   char     host[256];
   char     game[128];
   unsigned port;
   bool     listed;
} dopo_request;

static bool dopo_environment(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case DOPO_ENVIRONMENT_NETPLAY_GET_LOBBY:
         /* asked whether there are rooms: the core's Multiplayer menu
          * opened, so the lobby starts listening for the list to come */
         if (!data)
            return dopo_lobby_join();
         {
            struct dopo_netplay_lobby *lobby = (struct dopo_netplay_lobby*)data;

            lobby->state = dopo_lobby_state(lobby->refresh);
            lobby->json  = NULL;
            lobby->size  = 0;
            if (lobby->state == DOPO_NETPLAY_LOBBY_READY)
               lobby->json = dopo_lobby_json(&lobby->size);
         }
         return true;
      case DOPO_ENVIRONMENT_NETPLAY_POST_LOBBY:
         {
            const struct dopo_netplay_room *room = (const struct dopo_netplay_room*)data;

            if (!room)
               return false;
            /* no relays here: the host's peer id is the way in */
            dopo_request.kind   = DOPO_REQUEST_HOST;
            dopo_request.port   = room->port ? room->port : 55435;
            dopo_request.listed = room->listed;
            strlcpy(dopo_request.game, room->game_name ? room->game_name : "",
                  sizeof(dopo_request.game));
         }
         return true;
      case DOPO_ENVIRONMENT_NETPLAY_CONNECT:
         {
            const struct dopo_netplay_join *join = (const struct dopo_netplay_join*)data;

            if (!join || string_is_empty(join->host))
               return false;
            strlcpy(dopo_request.host, join->host, sizeof(dopo_request.host));
            dopo_request.kind = DOPO_REQUEST_JOIN;
            dopo_request.port = join->port ? join->port : 55435;
         }
         return true;
      case DOPO_ENVIRONMENT_NETPLAY_DISCONNECT:
         dopo_request.kind = DOPO_REQUEST_LEAVE;
         return true;
      default:
         break;
   }
   return false;
}

static void dopo_requests(void)
{
   settings_t *settings = config_get_ptr();
   char hostname[300];

   switch (dopo_request.kind)
   {
      case DOPO_REQUEST_HOST:
         /* announced over Trystero, never in the libretro lobby */
         settings->uints.netplay_port            = dopo_request.port;
         settings->bools.netplay_public_announce = false;
         settings->bools.netplay_use_mitm_server = false;
         settings->bools.netplay_nat_traversal   = false;
         if (command_event(CMD_EVENT_NETPLAY_ENABLE_HOST, NULL))
            dopo_peer_host(dopo_request.port, dopo_request.listed,
                  dopo_request.game, settings->paths.username,
                  runloop_state.system.info.library_name,
                  runloop_state.system.info.library_version);
         break;
      case DOPO_REQUEST_JOIN:
         snprintf(hostname, sizeof(hostname), "%s|%u",
               dopo_request.host, dopo_request.port);
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_USE_CORE_PACKET_INTERFACE, NULL))
         {
            netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
            command_event(CMD_EVENT_NETPLAY_INIT_DIRECT, hostname);
         }
         break;
      case DOPO_REQUEST_LEAVE:
         command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         break;
      default:
         return;
   }
   dopo_request.kind = DOPO_REQUEST_NONE;
}
#endif

/* @endpatch */

/**
 * @brief The calls go to dopo_environment.
 *
 * @patch runloop.c 1376
 */
#ifdef __EMSCRIPTEN__
   if (     cmd >= DOPO_ENVIRONMENT_NETPLAY_GET_LOBBY
         && cmd <= DOPO_ENVIRONMENT_NETPLAY_DISCONNECT)
      return dopo_environment(cmd, data);
#endif

/* @endpatch */

/**
 * @brief The requests of the last frame, before this one runs.
 *
 * @patch runloop.c 7169
 */
#ifdef __EMSCRIPTEN__
   dopo_requests();
#endif
/* @endpatch */
