/**
 * @brief Netplay's TCP sockets over WebRTC (Trystero), so a browser plays
 * against browsers: emscripten's own sockets (SOCKFS) only reach
 * WebSocket servers and cannot listen at all.
 *
 * A host is reached by its Trystero peer id, the address its room has in
 * the lobby (rooms.js): it waits in the Trystero room "game-<its id>", and
 * a player connecting to that id joins the room and talks to it alone
 * through the "net" action, which keeps order and splits large sends as
 * TCP would. Stream sockets take these ops; the others keep emscripten's.
 *
 * connect() succeeds at once and sends wait for the host: RetroArch waits
 * for a connect by polling with a timeout, which cannot block in the
 * browser, so it would never see one finish.
 *
 * Module.dopo.trystero is the page's Trystero ({joinRoom, selfId}),
 * Module.dopo.rtc its RTCConfiguration and Module.dopo.relays, if
 * given, the relays to meet through (player.js).
 */
mergeInto(LibraryManager.library, {
  $DOPO_PEER__deps: ['$SOCKFS', '$FS'],
  $DOPO_PEER__postset: 'DOPO_PEER.install();',
  $DOPO_PEER: {
    /* a host that does not show up by then refuses the connection */
    TIMEOUT: 20000,
    next: 1,

    trystero() {
      var dopo = Module['dopo'] || {};
      if (!dopo['trystero'])
        throw new Error('dopo: no Trystero on the page (player.js loads it)');
      return dopo['trystero'];
    },

    config() {
      var dopo = Module['dopo'] || {};
      var config = { appId: dopo['appId'] || 'dopo' };
      if (dopo['rtc'])
        config.rtcConfig = dopo['rtc'];
      if (dopo['relays'])
        config.relayConfig = { urls: dopo['relays'] };
      return config;
    },

    /* a Trystero room and its action (the "net" one by default) */
    join(name, action) {
      var room = DOPO_PEER.trystero()['joinRoom'](DOPO_PEER.config(), name);
      return { room, net: room.makeAction(action || 'net') };
    },

    /* the stream sockets' ops, emscripten's for the others */
    install() {
      var ops = SOCKFS.websocket_sock_ops;
      var base = {};
      ['poll', 'ioctl', 'close', 'connect', 'listen', 'accept', 'sendmsg', 'recvmsg'].forEach((name) => {
        base[name] = ops[name];
        ops[name] = function(sock) {
          if (sock.type !== {{{ cDefs.SOCK_STREAM }}})
            return base[name].apply(this, arguments);
          return DOPO_PEER[name].apply(DOPO_PEER, arguments);
        };
      });
    },

    receive(sock, data) {
      sock.recv_queue.push({ addr: sock.daddr, port: sock.dport, data: new Uint8Array(data) });
    },

    poll(sock) {
      if (sock.server)
        return sock.pending.length ? ({{{ cDefs.POLLRDNORM }}} | {{{ cDefs.POLLIN }}}) : 0;
      if (!sock.dopo)
        return 0;
      var mask = {{{ cDefs.POLLOUT }}}; /* sends are queued until open */
      if (sock.recv_queue.length || sock.dopo.closed)
        mask |= {{{ cDefs.POLLRDNORM }}} | {{{ cDefs.POLLIN }}};
      if (sock.dopo.closed)
        mask |= {{{ cDefs.POLLHUP }}};
      return mask;
    },

    ioctl(sock, request, arg) {
      if (request !== {{{ cDefs.FIONREAD }}})
        return {{{ cDefs.EINVAL }}};
      var bytes = sock.recv_queue.length ? sock.recv_queue[0].data.length : 0;
      {{{ makeSetValue('arg', '0', 'bytes', 'i32') }}};
      return 0;
    },

    close(sock) {
      if (sock.server) {
        sock.server.close();
        sock.server = null;
      }
      if (sock.dopo && !sock.dopo.closed) {
        sock.dopo.closed = true;
        if (sock.dopo.leave) sock.dopo.leave();
      }
      return 0;
    },

    /* to a host by its peer id, the address getaddrinfo gave for it */
    connect(sock, addr, port) {
      if (sock.dopo)
        throw new FS.ErrnoError({{{ cDefs.EISCONN }}});
      var game = DOPO_PEER.join('game-' + addr);
      var dopo = sock.dopo = { open: false, closed: false, out: [], net: game.net };
      var timer = setTimeout(() => {
        if (dopo.open) return;
        sock.error = {{{ cDefs.ECONNREFUSED }}};
        DOPO_PEER.close(sock);
      }, DOPO_PEER.TIMEOUT);
      sock.daddr = addr;
      sock.dport = port;
      dopo.target = addr;
      dopo.leave = () => { clearTimeout(timer); game.room.leave(); };
      game.room.onPeerJoin = (peer) => {
        if (peer !== addr || dopo.closed) return;
        dopo.open = true;
        dopo.out.forEach((data) => game.net.send(data, { target: addr }));
        dopo.out = [];
      };
      game.room.onPeerLeave = (peer) => { if (peer === addr) dopo.closed = true; };
      game.net.onMessage = (data, meta) => {
        if (meta.peerId === addr && !dopo.closed) DOPO_PEER.receive(sock, data);
      };
    },

    /* a host takes players once dopo_peer_host opens its room */
    listen(sock, backlog) {
      if (sock.server)
        throw new FS.ErrnoError({{{ cDefs.EINVAL }}});
      sock.server = { close() { if (this.stop) this.stop(); } };
    },

    accept(sock) {
      if (!sock.server || !sock.pending.length)
        throw new FS.ErrnoError({{{ cDefs.EAGAIN }}});
      var newsock = sock.pending.shift();
      newsock.stream.flags = sock.stream.flags;
      return newsock;
    },

    sendmsg(sock, buffer, offset, length) {
      if (!sock.dopo || sock.dopo.closed)
        throw new FS.ErrnoError({{{ cDefs.ENOTCONN }}});
      var data = new Uint8Array(buffer.buffer || buffer, (buffer.byteOffset || 0) + offset, length).slice();
      if (sock.dopo.open)
        sock.dopo.net.send(data, { target: sock.dopo.target });
      else
        sock.dopo.out.push(data);
      return length;
    },

    recvmsg(sock, length) {
      if (sock.server)
        throw new FS.ErrnoError({{{ cDefs.ENOTCONN }}});
      var queued = sock.recv_queue.shift();
      if (!queued) {
        if (!sock.dopo)
          throw new FS.ErrnoError({{{ cDefs.ENOTCONN }}});
        if (sock.dopo.closed)
          return null;
        throw new FS.ErrnoError({{{ cDefs.EAGAIN }}});
      }
      var bytes = Math.min(length, queued.data.length);
      if (bytes < queued.data.length)
        sock.recv_queue.unshift({ addr: queued.addr, port: queued.port, data: queued.data.subarray(bytes) });
      return { buffer: queued.data.subarray(0, bytes), addr: queued.addr, port: queued.port };
    },

    /* the listening socket on a port, once netplay has made it */
    listener(port) {
      var found = null;
      FS.streams.forEach((stream) => {
        var sock = stream && stream.node && stream.node.sock;
        if (sock && sock.server && sock.sport === port)
          found = sock;
      });
      return found;
    },

    /* the host's room: every player who joins is an accepted socket */
    host(listener, port, room) {
      var game = DOPO_PEER.join('game-' + DOPO_PEER.trystero()['selfId']);
      var players = {};
      var count = () => Object.keys(players).length;

      game.room.onPeerJoin = (peer) => {
        if (!listener.server) return;
        var sock = SOCKFS.createSocket(listener.family, listener.type, listener.protocol);
        var n = DOPO_PEER.next++;
        /* a made-up address of the listener's family, for accept() */
        sock.daddr = listener.family === {{{ cDefs.AF_INET6 }}} ?
          'fd00::' + n.toString(16) :
          '10.' + ((n >> 16) & 255) + '.' + ((n >> 8) & 255) + '.' + (n & 255);
        sock.dport = port;
        sock.dopo = { open: true, closed: false, out: [], net: game.net, target: peer,
                      leave: () => { delete players[peer]; room.update(count()); } };
        players[peer] = sock;
        listener.pending.push(sock);
        room.update(count());
      };
      game.room.onPeerLeave = (peer) => {
        if (players[peer]) players[peer].dopo.closed = true;
        delete players[peer];
        room.update(count());
      };
      game.net.onMessage = (data, meta) => {
        var sock = players[meta.peerId];
        if (sock && !sock.dopo.closed) DOPO_PEER.receive(sock, data);
      };
      listener.server.stop = () => {
        room.stop();
        game.room.leave();
        Object.keys(players).forEach((peer) => { players[peer].dopo.closed = true; });
        players = {};
      };
    }
  },

  /* rooms.c: hosting started, the room opens (and is announced if listed) */
  dopo_peer_host__deps: ['$DOPO_PEER', '$DOPO_ROOMS', '$UTF8ToString'],
  dopo_peer_host: function(port, listed, game, nick, core, version) {
    var fields = {
      game_name: UTF8ToString(game),
      username: UTF8ToString(nick),
      core_name: UTF8ToString(core),
      core_version: UTF8ToString(version),
      port
    };
    var tries = 0;
    var start = () => {
      var listener = DOPO_PEER.listener(port);
      if (!listener) {
        if (++tries < 50) setTimeout(start, 100);
        else console.warn('[dopo] no netplay host on port ' + port);
        return;
      }
      var room = listed ? DOPO_ROOMS.announce(fields) : { update() {}, stop() {} };
      DOPO_PEER.host(listener, port, room);
      console.log('[dopo] hosting as ' + DOPO_PEER.trystero()['selfId'] + (listed ? ', listed' : ''));
    };
    start();
  }
});
