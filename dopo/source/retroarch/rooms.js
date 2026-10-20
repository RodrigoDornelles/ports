/**
 * @brief The room list for the dopo netplay calls (rooms.c), in the
 * libretro lobby's format, from Trystero.
 *
 * Browsers meet in the Trystero room "lobby-<core>" (Module.dopo.core):
 * a host announces its room there through the "room" action, to everyone
 * when it changes and to each peer that comes in, and sends null when it
 * stops. The list a core asks for is what was heard, as the libretro
 * lobby's /list would give it ([{"fields": {...}}]), with the host's peer
 * id as the room's address (peer.js connects to it).
 */
mergeInto(LibraryManager.library, {
  $DOPO_ROOMS__deps: ['$DOPO_PEER', '$stringToNewUTF8', '$lengthBytesUTF8', 'free'],
  $DOPO_ROOMS: {
    /* enum dopo_netplay_lobby_state */
    NONE: 0, LOADING: 1, READY: 2, FAILED: 3,
    /* a list is ready once a room was heard and LISTEN went by, or after
     * SETTLE without any: peers take a few seconds to meet */
    SETTLE: 10000,
    LISTEN: 1500,
    /* a room not heard of for that long is gone */
    STALE: 30000,
    /* a host repeats its room that often */
    HEARTBEAT: 10000,

    state: 0,
    json: 0,
    size: 0,
    asked: 0,
    rooms: {},   /* peer id -> {fields, seen} */
    mine: null,  /* the room this page hosts */
    lobby: null,

    join() {
      if (DOPO_ROOMS.lobby)
        return DOPO_ROOMS.lobby;
      var dopo = Module['dopo'] || {};
      var lobby = DOPO_ROOMS.lobby = DOPO_PEER.join('lobby-' + (dopo['core'] || 'core'), 'room');
      lobby.room.onPeerJoin = (peer) => {
        console.log('[dopo] lobby: ' + peer + ' came in');
        if (DOPO_ROOMS.mine) lobby.net.send(DOPO_ROOMS.mine, { target: peer });
      };
      lobby.room.onPeerLeave = (peer) => {
        console.log('[dopo] lobby: ' + peer + ' left');
        delete DOPO_ROOMS.rooms[peer];
      };
      lobby.net.onMessage = (fields, meta) => {
        if (fields && typeof fields === 'object')
          DOPO_ROOMS.rooms[meta.peerId] = { fields, seen: Date.now() };
        else
          delete DOPO_ROOMS.rooms[meta.peerId];
      };
      console.log('[dopo] lobby: listening as ' + DOPO_PEER.trystero()['selfId']);
      return lobby;
    },

    /* hosting: the room goes out until stop(); update() takes the players */
    announce(fields) {
      var lobby = DOPO_ROOMS.join();
      var mine = Object.assign({}, fields, { player_count: 1 });
      var send = () => { if (DOPO_ROOMS.mine === mine) lobby.net.send(mine); };
      var timer = setInterval(send, DOPO_ROOMS.HEARTBEAT);
      DOPO_ROOMS.mine = mine;
      send();
      return {
        update(players) {
          if (mine.player_count === players + 1) return;
          mine.player_count = players + 1;
          send();
        },
        stop() {
          clearInterval(timer);
          if (DOPO_ROOMS.mine !== mine) return;
          DOPO_ROOMS.mine = null;
          lobby.net.send(null);
        }
      };
    },

    text(value, size) {
      return typeof value === 'string' ? value.slice(0, size) : '';
    },

    /* what was heard, as the libretro lobby's /list */
    list() {
      var now = Date.now();
      var rooms = [];
      Object.keys(DOPO_ROOMS.rooms).forEach((peer, i) => {
        var room = DOPO_ROOMS.rooms[peer];
        var fields = room.fields;
        if (now - room.seen > DOPO_ROOMS.STALE) {
          delete DOPO_ROOMS.rooms[peer];
          return;
        }
        rooms.push({ fields: {
          id: i + 1,
          username: DOPO_ROOMS.text(fields.username, 32),
          country: 'web',
          game_name: DOPO_ROOMS.text(fields.game_name, 64),
          game_crc: '00000000',
          core_name: DOPO_ROOMS.text(fields.core_name, 32),
          core_version: DOPO_ROOMS.text(fields.core_version, 24),
          subsystem_name: 'N/A',
          retroarch_version: '',
          frontend: 'web',
          ip: peer,
          port: Number(fields.port) || 55435,
          mitm_ip: '',
          mitm_port: 0,
          mitm_session: '',
          host_method: 0,
          has_password: false,
          has_spectate_password: false,
          connectable: true,
          is_retroarch: false,
          player_count: Number(fields.player_count) || 1,
          spectator_count: 0
        }});
      });
      return JSON.stringify(rooms);
    },

    refresh() {
      var asked = ++DOPO_ROOMS.asked;
      try {
        DOPO_ROOMS.join();
      } catch (e) {
        console.warn('[dopo] ' + e.message);
        DOPO_ROOMS.state = DOPO_ROOMS.FAILED;
        return;
      }
      DOPO_ROOMS.state = DOPO_ROOMS.LOADING;
      var since = Date.now();
      var check = () => {
        if (asked !== DOPO_ROOMS.asked) return;
        var listened = Date.now() - since;
        var heard = Object.keys(DOPO_ROOMS.rooms).length > 0;
        if (listened < DOPO_ROOMS.SETTLE && !(heard && listened >= DOPO_ROOMS.LISTEN)) {
          setTimeout(check, 250);
          return;
        }
        var json = DOPO_ROOMS.list();
        if (DOPO_ROOMS.json) _free(DOPO_ROOMS.json);
        DOPO_ROOMS.json = stringToNewUTF8(json);
        DOPO_ROOMS.size = lengthBytesUTF8(json);
        DOPO_ROOMS.state = DOPO_ROOMS.READY;
      };
      check();
    }
  },

  dopo_lobby_state__deps: ['$DOPO_ROOMS'],
  dopo_lobby_state: function(refresh) {
    if (refresh || DOPO_ROOMS.state === DOPO_ROOMS.NONE)
      DOPO_ROOMS.refresh();
    return DOPO_ROOMS.state;
  },

  /* rooms.c: a core looks at its rooms soon, the lobby starts listening */
  dopo_lobby_join__deps: ['$DOPO_ROOMS'],
  dopo_lobby_join: function() {
    try {
      DOPO_ROOMS.join();
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
