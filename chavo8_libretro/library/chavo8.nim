## The games the core runs: one Nim program, so they share one runtime,
## built into one static library (dopo_nim_library) and called from C
## through include/chavo8/game.h.

import game/v1/v1

proc chavo8_game_name(version: cint): cstring {.exportc, cdecl.} =
  case version
  of 1: cstring(v1.name)
  else: nil
