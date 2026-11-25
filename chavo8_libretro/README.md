# Chavo8 core for Libretro

> Street Chaves, the 2004 Delphi fighting game by Cybergamba, brought back as a libretro core: its logic redone as the original wrote it, on the files of the game you already have.

Load `Chaves.exe` from the game's folder, or a `.zip` or `.rar` (RAR 1.5 to 5, through [dmc_unrar](https://github.com/DrMcCoy/dmc_unrar), GPL-2.0-or-later) holding that folder. The core reads the pictures and waves of the folders and of the exe itself; nothing of the game is in this repository.

## Layout

- `library/platform/core/`: libretro, the game's files (folder, zip or rar, names without case), the exe's resources and form, BMP and WAV, and the game's texts read from the code that filled its memos (`texts.c`).
- `library/platform/delphix/`: what the game got from DelphiX and Windows: 16-bit surfaces, GDI canvas and text, DirectSound mixing.
- `library/platform/emu/`: what runs Delphi's code: `pascal.nim` gives its integer and RTL semantics to the Nim ports, `x86.c` and `vcl.c` its RTL, VCL and DelphiX to the lifted x86 (`include/chavo8/x86.h`).
- `library/game/v1/`: the original releases, ported from their Delphi units in Nim; `library/chavo8.nim` builds every game into one static library linked into the core.
- `library/game/scenario/`: the screens of DXTimer1Timer ported one a file (`tela8.nim`: the records, `tela9.nim`: continue and game over, `tela10.nim`: the fifteen endings, each a script in a small DSL over the frame, `tela11.nim`: the thanks, `tela12.nim`: a new record's name).

The fight (tela 7 of DXTimer1Timer, the methods it calls and ChavesGolpes' tables of moves, thousands of plain ifs) is not ported by hand: `library/game/assembly.tar.xz` holds its x86 as `scripts/dump.nim` took it from the exe, one `.s` a function, and the build unpacks it and turns it into C with `scripts/lift.nim` (`x86v1_<function>.c`), over the machine of `include/chavo8/x86.h` whose memory is ChavesVar's bytes. Its calls out go to Delphi's RTL (`library/platform/emu/x86.c`), to the VCL and DelphiX over a form laid out as objects (`library/platform/emu/vcl.c`), or back to the methods ported in Nim (`library/game/v1/lifted.nim`). To dump again:

```
dump Chaves.exe library/game/assembly.tar.xz 473728-48dd98 48edfc-4905a8=desenhaefeitos 4905a8-4908a8=seta \
  490bb8-4914bc=desenhalutador 491b5c-491edc=mostrasequencia 491edc-4931ac=mostragolpe 4931ac-494bd8=cpu \
  494bd8-4951b8=defendergolpe 4951b8-4951ec=sequencia 4951ec-4968b0=golpe 4968b0-499ea4=sequenciagolpe \
  49a3f4-49cd34=desenhaanimacoes 48e5fc-48e69c=circulo 48e69c-48e720=circulopintado 4a44b3-4ab259=tela7
```

The settings live in `chavo8.ini` of the frontend's save folder; without it, a `Chaves.dat` of the original beside the exe is read, as are the unlock files (`Churros.dat`, `Satanas.dat`, `SC01.dat`...).

The font is [Comic Relief](https://github.com/loudifier/Comic-Relief) (OFL), metric-compatible with the Comic Sans MS the game took from Windows.

## State

The game runs its resolution screen, its opening, the fighter selection and the one player ladder, then stops at the screens not ported yet. Each port picks a RetroPad layout. **Moderno**, the default: A confirms, so A and X punch, B and Y kick; A or START also put a coin in and start wherever a player may enter, SELECT opens the settings and backs out of them. **Original**: punches on Y and X, kicks on B and A as on arcade sticks (the keyboard's Z, C, X, V); START is the 1 or 2 key, SELECT inserts a coin.

The core option `chavo8_viewer` opens an asset viewer instead: L/R change folder, left/right the item, up/down the fighter frame (sliced as the game does) or the text lines, A mirrors or plays, B stops, X shows the color key, START animates.
