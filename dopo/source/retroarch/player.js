/**
 * The web player of every core: loads RetroArch with the core linked in
 * (<core>_libretro.js), the content, and starts it.
 *
 * userdata/ (config, saves, states) lives in IndexedDB, so it outlasts the
 * page; the content does not, it is fetched every time. Netplay is WebRTC
 * between browsers (peer.js): the rooms are listed by the lobby of a
 * Supabase project, which answers as the libretro lobby does (rooms.js,
 * dopo/supabase), and the players meet on its Realtime (signal.js); STUN
 * from Google and Cloudflare, and TURN when the page passes a URL that
 * hands some out. Any static server serves the dist folder.
 */

const STUN = [{ urls: ['stun:stun.l.google.com:19302', 'stun:stun.cloudflare.com:3478'] }];

/* dopo/supabase/lobby.json, copied next to this file by the build */
const LOBBY = new URL('lobby.json', import.meta.url);

let lobbyProject = null;
/** The lobby's project, { url, key }, or null without one (no netplay). */
export function lobbyConfig() {
  lobbyProject ??= fetch(LOBBY).then((response) => response.ok ? response.json() : null)
    .then((lobby) => (lobby?.project && lobby?.key
      ? { url: `https://${lobby.project}.supabase.co`, key: lobby.key } : null))
    .catch(() => null);
  return lobbyProject;
}

const HOME = '/home/web_user/retroarch';
const USERDATA = `${HOME}/userdata`;
const CONFIG = `${USERDATA}/retroarch.cfg`;
/* outside userdata: not kept, the options always come from the page */
const CORE_OPTIONS = `${HOME}/core-options.cfg`;

/* RetroArch's keyboard hotkeys (configuration.c), all off: their keys are
 * the game's (Esc, space, F2...) */
const HOTKEYS = [
  'ai_service', 'audio_mute', 'cheat_index_minus', 'cheat_index_plus',
  'cheat_toggle', 'close_content', 'desktop_menu_toggle',
  'disk_eject_toggle', 'disk_next', 'disk_prev', 'enable_hotkey',
  'exit_emulator', 'fps_toggle', 'frame_advance', 'game_focus_toggle',
  'grab_mouse_toggle', 'halt_replay', 'hold_fast_forward', 'hold_slowmotion',
  'load_state', 'menu_toggle', 'netplay_fade_chat_toggle',
  'netplay_game_watch', 'netplay_host_toggle', 'netplay_ping_toggle',
  'netplay_player_chat', 'next_replay_checkpoint', 'osk_toggle',
  'overlay_next', 'pause_toggle', 'play_replay', 'preempt_toggle',
  'prev_replay_checkpoint', 'recording_toggle', 'record_replay',
  'replay_slot_decrease', 'replay_slot_increase', 'reset', 'rewind',
  'runahead_toggle', 'save_replay_checkpoint', 'save_state', 'screenshot',
  'shader_hold', 'shader_next', 'shader_prev', 'shader_toggle',
  'state_slot_decrease', 'state_slot_increase', 'streaming_toggle',
  'toggle_fast_forward', 'toggle_fullscreen', 'toggle_slowmotion',
  'toggle_statistics', 'toggle_vrr_runloop', 'turbo_fire_toggle',
  'volume_down', 'volume_up',
];

