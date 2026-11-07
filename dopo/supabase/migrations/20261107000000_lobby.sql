-- The netplay lobby: the rooms hosts announce, as the libretro lobby
-- (lobby.libretro.com) keeps them. The lobby function (functions/lobby)
-- is the only one that reads and writes it, with the service role; the
-- anon key the pages carry sees nothing here.
create table if not exists public.lobby_rooms (
  id                    bigint generated always as identity primary key,
  -- a native host's address, or a browser host's peer id (peer.js)
  ip                    text        not null,
  port                  integer     not null,
  -- the address the announce came from: a room is changed or removed only
  -- from there, and it caps the rooms one address keeps open
  source                text        not null default '',
  username              text        not null default '',
  country               text        not null default '',
  game_name             text        not null default '',
  game_crc              text        not null default '00000000',
  core_name             text        not null default '',
  core_version          text        not null default '',
  subsystem_name        text        not null default 'N/A',
  retroarch_version     text        not null default '',
  frontend              text        not null default '',
  mitm_ip               text        not null default '',
  mitm_port             integer     not null default 0,
  mitm_session          text        not null default '',
  host_method           integer     not null default 0,
  has_password          boolean     not null default false,
  has_spectate_password boolean     not null default false,
  connectable           boolean     not null default true,
  is_retroarch          boolean     not null default false,
  player_count          integer     not null default 0,
  spectator_count       integer     not null default 0,
  created               timestamptz not null default now(),
  updated               timestamptz not null default now(),
  unique (ip, port)
);

create index if not exists lobby_rooms_updated on public.lobby_rooms (updated);
create index if not exists lobby_rooms_source on public.lobby_rooms (source);

alter table public.lobby_rooms enable row level security;
revoke all on public.lobby_rooms from anon, authenticated;
