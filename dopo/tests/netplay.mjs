/**
 * The web netplay's transport (dopo/source/retroarch/signal.js and
 * peer.js) between two pages in Node: a stand-in for Supabase Realtime
 * (Phoenix join and broadcast over a WebSocket) and real WebRTC from
 * node-datachannel. A player dials a host, sends 3 MB in two writes, the
 * host checks them whole and in order, answers, and the player leaves.
 * Also fails when an SDP crosses the stand-in unsealed.
 *
 * npm test; with --real (npm run test:lobby), over the project of
 * dopo/supabase/lobby.json instead of the stand-in.
 */
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { WebSocketServer } from 'ws';
import { RTCPeerConnection } from 'node-datachannel/polyfill';

globalThis.RTCPeerConnection = RTCPeerConnection;
const LIBRARIES = fileURLToPath(new URL('../source/retroarch/', import.meta.url));
const REAL = process.argv.includes('--real');
const failures = [];
const fail = (text) => { failures.push(text); console.error(`FAIL ${text}`); };

/* Realtime as signal.js speaks it: join, reply, heartbeat, broadcast to
 * the other sockets on the topic */
const server = REAL ? null : new WebSocketServer({ port: 0, host: '127.0.0.1' });
if (server) {
  const topics = new Map();
  server.on('connection', (ws, request) => {
    if (!request.url.startsWith('/realtime/v1/websocket?apikey=anon&vsn=1.0.0')) {
      fail(`connected to ${request.url}`);
      ws.close();
      return;
    }
    ws.on('message', (raw) => {
      const message = JSON.parse(raw);
      const reply = (payload) => ws.send(JSON.stringify({ topic: message.topic, event: 'phx_reply',
        payload, ref: message.ref, join_ref: message.join_ref }));
      if (message.event === 'phx_join') {
        if (!topics.has(message.topic)) topics.set(message.topic, new Set());
        topics.get(message.topic).add(ws);
        reply({ status: 'ok', response: {} });
      } else if (message.event === 'heartbeat') {
        reply({ status: 'ok', response: {} });
      } else if (message.event === 'phx_leave') {
        topics.get(message.topic)?.delete(ws);
      } else if (message.event === 'broadcast') {
        if (JSON.stringify(message.payload).includes('v=0'))
          fail('an SDP went over Realtime unsealed');
        for (const other of topics.get(message.topic) ?? [])
          if (other !== ws)
            other.send(JSON.stringify({ topic: message.topic, event: 'broadcast', payload: message.payload, ref: null }));
      }
    });
  });
}
let lobby;
if (REAL) {
  const project = JSON.parse(readFileSync(new URL('../supabase/lobby.json', import.meta.url), 'utf8'));
  lobby = { url: `https://${project.project}.supabase.co`, key: project.key };
} else {
  await new Promise((resolve) => server.on('listening', resolve));
  lobby = { url: `http://127.0.0.1:${server.address().port}`, key: 'anon' };
}

/* a page: the libraries as emscripten links them, in a scope of their own */
function page() {
  const Module = { dopo: { lobby, rtc: { iceServers: [] } } };
  const source = ['signal.js', 'peer.js']
    .map((file) => readFileSync(LIBRARIES + file, 'utf8').replace(/\{\{\{[^}]*\}\}\}/g, '0'))
    .join('\n');
  return new Function('Module', `
    const library = {};
    const LibraryManager = { library: {} };
    function mergeInto(_, entries) { Object.assign(library, entries); }
    let DOPO_SIGNAL, DOPO_PEER;
    ${source}
    DOPO_SIGNAL = library.$DOPO_SIGNAL;
    DOPO_PEER = library.$DOPO_PEER;
    return { DOPO_SIGNAL, DOPO_PEER };`)(Module);
}

const host = page();
const player = page();
const SIZE = 3 * 1024 * 1024 + 123;
const sent = new Uint8Array(SIZE).map((_, i) => (i * 7 + (i >> 9)) & 255);
const sha1 = (bytes) => createHash('sha1').update(bytes).digest('hex');
const started = Date.now();
const steps = [];

await new Promise((resolve) => {
  const timer = setTimeout(() => { fail('timed out'); resolve(); }, 30000);
  const pieces = [];
  let received = 0;
  let joins = 0;
  let answer = '';
  const hosting = host.DOPO_PEER.serve({
    join(id, link) {
      joins++;
      steps.push(`joined in ${Date.now() - started} ms`);
      if (id !== player.DOPO_SIGNAL.id()) fail('the host saw another id');
      return {
        data(bytes) {
          pieces.push(bytes);
          received += bytes.length;
          if (received < SIZE) return;
          if (received > SIZE) fail(`the host got ${received} bytes`);
          else if (sha1(Buffer.concat(pieces)) !== sha1(sent)) fail('the bytes came changed or out of order');
          else steps.push(`${SIZE} bytes whole in ${pieces.length} pieces`);
          link.send(new TextEncoder().encode('thanks'));
        },
        close() {
          steps.push('the host saw the player leave');
          if (joins !== 1) fail(`the player joined ${joins} times`);
          hosting.stop();
          clearTimeout(timer);
          resolve();
        }
      };
    }
  });
  const call = player.DOPO_PEER.dial(host.DOPO_SIGNAL.id(), {
    open() {
      call.send(sent.subarray(0, 1000));
      call.send(sent.subarray(1000));
    },
    data(bytes) {
      answer += new TextDecoder().decode(bytes);
      if (answer === 'thanks') {
        steps.push('the host answered');
        call.close();
      }
    },
    close() {}
  });
});

for (const step of steps) console.log(`ok ${step}`);
console.log(failures.length ? `${failures.length} failed` : 'netplay transport: ok');
server?.close();
process.exit(failures.length ? 1 : 0);
