/**
 * @brief Netplay transport: the RetroArch netpacket interface.
 *
 * RetroArch hosts and joins the session (rooms, lobby, relay); with this
 * interface set it stops syncing input and saving states, and only
 * carries the core's packets. start() says who this machine is: client
 * id 0 is the host, which drops the core straight into the lobby, as a
 * client does.
 */

/**
 * @brief Callbacks that forward the frontend's events to the session.
 *
 * @patch libretro/libretro.c 908
 */
#include "dopo/multiplayer.h"
#include "dopo/version.h"

static retro_netpacket_send_t dopo_netpacket_send_fn;

static void dopo_netpacket_send(uint16_t to, const void *buf, size_t len)
{
   if (dopo_netpacket_send_fn)
      dopo_netpacket_send_fn(RETRO_NETPACKET_RELIABLE | RETRO_NETPACKET_FLUSH_HINT,
            buf, len, to == DOPO_MP_ALL ? RETRO_NETPACKET_BROADCAST : to);
}

static void RETRO_CALLCONV dopo_netpacket_start(uint16_t client_id,
      retro_netpacket_send_t send_fn, retro_netpacket_poll_receive_t poll_receive_fn)
{
   const char *name = NULL;

   dopo_netpacket_send_fn = send_fn;
   environ_cb(RETRO_ENVIRONMENT_GET_USERNAME, &name);
   dopo_mp_on_start(client_id, name, dopo_netpacket_send);
}

static void RETRO_CALLCONV dopo_netpacket_receive(const void *buf, size_t len,
      uint16_t client_id)
{
   dopo_mp_on_receive(buf, len, client_id);
}

static void RETRO_CALLCONV dopo_netpacket_stop(void)
{
   dopo_mp_on_stop();
   dopo_netpacket_send_fn = NULL;
}

static void RETRO_CALLCONV dopo_netpacket_poll(void)
{
   dopo_mp_on_poll();
}

static bool RETRO_CALLCONV dopo_netpacket_connected(uint16_t client_id)
{
   return dopo_mp_on_connected(client_id);
}

static void RETRO_CALLCONV dopo_netpacket_disconnected(uint16_t client_id)
{
   dopo_mp_on_disconnected(client_id);
}

/* protocol_version: the core's version; RetroArch only warns when it
 * differs, so the session's HELLO checks it too (multiplayer/session.c) */
static const struct retro_netpacket_callback dopo_netpacket_callback =
{
   dopo_netpacket_start,
   dopo_netpacket_receive,
   dopo_netpacket_stop,
   dopo_netpacket_poll,
   dopo_netpacket_connected,
   dopo_netpacket_disconnected,
   DOPO_VERSION
};

/* @endpatch */

/**
 * @brief Registers the interface, which RetroArch takes in retro_init.
 *
 * @patch libretro/libretro.c 981
 */
   environ_cb(RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE,
         (void *)&dopo_netpacket_callback);
/* @endpatch */
