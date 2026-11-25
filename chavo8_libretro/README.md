# Chavo8 core for Libretro

> Street Chaves, the 2004 Delphi fighting game by Cybergamba, brought back as a libretro core: its logic redone as the original wrote it, on the files of the game you already have.

Load `Chaves.exe` from the game's folder, or a `.zip` holding that folder. The core reads the pictures and waves of the folders and of the exe itself; nothing of the game is in this repository.

## Layout

- `library/platform/`: libretro, the game's files (folder or zip, names without case), the exe's resources and form, BMP and WAV, and the game's texts read from the code that filled its memos (`texts.c`).
- `library/delphix/`: what the game got from DelphiX and Windows: 16-bit surfaces, GDI canvas and text, DirectSound mixing; `pascal.nim` gives Delphi's integer and RTL semantics to the Nim ports.
- `library/game/v1/`: the original releases, ported from their Delphi units in Nim; `library/chavo8.nim` builds every game into one static library linked into the core.

The font is [Comic Relief](https://github.com/loudifier/Comic-Relief) (OFL), metric-compatible with the Comic Sans MS the game took from Windows.

## State

For now the core is an asset viewer: L/R change folder, left/right the item, up/down the fighter frame (sliced as the game does) or the text lines, A mirrors or plays, B stops, X shows the color key, START animates.