/** retroarch.cfg values for the browser, on top of what the user saved. */
const DEFAULTS = {
  menu_driver: 'rgui',
  pause_nonactive: 'false', /* a netgame stalls when a tab pauses */
  video_font_enable: 'false', /* RetroArch's notifications: the core has its own */
  config_save_on_exit: 'true',
  netplay_nat_traversal: 'false',
  netplay_use_mitm_server: 'false',
  savefile_directory: `${USERDATA}/saves`,
  savestate_directory: `${USERDATA}/states`,
  system_directory: `${USERDATA}/system`,
  ...Object.fromEntries(HOTKEYS.map((name) => [`input_${name}`, 'nul'])),
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

/** The bytes at url, progress(loaded, total) as they come; total is 0
 * when the server does not say. */
export async function fetchContent(url, progress) {
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
 * disk; any other content alone. The URLs of more go next to the content
 * under their own names and, in a playlist, are listed at its end.
 */
async function fetchFiles(content, more, progress) {
  const url = new URL(content, location.href);
  const name = decodeURIComponent(url.pathname.split('/').pop());
  const m3u = /\.m3u$/i.test(name);
  const entries = [];
  let playlist = null;
  if (m3u) {
    playlist = new TextDecoder().decode(await fetchContent(url));
    for (const line of playlist.split(/\r?\n/).map((l) => l.trim()))
      if (line && !line.startsWith('#'))
        entries.push({ path: contentPath(line), url: new URL(line, url) });
  } else {
    entries.push({ path: name, url });
  }
  for (const extra of more) {
    const at = new URL(extra, location.href);
    const path = contentPath(decodeURIComponent(at.pathname.split('/').pop()));
    if (entries.some((entry) => entry.path === path))
      throw new Error(`${path}: already in the content`);
    entries.push({ path, url: at });
    if (m3u) playlist = playlist.replace(/\s*$/, '\n') + path + '\n';
  }

  const loaded = new Map();
  const totals = new Map();
  const sum = (map) => [...map.values()].reduce((a, b) => a + b, 0);
  const files = await Promise.all(entries.map(async ({ path, url }) => ({
    path,
    data: await fetchContent(url, (done, total) => {
      loaded.set(path, done);
      totals.set(path, total);
      progress?.(sum(loaded), sum(totals));
    }),
  })));
  if (!m3u) return files;
  return [{ path: name, data: new TextEncoder().encode(playlist) }, ...files];
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
 * playlist: URLs of more files, written next to the content under their
 *           own names and added to the end of an m3u content (optional)
 * nick:     netplay nickname
 * config:   more retroarch.cfg values
 * options:  core options, { 'prboom-glow': '1' } (default: the ones in
 *           the page's hash, coreOptions())
 * turn:     URL answering {"iceServers": [...]} with TURN servers
 *           (optional; without it, STUN only)
 * progress: (loaded, total) while the content downloads
 */
export async function play({ canvas, core, content, playlist = [], nick, config = {},
                            options = coreOptions(), turn, progress }) {
  const coreUrl = new URL(core, location.href);
  const [files, { default: factory }, lobby, iceServers] = await Promise.all([
    fetchFiles(content, playlist, progress),
    import(coreUrl.href),
    lobbyConfig(),
    turn ? turnServers(turn) : [],
  ]);
  if (!lobby) console.warn('[player] no lobby.json: netplay has no lobby');

  const module = await factory({
    canvas,
    noInitialRun: true,
    print: (text) => console.log(text),
    printErr: (text) => console.warn(text),
    dopo: {
      lobby,
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
    netplay_nickname: String(nick ?? '').trim() || 'Player',
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
 * The rooms of a core in the lobby (rooms.js), asked again every few
 * seconds, without hosting or joining any: a page outside the game can
 * show who is playing. core is the core_name hosts announce (any case:
 * prboomtv for PrBoomTV). onChange gets the open rooms, [{ peer, game,
 * players }] (game: the content's name, doom1 for doom1.m3u), each time
 * the list comes. Returns a function that stops asking; throws when the
 * build has no lobby.
 */
export async function watchLobby(core, onChange) {
  const EVERY = 10000;
  const lobby = await lobbyConfig();
  if (!lobby) throw new Error('no lobby.json');
  const url = `${lobby.url.replace(/\/+$/, '')}/functions/v1/lobby/list`;
  let timer = 0;
  let stopped = false;
  const ask = async () => {
    try {
      const response = await fetch(url, { cache: 'no-store' });
      if (!response.ok) throw new Error(`lobby: list answered ${response.status}`);
      const rooms = (await response.json())
        .map((room) => room?.fields ?? {})
        .filter((fields) => String(fields.core_name ?? '').toLowerCase() === core.toLowerCase());
      if (!stopped) onChange(rooms.map((fields) => ({
        peer: String(fields.ip ?? ''),
        game: String(fields.game_name ?? '').split(/[\\/]/).pop(),
        players: Math.max(1, Number(fields.player_count) || 1),
      })));
    } catch (e) {
      console.warn('[player]', e.message ?? e);
    }
    if (!stopped) timer = setTimeout(ask, EVERY);
  };
  await ask();
  return () => {
    stopped = true;
    clearTimeout(timer);
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
