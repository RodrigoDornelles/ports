/**
 * @brief The room list for the dopo netplay calls (rooms.c): the lobby of
 * the page's Supabase project (dopo/supabase/functions/lobby), which
 * answers as the libretro lobby does. A host announces its room with
 * POST /add, again every HEARTBEAT as RetroArch does and when its players
 * change, and ends it with POST /remove; the list a core asks for is
 * GET /list, [{"fields": {...}}], handed over as it comes. A browser
 * host's address in it is its id (signal.js), which peer.js dials.
 */
mergeInto(LibraryManager.library, {
  $DOPO_ROOMS__deps: ['$DOPO_SIGNAL', '$stringToNewUTF8', '$lengthBytesUTF8', 'free'],
  $DOPO_ROOMS: {
    /* enum dopo_netplay_lobby_state */
    NONE: 0, LOADING: 1, READY: 2, FAILED: 3,
    /* RetroArch announces that often (NETPLAY_ANNOUNCE_TIME) */
    HEARTBEAT: 20000,

    state: 0,
    json: 0,
    size: 0,
    asked: 0,

    endpoint(path) {
      return DOPO_SIGNAL.config().url + '/functions/v1/lobby/' + path;
    },

    /* as RetroArch posts: form-urlencoded; keepalive so a room still ends
     * when the page goes */
    post(path, fields) {
      return fetch(DOPO_ROOMS.endpoint(path), {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams(fields).toString(),
        keepalive: true
      });
    },

    /* hosting: the room is listed until stop(); update() takes the players */
    announce(fields) {
      var mine = Object.assign({
        game_crc: '00000000',
        subsystem_name: 'N/A',
        frontend: 'web',
        has_password: 0,
        has_spectate_password: 0,
        spectator_count: 0
      }, fields, { peer: DOPO_SIGNAL.id(), player_count: 1 });
      var stopped = false;
      var send = () => {
        if (stopped) return;
        DOPO_ROOMS.post('add', mine).then((response) => {
          if (!response.ok) console.warn('[dopo] lobby: add answered ' + response.status);
        }, (e) => console.warn('[dopo] lobby: ' + e.message));
      };
      var stop = () => {
        if (stopped) return;
        stopped = true;
        clearInterval(timer);
        removeEventListener('pagehide', stop);
        DOPO_ROOMS.post('remove', { peer: mine.peer, port: mine.port }).catch(() => {});
      };
      var timer = setInterval(send, DOPO_ROOMS.HEARTBEAT);
      addEventListener('pagehide', stop);
      send();
      return {
        update(players) {
          if (mine.player_count === players + 1) return;
          mine.player_count = players + 1;
          send();
        },
        stop
      };
    },

    refresh() {
      var asked = ++DOPO_ROOMS.asked;
      var url;
      try {
        url = DOPO_ROOMS.endpoint('list');
      } catch (e) {
        console.warn('[dopo] ' + e.message);
        DOPO_ROOMS.state = DOPO_ROOMS.FAILED;
        return;
      }
      DOPO_ROOMS.state = DOPO_ROOMS.LOADING;
      fetch(url, { cache: 'no-store' }).then((response) => {
        if (!response.ok) throw new Error('lobby: list answered ' + response.status);
        return response.text();
      }).then((json) => {
        if (asked !== DOPO_ROOMS.asked) return;
        if (!Array.isArray(JSON.parse(json))) throw new Error('lobby: the list is not a list');
        if (DOPO_ROOMS.json) _free(DOPO_ROOMS.json);
        DOPO_ROOMS.json = stringToNewUTF8(json);
        DOPO_ROOMS.size = lengthBytesUTF8(json);
        DOPO_ROOMS.state = DOPO_ROOMS.READY;
      }).catch((e) => {
        if (asked !== DOPO_ROOMS.asked) return;
        console.warn('[dopo] ' + e.message);
        DOPO_ROOMS.state = DOPO_ROOMS.FAILED;
      });
    }
  },

  dopo_lobby_state__deps: ['$DOPO_ROOMS'],
  dopo_lobby_state: function(refresh) {
    if (refresh || DOPO_ROOMS.state === DOPO_ROOMS.NONE)
      DOPO_ROOMS.refresh();
    return DOPO_ROOMS.state;
  },

  /* rooms.c: a core looks at its rooms soon; with a lobby project there is
   * nothing to start ahead, as the list is one request */
  dopo_lobby_join__deps: ['$DOPO_SIGNAL'],
  dopo_lobby_join: function() {
    try {
      DOPO_SIGNAL.config();
      return true;
    } catch (e) {
      console.warn('[dopo] ' + e.message);
      return false;
    }
  },

  dopo_lobby_json__deps: ['$DOPO_ROOMS'],
  dopo_lobby_json: function(size) {
    {{{ makeSetValue('size', '0', 'DOPO_ROOMS.size', 'i32') }}};
    return DOPO_ROOMS.json;
  }
});
