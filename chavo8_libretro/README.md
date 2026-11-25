# Chavo8 core for Libretro

> Street Chaves, the 2004 Delphi fighting game by Cybergamba, brought back as a libretro core: its logic redone as the original wrote it, on the files of the game you already have.

Load `Chaves.exe` from the game's folder, or a `.zip` holding that folder. The core reads the pictures and waves of the folders and of the exe itself; nothing of the game is in this repository.

## Layout

- `library/platform/`: libretro, the game's files (folder or zip, names without case), the exe's resources and form, BMP and WAV, and the game's texts read from the code that filled its memos (`texts.c`).
- `library/delphix/`: what the game got from DelphiX and Windows: 16-bit surfaces, GDI canvas and text, DirectSound mixing; `pascal.nim` gives Delphi's integer and RTL semantics to the Nim ports.
- `library/game/v1/`: the original releases, ported from their Delphi units in Nim; `library/chavo8.nim` builds every game into one static library linked into the core.

The settings live in `chavo8.ini` of the frontend's save folder; without it, a `Chaves.dat` of the original beside the exe is read, as are the unlock files (`Churros.dat`, `Satanas.dat`, `SC01.dat`...).

The font is [Comic Relief](https://github.com/loudifier/Comic-Relief) (OFL), metric-compatible with the Comic Sans MS the game took from Windows.

## State

The game runs its resolution screen, its opening, the fighter selection and the one player ladder, then stops at the screens not ported yet. Each port picks a RetroPad layout. **Moderno**, the default: A confirms, so A and X punch, B and Y kick; A or START also put a coin in and start wherever a player may enter, SELECT opens the settings and backs out of them. **Original**: punches on Y and X, kicks on B and A as on arcade sticks (the keyboard's Z, C, X, V); START is the 1 or 2 key, SELECT inserts a coin.

The core option `chavo8_viewer` opens an asset viewer instead: L/R change folder, left/right the item, up/down the fighter frame (sliced as the game does) or the text lines, A mirrors or plays, B stops, X shows the color key, START animates.
