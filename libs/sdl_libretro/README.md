# SDL Core for Libretro

> A libretro core for native games on embedded devices without a window manager. Provides an SDL backend that communicates with the libretro frontend over IPC.

## Supported Games

 - [Balatro](playbalatro.com) (love2d)
 - [Hoarder's Horrible House of Stuff](https://alesan99.itch.io/hoarders-horrible-house-of-stuff) (love2d)

## Options

Environment overrides the core option when both are set.

| environment | core option | default | description |
|---|---|---|---|
| `DOPO_SDL_BIN` | `sdl_bin` | — | Binary that runs the content, which becomes its argument. |
| `DOPO_SDL_LD` | `sdl_ld` | — | Extra `LD_LIBRARY_PATH` entries, `:` separated. |
| `DOPO_SDL_GPTK` | `sdl_gptk` | — | GPTokeyb file; listed buttons become keys, the rest stay gamepad. |
| `DOPO_SDL_SHIM` | `sdl_shim` | core dir | Where the shim libraries live. |
| `DOPO_SDL_FORMAT` | `sdl_format` | `rgba8888` | `rgba8888`, `rgb565` or `egl` (own window, no capture). |
| `DOPO_SDL_PRELOAD` | `sdl_preload` | `enabled` | Inject the shim with `LD_PRELOAD` too. |

Relative paths resolve against the content directory.
