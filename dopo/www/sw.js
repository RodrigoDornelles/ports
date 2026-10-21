/**
 * The installed games' cache, one per folder it serves: every file a game
 * page loads (the page, the engine, the game's files, Trystero for
 * multiplayer) is kept, so the next start comes from this device. A cached
 * file is answered at once and fetched again behind it: an update arrives
 * whole on the start after, never a new core.js with an old core.wasm.
 */
const CACHE = self.registration.scope;

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

/* what a page loaded before this worker took it over, the first time:
 * the page, its scripts and the core */
self.addEventListener('message', (event) => {
  const urls = event.data?.cache;
  if (!Array.isArray(urls)) return;
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE);
    for (const url of urls) {
      if (await cache.match(url)) continue;
      await cache.add(url).catch(() => {});
    }
  })());
});

self.addEventListener('fetch', (event) => {
  const { request } = event;
  if (request.method !== 'GET' || !request.url.startsWith('http') || request.headers.has('range'))
    return;
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const cached = await cache.match(request);
    const fresh = fetch(request).then((response) => {
      if (response.ok) cache.put(request, response.clone());
      return response;
    });
    if (!cached) return fresh;
    event.waitUntil(fresh.catch(() => {}));
    return cached;
  })());
});
