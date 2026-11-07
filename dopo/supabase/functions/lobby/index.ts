/**
 * The netplay lobby, compatible with lobby.libretro.com: POST /add, GET
 * /list, plus POST /remove. A browser host's address is its peer id.
 */
import { createClient } from "jsr:@supabase/supabase-js@2";
import lobby from "../../lobby.json" with { type: "json" };

const TTL = 60;
const PER_SOURCE = 8;
const PEER = /^[A-Za-z0-9]{16,64}$/;

const TEXT: Record<string, number> = {
  username: 32,
  game_name: 255,
  game_crc: 8,
  core_name: 64,
  core_version: 64,
  subsystem_name: 64,
  retroarch_version: 32,
  frontend: 64,
  mitm_session: 64,
};

const FIELDS = [
  "id", "username", "country", "game_name", "game_crc", "core_name",
  "core_version", "subsystem_name", "retroarch_version", "frontend", "ip",
  "port", "mitm_ip", "mitm_port", "mitm_session", "host_method",
  "has_password", "has_spectate_password", "connectable", "is_retroarch",
  "created", "updated", "player_count", "spectator_count",
];

const db = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
  { auth: { persistSession: false } },
);

const LOCAL = /^http:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/;

const CORS = {
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-client-info",
};

function reply(body: string, status = 200, type = "text/plain; charset=utf-8") {
  return new Response(body, { status, headers: { ...CORS, "Content-Type": type } });
}

function source(req: Request): string {
  const forwarded = req.headers.get("x-forwarded-for") ?? "";
  return forwarded.split(",")[0].trim() || req.headers.get("x-real-ip") || "";
}

function country(req: Request): string {
  const code = (req.headers.get("cf-ipcountry") ?? "").toLowerCase();
  return /^[a-z]{2}$/.test(code) && code !== "xx" ? code : "";
}

async function form(req: Request): Promise<Record<string, string>> {
  const type = req.headers.get("content-type") ?? "";
  if (type.includes("application/json")) {
    const body = await req.json().catch(() => ({}));
    return Object.fromEntries(Object.entries(body ?? {}).map(([k, v]) => [k, String(v)]));
  }
  return Object.fromEntries(new URLSearchParams(await req.text()));
}

const flag = (value?: string) => value === "1" || value?.toLowerCase() === "true";
const int = (value: string | undefined, min: number, max: number) => {
  const n = Math.trunc(Number(value));
  return Number.isFinite(n) ? Math.min(max, Math.max(min, n)) : min;
};

function address(req: Request, fields: Record<string, string>): string | null {
  if (fields.peer) return PEER.test(fields.peer) ? fields.peer : null;
  return source(req) || null;
}

function room(row: Record<string, unknown>) {
  return Object.fromEntries(FIELDS.map((key) => [key, row[key]]));
}

async function add(req: Request) {
  const fields = await form(req);
  const ip = address(req, fields);
  const from = source(req);
  if (!ip) return reply("invalid peer\n", 400);

  const row: Record<string, unknown> = {
    ip,
    port: int(fields.port, 1, 65535),
    source: from,
    country: country(req),
    is_retroarch: !fields.peer,
    has_password: flag(fields.has_password),
    has_spectate_password: flag(fields.has_spectate_password),
    player_count: int(fields.player_count, 0, 64),
    spectator_count: int(fields.spectator_count, 0, 64),
    updated: new Date().toISOString(),
  };
  for (const [key, max] of Object.entries(TEXT))
    if (fields[key] !== undefined) row[key] = fields[key].slice(0, max);
  if (fields.mitm_custom_addr) {
    row.mitm_ip = fields.mitm_custom_addr.slice(0, 64);
    row.mitm_port = int(fields.mitm_custom_port, 0, 65535);
    row.host_method = 3;
  }
  if (!row.core_name) return reply("core_name missing\n", 400);

  const { data: found } = await db.from("lobby_rooms").select("source")
    .eq("ip", ip).eq("port", row.port).maybeSingle();
  if (found && found.source !== from) return reply("room taken\n", 409);
  if (!found) {
    const { count } = await db.from("lobby_rooms").select("id", { count: "exact", head: true })
      .eq("source", from);
    if ((count ?? 0) >= PER_SOURCE) return reply("too many rooms\n", 429);
  }

  const { data, error } = await db.from("lobby_rooms")
    .upsert(row, { onConflict: "ip,port" }).select().single();
  if (error) return reply(`${error.message}\n`, 500);
  const lines = Object.entries(room(data)).map(([k, v]) => `${k}=${v ?? ""}`);
  return reply(lines.join("\n") + "\n");
}

async function list() {
  const stale = new Date(Date.now() - TTL * 1000).toISOString();
  await db.from("lobby_rooms").delete().lt("updated", stale);
  const { data, error } = await db.from("lobby_rooms").select(FIELDS.join(","))
    .order("id");
  if (error) return reply(`${error.message}\n`, 500);
  const rooms = (data ?? []).map((row) => ({ fields: room(row as Record<string, unknown>) }));
  return reply(JSON.stringify(rooms), 200, "application/json");
}

async function remove(req: Request) {
  const fields = await form(req);
  const ip = address(req, fields);
  if (!ip) return reply("invalid peer\n", 400);
  await db.from("lobby_rooms").delete()
    .eq("ip", ip).eq("port", int(fields.port, 1, 65535)).eq("source", source(req));
  return reply("status=OK\n");
}

async function route(req: Request): Promise<Response> {
  if (req.method === "OPTIONS") return reply("", 204);
  const path = new URL(req.url).pathname.replace(/\/+$/, "");
  try {
    if (req.method === "POST" && path.endsWith("/add")) return await add(req);
    if (req.method === "GET" && path.endsWith("/list")) return await list();
    if (req.method === "POST" && path.endsWith("/remove")) return await remove(req);
  } catch (e) {
    return reply(`${(e as Error).message}\n`, 500);
  }
  return reply("not found\n", 404);
}

Deno.serve(async (req) => {
  const origin = req.headers.get("origin");
  if (origin && origin !== lobby.site && !LOCAL.test(origin))
    return reply("origin not allowed\n", 403);
  const response = await route(req);
  if (origin) response.headers.set("Access-Control-Allow-Origin", origin);
  response.headers.set("Vary", "Origin");
  return response;
});
