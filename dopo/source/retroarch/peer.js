/**
 * @brief Netplay's TCP sockets over WebRTC, so a browser plays against
 * browsers: emscripten's own sockets (SOCKFS) only reach WebSocket servers
 * and cannot listen at all.
 *
 * A host is reached by its id (signal.js), the address its room has in
 * the lobby (rooms.js): it waits on its topic, and a player connecting to
 * that id meets it there and opens one RTCPeerConnection with one ordered,
 * reliable data channel to it, a byte stream as TCP is: sends go in
 * pieces of CHUNK bytes, held back while the channel's buffer is full, and
 * come out in order on the other side. Stream sockets take these ops; the
 * others keep emscripten's.
 *
 * The meeting: the player knocks with its public key until the host
 * welcomes it with its own, then sends its offer sealed, the host answers
 * sealed; each side gathers its ICE candidates whole before sending (no
 * trickle), so two messages carry the whole handshake.
 *
 * connect() succeeds at once and sends wait for the host: RetroArch waits
 * for a connect by polling with a timeout, which cannot block in the
 * browser, so it would never see one finish.
 *
 * Module.dopo.rtc is the RTCConfiguration (player.js).
 */
mergeInto(LibraryManager.library, {
  $DOPO_PEER__deps: ['$SOCKFS', '$FS', '$DOPO_SIGNAL'],
  $DOPO_PEER__postset: 'DOPO_PEER.install();',
  $DOPO_PEER: {
    /* a host that does not show up by then refuses the connection */
    TIMEOUT: 20000,
    /* the knock again until the host answers, ICE gathering at most */
    KNOCK: 2000,
    GATHER: 3000,
    /* data channel pieces, and how much may wait in its buffer */
    CHUNK: 16384,
    BUFFERED: 1048576,
    next: 1,

    rtc() {
      return (Module['dopo'] || {})['rtc'] || {};
    },

    /* once every ICE candidate is in the description, or after GATHER */
    gathered(pc) {
      if (pc.iceGatheringState === 'complete')
        return Promise.resolve();
      return new Promise((resolve) => {
        var timer = setTimeout(done, DOPO_PEER.GATHER);
        function done() {
          clearTimeout(timer);
          pc.removeEventListener('icegatheringstatechange', check);
          resolve();
        }
        function check() {
          if (pc.iceGatheringState === 'complete') done();
        }
        pc.addEventListener('icegatheringstatechange', check);
      });
    },

    /* a channel as a byte stream: on.open(), on.data(bytes), on.close()
     * once; returns { send(bytes), close() } */
    link(pc, channel, on) {
      var queue = [];
      var opened = false;
      var ended = false;
      var pump = () => {
        while (queue.length && channel.readyState === 'open' && channel.bufferedAmount < DOPO_PEER.BUFFERED)
          channel.send(queue.shift());
      };
      var end = () => {
        if (ended) return;
        ended = true;
        queue = [];
        try { channel.close(); } catch (e) {}
        try { pc.close(); } catch (e) {}
        on.close();
      };
      channel.binaryType = 'arraybuffer';
      channel.bufferedAmountLowThreshold = DOPO_PEER.CHUNK * 16;
      channel.onbufferedamountlow = pump;
      channel.onopen = () => {
        if (opened || ended) return;
        opened = true;
        pump();
        on.open();
      };
      channel.onmessage = (event) => { if (!ended) on.data(new Uint8Array(event.data)); };
      channel.onclose = end;
      pc.addEventListener('connectionstatechange', () => {
        if (pc.connectionState === 'failed' || pc.connectionState === 'closed') end();
      });
      if (channel.readyState === 'open')
        setTimeout(channel.onopen);
      return {
        send(bytes) {
          for (var at = 0; at < bytes.length; at += DOPO_PEER.CHUNK)
            queue.push(bytes.slice(at, at + DOPO_PEER.CHUNK));  /* its own buffer */
          pump();
        },
        close: end
      };
    },

    /* to the host whose id is given; returns { send(bytes), close() } */
    dial(host, on) {
      var me = DOPO_SIGNAL.id();
      var topic = 'dopo:' + host;
      var pc = new RTCPeerConnection(DOPO_PEER.rtc());
      var link = DOPO_PEER.link(pc, pc.createDataChannel('net', { ordered: true }), on);
      var key = null;
      var answered = false;
      var knocking = 0;
      var stop = DOPO_SIGNAL.join(topic, async (message) => {
        if (message.to !== me || message.from !== host) return;
        try {
          if (message.t === 'welcome' && !key) {
            clearInterval(knocking);
            key = await DOPO_SIGNAL.shared(message.pub);
            await pc.setLocalDescription(await pc.createOffer());
            await DOPO_PEER.gathered(pc);
            DOPO_SIGNAL.send(topic, Object.assign({ t: 'offer', from: me, to: host },
              await DOPO_SIGNAL.seal(key, pc.localDescription.toJSON())));
          } else if (message.t === 'answer' && key && !answered) {
            answered = true;
            await pc.setRemoteDescription(await DOPO_SIGNAL.open(key, message));
            stop();
          }
        } catch (e) {
          console.warn('[dopo] dial ' + host + ': ' + e.message);
          link.close();
        }
      });
      var knock = () => DOPO_SIGNAL.publicKey().then((pub) =>
        DOPO_SIGNAL.send(topic, { t: 'knock', from: me, to: host, pub }));
      knock();
      knocking = setInterval(knock, DOPO_PEER.KNOCK);
      return {
        send: link.send,
        close() {
          clearInterval(knocking);
          stop();
          link.close();
        }
      };
    },

    /* players dialing this page: on.join(id, link) when one's channel opens,
     * returning { data(bytes), close() }; returns { stop() } */
    serve(on) {
      var me = DOPO_SIGNAL.id();
      var topic = 'dopo:' + me;
      var peers = {};
      var forget = (id, peer) => { if (peers[id] === peer) delete peers[id]; };
      var stop = DOPO_SIGNAL.join(topic, async (message) => {
        var id = message.from;
        if (message.to !== me || typeof id !== 'string') return;
        var peer = peers[id];
        try {
          if (message.t === 'knock') {
            if (peer && peer.pc && peer.pc.connectionState !== 'new' && peer.pc.connectionState !== 'connecting')
              return;  /* a knock repeated while the last one gets through */
            if (!peer)
              peer = peers[id] = { key: DOPO_SIGNAL.shared(message.pub) };
            DOPO_SIGNAL.send(topic, { t: 'welcome', from: me, to: id, pub: await DOPO_SIGNAL.publicKey() });
          } else if (message.t === 'offer' && peer && !peer.pc) {
            var key = await peer.key;
            var pc = peer.pc = new RTCPeerConnection(DOPO_PEER.rtc());
            pc.addEventListener('connectionstatechange', () => {
              if (pc.connectionState === 'failed' || pc.connectionState === 'closed') forget(id, peer);
            });
            pc.ondatachannel = (event) => {
              var player = null;
              var link = DOPO_PEER.link(pc, event.channel, {
                open() { player = on.join(id, link); },
                data(bytes) { if (player) player.data(bytes); },
                close() {
                  forget(id, peer);
                  if (player) player.close();
                }
              });
              peer.link = link;
            };
            await pc.setRemoteDescription(await DOPO_SIGNAL.open(key, message));
            await pc.setLocalDescription(await pc.createAnswer());
            await DOPO_PEER.gathered(pc);
            DOPO_SIGNAL.send(topic, Object.assign({ t: 'answer', from: me, to: id },
              await DOPO_SIGNAL.seal(key, pc.localDescription.toJSON())));
          }
        } catch (e) {
          console.warn('[dopo] serve ' + id + ': ' + e.message);
          forget(id, peer);
          if (peer && peer.pc) peer.pc.close();
        }
      });
      return {
        stop() {
          stop();
          Object.keys(peers).forEach((id) => {
            var peer = peers[id];
            if (peer.link) peer.link.close();
            else if (peer.pc) peer.pc.close();
          });
          peers = {};
        }
      };
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
      var dopo = sock.dopo = { open: false, closed: false, out: [], send: null };
      var timer = setTimeout(() => {
        if (dopo.open) return;
        sock.error = {{{ cDefs.ECONNREFUSED }}};
        DOPO_PEER.close(sock);
      }, DOPO_PEER.TIMEOUT);
      sock.daddr = addr;
      sock.dport = port;
      dopo.target = addr;
      var link;
      try {
        link = DOPO_PEER.dial(addr, {
          open() {
            if (dopo.closed) return;
            clearTimeout(timer);
            dopo.open = true;
            dopo.send = link.send;
            dopo.out.forEach(link.send);
            dopo.out = [];
          },
          data(bytes) { if (!dopo.closed) DOPO_PEER.receive(sock, bytes); },
          close() { dopo.closed = true; }
        });
      } catch (e) {
        console.warn('[dopo] ' + e.message);
        clearTimeout(timer);
        sock.error = {{{ cDefs.ECONNREFUSED }}};
        dopo.closed = true;
        return;
      }
      dopo.leave = () => { clearTimeout(timer); link.close(); };
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
        sock.dopo.send(data);
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
      var players = {};
      var count = () => Object.keys(players).length;
      var hosting = DOPO_PEER.serve({
        join(peer, link) {
          if (!listener.server) {
            link.close();
            return null;
          }
          var sock = SOCKFS.createSocket(listener.family, listener.type, listener.protocol);
          var n = DOPO_PEER.next++;
          /* a made-up address of the listener's family, for accept() */
          sock.daddr = listener.family === {{{ cDefs.AF_INET6 }}} ?
            'fd00::' + n.toString(16) :
            '10.' + ((n >> 16) & 255) + '.' + ((n >> 8) & 255) + '.' + (n & 255);
          sock.dport = port;
          var gone = () => {
            if (players[peer] !== sock) return;
            delete players[peer];
            room.update(count());
          };
          sock.dopo = { open: true, closed: false, out: [], send: link.send, target: peer,
                        leave: () => { gone(); link.close(); } };
          players[peer] = sock;
          listener.pending.push(sock);
          room.update(count());
          return {
            data(bytes) { if (!sock.dopo.closed) DOPO_PEER.receive(sock, bytes); },
            close() { sock.dopo.closed = true; gone(); }
          };
        }
      });
      listener.server.stop = () => {
        room.stop();
        hosting.stop();
        Object.keys(players).forEach((peer) => { players[peer].dopo.closed = true; });
        players = {};
      };
    }
  },

  /* rooms.c: hosting started, the room opens (and is announced if listed) */
  dopo_peer_host__deps: ['$DOPO_PEER', '$DOPO_ROOMS', '$DOPO_SIGNAL', '$UTF8ToString'],
  dopo_peer_host: function(port, listed, game, nick, core, version) {
    var fields = {
      game_name: UTF8ToString(game),
      username: UTF8ToString(nick).trim() || 'Player',
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
      try {
        DOPO_PEER.host(listener, port, room);
      } catch (e) {
        console.warn('[dopo] ' + e.message);
        room.stop();
        return;
      }
      console.log('[dopo] hosting as ' + DOPO_SIGNAL.id() + (listed ? ', listed' : ''));
    };
    start();
  }
});
