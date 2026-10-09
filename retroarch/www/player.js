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

const HOME = '/home/web_user/retroarch';
const USERDATA = `${HOME}/userdata`;
const CONFIG = `${USERDATA}/retroarch.cfg`;

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
 * content:  URL of the content, written under its own file name
 * nick:     netplay nickname
 * config:   more retroarch.cfg values
 * turn:     URL answering {"iceServers": [...]} with TURN servers
 *           (optional; without it, STUN only)
 * progress: (loaded, total) while the content downloads
 */
export async function play({ canvas, core, content, nick, config = {}, turn, progress }) {
  const name = decodeURIComponent(new URL(content, location.href).pathname.split('/').pop());
  const coreUrl = new URL(core, location.href);
  const [data, { default: factory }, trystero, iceServers] = await Promise.all([
    fetchContent(content, progress),
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
      appId: 'dopo',
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
    ...(nick ? { netplay_nickname: nick } : {}),
    ...config,
  }));
  FS.writeFile(`${HOME}/content/${name}`, data);

  /* userdata back to IndexedDB now and then, and when the page goes */
  const save = () => sync(FS, false);
  setInterval(save, 10000);
  addEventListener('pagehide', save);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') save();
  });

  canvas.focus();
  module.callMain(['-v', `${HOME}/content/${name}`, '-c', CONFIG]);
  return module;
}

/** The page's hash as parameters: #nick=rodrigo */
export function hashParams() {
  return new URLSearchParams(location.hash.slice(1));
}
