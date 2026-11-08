/**
 * The installed games' cache, one per folder it serves: every file a game
 * page loads is kept, so the game starts offline too. The site's own files
 * (pages, player, engine) come from the network first, the cache only
 * offline, so an update never mixes an old engine with a new player; the
 * game's files (other origins: WADs, soundfonts) come from the cache first
 * and are fetched again behind. Requests with cache: 'no-store' (the
 * netplay lobby's list) are left alone.
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
  if (request.method !== 'GET' || !request.url.startsWith('http') || request.headers.has('range')
      || request.cache === 'no-store')
    return;
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const fresh = fetch(request).then((response) => {
      if (response.ok) cache.put(request, response.clone());
      return response;
    });
    if (new URL(request.url).origin === self.location.origin)
      return fresh.catch(async (e) => (await cache.match(request)) ?? Promise.reject(e));
    const cached = await cache.match(request);
    if (!cached) return fresh;
    event.waitUntil(fresh.catch(() => {}));
    return cached;
  })());
});
