# Netplay lobby (Supabase)

The web build's netplay meets here instead of public relays:

- **Lobby**: the `lobby` Edge Function answers like `lobby.libretro.com`:
  `POST /functions/v1/lobby/add` (RetroArch's form announce, answered with
  `key=value` lines), `GET /functions/v1/lobby/list` (`[{"fields": {...}}]`)
  and, extra, `POST /functions/v1/lobby/remove`. A room not announced for
  60 s is gone. Rooms live in `public.lobby_rooms` (`migrations/`), which
  only the function reads and writes.
- **Handshake**: the pages swap their WebRTC offer and answer over Realtime
  broadcast, topic `dopo:<host id>`, sealed with ECDH + AES-GCM
  (`dopo/source/retroarch/signal.js`). The game itself goes browser to
  browser and never through Supabase.

## Setting up a project

From the repository root, once (the Supabase CLI is a dev dependency in
package.json):

```sh
npm install                   # the CLI, into node_modules
npm run lobby:login           # opens the browser
npm run lobby:link            # the project of package.json (its ref is public)
npm run lobby:db              # creates the table on it (--remote: the CLI's
                              # default since v3 is a local Docker stack)
npm run lobby:deploy          # the lobby, no JWT, bundled by Supabase (no Docker)
```

Realtime broadcast on public channels needs nothing more (Project Settings >
Realtime: public access allowed, the default).

`lobby.json` holds the project: the site browsers may call the lobby from,
the project id and the **anon** key (Project Settings > API), all public;
the scripts, the function and the build read it, and the build copies it
next to the player. Never put the `service_role` key there: the function
gets it on its own.
