/**
 * @brief The handshake of the WebRTC channels (peer.js) over Supabase
 * Realtime, without its SDK: one WebSocket per page (Phoenix protocol,
 * vsn 1.0.0), a broadcast topic per host ("dopo:<host id>") that the host
 * and whoever dials it join, and messages that say who they are from and
 * to. The host's id is what the lobby lists as the room's address.
 *
 * The offer and the answer carry the players' addresses, and anyone may
 * listen on a public topic, so they go sealed: each page has an ECDH key
 * (P-256), the two sides swap the public halves first (knock, welcome)
 * and seal the rest with AES-GCM under the key they share.
 *
 * Module.dopo.lobby is the project: { url, key }, its URL and anon key
 * (player.js reads them from lobby.json).
 */
mergeInto(LibraryManager.library, {
  $DOPO_SIGNAL: {
    HEARTBEAT: 25000,
    RECONNECT: 2000,

    ws: null,
    beat: 0,
    ref: 0,
    topics: {},  /* "realtime:<topic>" -> { handlers, joined, joinRef, queue } */
    self: null,
    pair: null,

    /* this page's id: what a host is listed and dialed by */
    id() {
      if (!DOPO_SIGNAL.self) {
        var bytes = crypto.getRandomValues(new Uint8Array(16));
        DOPO_SIGNAL.self = Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
      }
      return DOPO_SIGNAL.self;
    },

    config() {
      var lobby = (Module['dopo'] || {})['lobby'];
      if (!lobby || !lobby['url'] || !lobby['key'])
        throw new Error('dopo: no lobby project on the page (lobby.json)');
      return { url: String(lobby['url']).replace(/\/+$/, ''), key: String(lobby['key']) };
    },

    next() {
      return String(++DOPO_SIGNAL.ref);
    },

    raw(message) {
      var ws = DOPO_SIGNAL.ws;
      if (ws && ws.readyState === 1)
        ws.send(JSON.stringify(message));
    },

    socket() {
      if (DOPO_SIGNAL.ws)
        return DOPO_SIGNAL.ws;
      var config = DOPO_SIGNAL.config();
      var ws = DOPO_SIGNAL.ws = new WebSocket(config.url.replace(/^http/, 'ws') +
        '/realtime/v1/websocket?apikey=' + encodeURIComponent(config.key) + '&vsn=1.0.0');
      ws.onopen = () => {
        Object.keys(DOPO_SIGNAL.topics).forEach(DOPO_SIGNAL.subscribe);
        DOPO_SIGNAL.beat = setInterval(() => DOPO_SIGNAL.raw({
          topic: 'phoenix', event: 'heartbeat', payload: {}, ref: DOPO_SIGNAL.next()
        }), DOPO_SIGNAL.HEARTBEAT);
      };
      ws.onmessage = (event) => {
        var message;
        try { message = JSON.parse(event.data); } catch (e) { return; }
        var topic = DOPO_SIGNAL.topics[message.topic];
        if (!topic) return;
        if (message.event === 'phx_reply' && message.ref === topic.joinRef) {
          if (message.payload && message.payload.status === 'ok') {
            topic.joined = true;
            topic.queue.splice(0).forEach((queued) => {
              queued.join_ref = topic.joinRef;
              DOPO_SIGNAL.raw(queued);
            });
          } else {
            console.warn('[dopo] signal: ' + message.topic + ' refused', message.payload);
          }
        } else if (message.event === 'broadcast' && message.payload && message.payload.event === 'dopo') {
          topic.handlers.slice().forEach((handler) => handler(message.payload.payload || {}));
        } else if (message.event === 'phx_error' || message.event === 'phx_close') {
          topic.joined = false;
        }
      };
      ws.onclose = () => {
        clearInterval(DOPO_SIGNAL.beat);
        DOPO_SIGNAL.ws = null;
        Object.keys(DOPO_SIGNAL.topics).forEach((name) => { DOPO_SIGNAL.topics[name].joined = false; });
        setTimeout(() => {
          if (!DOPO_SIGNAL.ws && Object.keys(DOPO_SIGNAL.topics).length)
            DOPO_SIGNAL.socket();
        }, DOPO_SIGNAL.RECONNECT);
      };
      return ws;
    },

    subscribe(name) {
      var topic = DOPO_SIGNAL.topics[name];
      topic.joinRef = DOPO_SIGNAL.next();
      DOPO_SIGNAL.raw({
        topic: name, event: 'phx_join', ref: topic.joinRef, join_ref: topic.joinRef,
        payload: { config: { broadcast: { self: false, ack: false }, presence: { key: '' }, private: false } }
      });
    },

    /* handler(message) for what comes on topic; returns a function that
     * stops it, the last one leaving the topic */
    join(topic, handler) {
      var name = 'realtime:' + topic;
      var entry = DOPO_SIGNAL.topics[name];
      if (!entry) {
        entry = DOPO_SIGNAL.topics[name] = { handlers: [], joined: false, joinRef: null, queue: [] };
        var ws = DOPO_SIGNAL.socket();
        if (ws.readyState === 1)
          DOPO_SIGNAL.subscribe(name);
      }
      entry.handlers.push(handler);
      return () => {
        var at = entry.handlers.indexOf(handler);
        if (at >= 0) entry.handlers.splice(at, 1);
        if (entry.handlers.length || DOPO_SIGNAL.topics[name] !== entry) return;
        delete DOPO_SIGNAL.topics[name];
        if (entry.joined)
          DOPO_SIGNAL.raw({ topic: name, event: 'phx_leave', payload: {},
                            ref: DOPO_SIGNAL.next(), join_ref: entry.joinRef });
      };
    },

    /* to everyone else on a topic this page joined */
    send(topic, message) {
      var name = 'realtime:' + topic;
      var entry = DOPO_SIGNAL.topics[name];
      if (!entry) return;
      var out = { topic: name, event: 'broadcast', ref: DOPO_SIGNAL.next(),
                  payload: { type: 'broadcast', event: 'dopo', payload: message } };
      if (entry.joined) {
        out.join_ref = entry.joinRef;
        DOPO_SIGNAL.raw(out);
      } else {
        entry.queue.push(out);
      }
    },

    /* ---- sealing ---- */

    b64(bytes) {
      var text = '';
      bytes.forEach((b) => { text += String.fromCharCode(b); });
      return btoa(text);
    },

    unb64(text) {
      return Uint8Array.from(atob(String(text)), (c) => c.charCodeAt(0));
    },

    keys() {
      if (!DOPO_SIGNAL.pair)
        DOPO_SIGNAL.pair = crypto.subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' },
                                                     false, ['deriveKey']);
      return DOPO_SIGNAL.pair;
    },

    publicKey: async function() {
      var pair = await DOPO_SIGNAL.keys();
      return DOPO_SIGNAL.b64(new Uint8Array(await crypto.subtle.exportKey('raw', pair.publicKey)));
    },

    /* the key this page shares with the one whose public half is given */
    shared: async function(publicKey) {
      var pair = await DOPO_SIGNAL.keys();
      var other = await crypto.subtle.importKey('raw', DOPO_SIGNAL.unb64(publicKey),
                                                { name: 'ECDH', namedCurve: 'P-256' }, false, []);
      return crypto.subtle.deriveKey({ name: 'ECDH', public: other }, pair.privateKey,
                                     { name: 'AES-GCM', length: 256 }, false, ['encrypt', 'decrypt']);
    },

    seal: async function(key, value) {
      var iv = crypto.getRandomValues(new Uint8Array(12));
      var data = await crypto.subtle.encrypt({ name: 'AES-GCM', iv }, key,
                                             new TextEncoder().encode(JSON.stringify(value)));
      return { iv: DOPO_SIGNAL.b64(iv), data: DOPO_SIGNAL.b64(new Uint8Array(data)) };
    },

    open: async function(key, sealed) {
      var data = await crypto.subtle.decrypt({ name: 'AES-GCM', iv: DOPO_SIGNAL.unb64(sealed.iv) }, key,
                                             DOPO_SIGNAL.unb64(sealed.data));
      return JSON.parse(new TextDecoder().decode(data));
    }
  }
});
