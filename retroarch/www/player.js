/**
 * The web player of every core: loads RetroArch with the core linked in
 * (<core>_libretro.js), the content, and starts it.
 *
 * userdata/ (config, saves, states) lives in IndexedDB, so it outlasts the
 * page; the content does not, it is fetched every time. Netplay is WebRTC
 * between browsers through Trystero (retroarch/source/peer.js, rooms.js):
 * STUN from Google and Cloudflare, and TURN when the page passes a URL
 * that hands some out. Any static server serves the dist folder.
 */

/* Nostr: an announce reaches everyone on its topic at once (BitTorrent
 * trackers hand it to a few peers at random, and most public ones are down) */
const TRYSTERO = 'https://esm.sh/@trystero-p2p/nostr@0.25.4';

const STUN = [{ urls: ['stun:stun.l.google.com:19302', 'stun:stun.cloudflare.com:3478'] }];

/* every core's rooms live under this Trystero app (peer.js, rooms.js) */
const APP_ID = 'dopo';

const HOME = '/home/web_user/retroarch';
const USERDATA = `${HOME}/userdata`;
const CONFIG = `${USERDATA}/retroarch.cfg`;
/* outside userdata: not kept, the options always come from the page */
const CORE_OPTIONS = `${HOME}/core-options.cfg`;

/** retroarch.cfg values for the browser, on top of what the user saved. */
const DEFAULTS = {
  menu_driver: 'rgui',
  pause_nonactive: 'false', /* a netgame stalls when a tab pauses */
  config_save_on_exit: 'true',
  netplay_nat_traversal: 'false',
  netplay_use_mitm_server: 'false',
  savefile_directory: `${USERDATA}/saves`,
  savestate_directory: `${USERDATA}/states`,
  system_directory: `${USERDATA}/system`,
};

function setConfig(text, values) {
  const lines = text ? text.split('\n') : [];
  for (const [key, value] of Object.entries(values)) {
    const line = `${key} = "${String(value).replace(/"/g, '')}"`;
    const at = lines.findIndex((l) => l.split('=')[0].trim() === key);
    if (at >= 0) lines[at] = line;
    else lines.push(line);
  }
  return lines.filter((l) => l.trim()).join('\n') + '\n';
}

async function fetchContent(url, progress) {
  const response = await fetch(url);
  if (!response.ok)
    throw new Error(`${url}: HTTP ${response.status}`);
  const total = Number(response.headers.get('Content-Length')) || 0;
  const reader = response.body.getReader();
  const chunks = [];
  let loaded = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    loaded += value.length;
    progress?.(loaded, total);
  }
  const data = new Uint8Array(loaded);
  let at = 0;
  for (const chunk of chunks) {
    data.set(chunk, at);
    at += chunk.length;
  }
  return data;
}

/** A playlist line as a path under the content folder; one that is
 * absolute or climbs out of it is refused. */
function contentPath(line) {
  const parts = [];
  for (const part of line.replace(/\\/g, '/').split('/')) {
    if (part === '' || part === '.') continue;
    if (part === '..') {
      if (!parts.length) throw new Error(`${line}: outside the playlist's folder`);
      parts.pop();
    } else {
      parts.push(part);
    }
  }
  if (/^[a-z]:/i.test(line) || line.startsWith('/') || !parts.length)
    throw new Error(`${line}: not a path relative to the playlist`);
  return parts.join('/');
}

/**
 * The files of the content, as [{ path, data }], the content first: an
 * m3u playlist with every file it lists, fetched relative to it and kept
 * at the same relative paths, so the core resolves the playlist as on
 * disk; any other content alone.
 */
async function fetchFiles(content, progress) {
  const url = new URL(content, location.href);
  const name = decodeURIComponent(url.pathname.split('/').pop());
  if (!/\.m3u$/i.test(name))
    return [{ path: name, data: await fetchContent(url, progress) }];

  const playlist = await fetchContent(url);
  const lines = new TextDecoder().decode(playlist).split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line && !line.startsWith('#'));
  const loaded = new Map();
  const totals = new Map();
  const sum = (map) => [...map.values()].reduce((a, b) => a + b, 0);
  const files = await Promise.all(lines.map(async (line) => ({
    path: contentPath(line),
    data: await fetchContent(new URL(line, url), (done, total) => {
      loaded.set(line, done);
      totals.set(line, total);
      progress?.(sum(loaded), sum(totals));
    }),
  })));
  return [{ path: name, data: playlist }, ...files];
}

/** TURN servers from url, [] when there are none. */
async function turnServers(url) {
  try {
    const response = await fetch(url);
    if (!response.ok) return [];
    const { iceServers } = await response.json();
    return Array.isArray(iceServers) ? iceServers : iceServers ? [iceServers] : [];
  } catch {
    return [];
  }
}

function sync(FS, populate) {
  return new Promise((resolve) => FS.syncfs(populate, (error) => {
    if (error) console.warn('[player] syncfs:', error);
    resolve();
  }));
}

/**
 * Starts RetroArch on canvas.
 *
 * core:     URL of <core>_libretro.js
 * content:  URL of the content, written under its own file name; an m3u
 *           playlist brings the files it lists, at their relative paths
 * nick:     netplay nickname
 * config:   more retroarch.cfg values
 * options:  core options, { 'prboom-glow': '1' } (default: the ones in
 *           the page's hash, coreOptions())
 * turn:     URL answering {"iceServers": [...]} with TURN servers
 *           (optional; without it, STUN only)
 * progress: (loaded, total) while the content downloads
 */
export async function play({ canvas, core, content, nick, config = {}, options = coreOptions(),
                            turn, progress }) {
  const coreUrl = new URL(core, location.href);
  const [files, { default: factory }, trystero, iceServers] = await Promise.all([
    fetchFiles(content, progress),
    import(coreUrl.href),
    import(TRYSTERO),
    turn ? turnServers(turn) : [],
  ]);

  const module = await factory({
    canvas,
    noInitialRun: true,
    print: (text) => console.log(text),
    printErr: (text) => console.warn(text),
    dopo: {
      trystero: { joinRoom: trystero.joinRoom, selfId: trystero.selfId },
      appId: APP_ID,
      /* the lobby room: rooms of this core only */
      core: coreUrl.pathname.split('/').pop().replace(/_libretro\.js$/, ''),
      rtc: { iceServers: [...STUN, ...iceServers] },
    },
  });

  const { FS } = module;
  for (const dir of [HOME, `${HOME}/content`, USERDATA])
    FS.mkdirTree(dir);
  FS.mount(FS.filesystems.IDBFS, {}, USERDATA);
  await sync(FS, true);
  for (const dir of ['saves', 'states', 'system'])
    FS.mkdirTree(`${USERDATA}/${dir}`);

  const saved = FS.analyzePath(CONFIG).exists ? FS.readFile(CONFIG, { encoding: 'utf8' }) : '';
  FS.writeFile(CONFIG, setConfig(saved, {
    ...DEFAULTS,
    /* one core options file, so the page can set them without knowing
     * the core's library name (config/<name>/<name>.opt) */
    global_core_options: 'true',
    core_options_path: CORE_OPTIONS,
    ...(nick ? { netplay_nickname: nick } : {}),
    ...config,
  }));
  /* only the page's options; the rest take the core's defaults */
  FS.writeFile(CORE_OPTIONS, setConfig('', options));
  for (const { path, data } of files) {
    const file = `${HOME}/content/${path}`;
    FS.mkdirTree(file.slice(0, file.lastIndexOf('/')));
    FS.writeFile(file, data);
  }

  /* userdata back to IndexedDB now and then, and when the page goes */
  const save = () => sync(FS, false);
  setInterval(save, 10000);
  addEventListener('pagehide', save);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') save();
  });

  canvas.focus();
  module.callMain(['-v', `${HOME}/content/${files[0].path}`, '-c', CONFIG]);
  return module;
}

/**
 * Listens to the rooms the hosts of a core announce in its lobby
 * (retroarch/source/rooms.js), without hosting or joining any: a page
 * outside the game can show who is playing. onChange gets the open
 * rooms, [{ peer, game, players }] (game: the content's name, doom1 for
 * doom1.m3u), on every change and as rooms go stale. Returns a function
 * that leaves the lobby.
 */
export async function watchLobby(core, onChange) {
  const STALE = 30000; /* rooms.js: a host repeats its room every 10 s */
  const { joinRoom } = await import(TRYSTERO);
  const lobby = joinRoom({ appId: APP_ID, rtcConfig: { iceServers: STUN } }, `lobby-${core}`);
  const net = lobby.makeAction('room');
  const rooms = new Map();
  const report = () => {
    const now = Date.now();
    for (const [peer, room] of rooms)
      if (now - room.seen > STALE) rooms.delete(peer);
    onChange([...rooms].map(([peer, { fields }]) => ({
      peer,
      game: String(fields.game_name ?? '').split(/[\\/]/).pop(),
      players: Math.max(1, Number(fields.player_count) || 1),
    })));
  };
  net.onMessage = (fields, meta) => {
    if (fields && typeof fields === 'object') rooms.set(meta.peerId, { fields, seen: Date.now() });
    else rooms.delete(meta.peerId);
    report();
  };
  lobby.onPeerLeave = (peer) => {
    if (rooms.delete(peer)) report();
  };
  const timer = setInterval(report, 5000);
  report();
  return () => {
    clearInterval(timer);
    lobby.leave();
  };
}

/** Core options from a text file at url, one key=value a line ('#' starts
 * a comment): { 'prboom-glow': '2' }. */
export async function loadOptions(url) {
  const response = await fetch(url);
  if (!response.ok)
    throw new Error(`${url}: HTTP ${response.status}`);
  const options = {};
  for (const line of (await response.text()).split(/\r?\n/)) {
    const text = line.trim();
    const at = text.indexOf('=');
    if (!text || text.startsWith('#') || at < 1) continue;
    options[text.slice(0, at).trim()] = text.slice(at + 1).trim();
  }
  return options;
}

/** The page's hash as parameters: #nick=rodrigo */
export function hashParams() {
  return new URLSearchParams(location.hash.slice(1));
}

/** The core options in the page's hash, the keys with a dash (libretro
 * option keys are <core>-<option>): #prboom-glow=1&prboom-dynlight=2 */
export function coreOptions() {
  const options = {};
  for (const [key, value] of hashParams())
    if (key.includes('-'))
      options[key] = value;
  return options;
}
