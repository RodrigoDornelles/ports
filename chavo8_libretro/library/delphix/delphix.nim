## What the ports reach of the form, DelphiX, Windows and the RTL's files,
## over the C of library/delphix and library/platform, named as the Delphi
## code named it.

import pascal

type
  TColor* = Cardinal
  TImage* = enum
    ImageLutador, ImageRetrato, ImageLutadorFrame, ImageRetratoInvert
  TDXImageList* = enum
    DXImageListRetrato, DXImageListEfeitos, DXImageListCenario, DXImageListIcone,
    DXImageListLutador, DXImageListLutadorInvert, DXImageListAbertura, DXImageListFinal,
    DXImageListAnimacao, DXImageListEfeitosInvert
  TDXWaveList* = enum
    DXWaveListSnd, DXWaveListMusica, DXWaveListVitoria
  TRect* = object
    Left*, Top*, Right*, Bottom*: Integer
  Bitmap {.bycopy.} = object
    w, h: cint
    rgb: pointer

proc form_image(image: cint): ptr Bitmap {.importc, cdecl.}
proc form_image_load(image: cint, name: cstring): bool {.importc, cdecl.}
proc form_image_resize(image, w, h: cint) {.importc, cdecl.}
proc canvas_copy_rect(dst: ptr Bitmap, dl, dt, dr, db: cint,
  src: ptr Bitmap, sl, st, sr, sb: cint) {.importc, cdecl.}
proc dx_images_add(list: cint): cint {.importc, cdecl.}
proc dx_images_count(list: cint): cint {.importc, cdecl.}
proc dx_images_assign(list, index, image: cint, transparent: bool, color: uint32) {.importc, cdecl.}
proc dx_waves_add(list: cint): cint {.importc, cdecl.}
proc dx_waves_count(list: cint): cint {.importc, cdecl.}
proc dx_waves_load(list, index: cint, name: cstring): bool {.importc, cdecl.}
proc vfs_exists(name: cstring): bool {.importc, cdecl.}
proc vfs_load(name: cstring, size: ptr csize_t): pointer {.importc, cdecl.}
proc c_free(p: pointer) {.importc: "free", header: "<stdlib.h>".}
proc clock_day_ms(): uint32 {.importc, cdecl.}

proc RGB*(r, g, b: Integer): TColor =
  TColor(r and 0xFF) or TColor(g and 0xFF) shl 8 or TColor(b and 0xFF) shl 16

proc Rect*(left, top, right, bottom: Integer): TRect =
  TRect(Left: left, Top: top, Right: right, Bottom: bottom)

## RandSeed from the time of day, as GetSystemTime gave it
proc Randomize*() =
  RandSeed = cast[int32](clock_day_ms())

## only the game's folder is searched, whatever the list of folders
proc FileSearch*(name, dirList: string): string =
  if vfs_exists(cstring(name)): name else: ""

## TStrings.LoadFromFile: lines end at CR, LF or CR LF
proc LoadLines*(name: string): seq[string] =
  var size: csize_t
  let data = cast[ptr UncheckedArray[char]](vfs_load(cstring(name), addr size))
  if data == nil: return
  var line = ""
  var i = 0
  while i < int(size):
    let c = data[i]
    if c == '\r' or c == '\n':
      result.add line
      line = ""
      if c == '\r' and i + 1 < int(size) and data[i + 1] == '\n': inc i
    else:
      line.add c
    inc i
  if line.len > 0: result.add line
  c_free(data)

proc LoadFromFile*(image: TImage, name: string) =
  discard form_image_load(cint(ord(image)), cstring(name))

proc Width*(image: TImage): Integer = form_image(cint(ord(image))).w
proc Height*(image: TImage): Integer = form_image(cint(ord(image))).h

proc `Width=`*(image: TImage, w: Integer) =
  form_image_resize(cint(ord(image)), w, image.Height)

proc `Height=`*(image: TImage, h: Integer) =
  form_image_resize(cint(ord(image)), image.Width, h)

proc CopyRect*(dst: TImage, d: TRect, src: TImage, s: TRect) =
  canvas_copy_rect(form_image(cint(ord(dst))), d.Left, d.Top, d.Right, d.Bottom,
    form_image(cint(ord(src))), s.Left, s.Top, s.Right, s.Bottom)

proc Add*(list: TDXImageList) = discard dx_images_add(cint(ord(list)))
proc Count*(list: TDXImageList): Integer = dx_images_count(cint(ord(list)))

## Items[index].Picture := image's; Transparent; TransparentColor; Restore
proc Assign*(list: TDXImageList, index: Integer, image: TImage, transparent: bool, color: TColor) =
  dx_images_assign(cint(ord(list)), index, cint(ord(image)), transparent, color)

proc dx_images_load(list, index: cint, name: cstring, transparent: bool, color: uint32): bool {.importc, cdecl.}

## Items[index].Picture.LoadFromFile; Transparent; TransparentColor; Restore
proc LoadFromFile*(list: TDXImageList, index: Integer, name: string, transparent: bool, color: TColor) =
  discard dx_images_load(cint(ord(list)), index, cstring(name), transparent, color)

proc Add*(list: TDXWaveList) = discard dx_waves_add(cint(ord(list)))
proc Count*(list: TDXWaveList): Integer = dx_waves_count(cint(ord(list)))

proc LoadFromFile*(list: TDXWaveList, index: Integer, name: string) =
  discard dx_waves_load(cint(ord(list)), index, cstring(name))

proc config_get(key: cstring): cstring {.importc, cdecl.}

## the frontend's saved settings (chavo8.ini) as the lines a TStrings held:
## one per key, up to the first missing
proc ConfigLines*(keys: openArray[string]): seq[string] =
  for key in keys:
    let value = config_get(cstring(key))
    if value == nil: break
    result.add $value

proc exe_literal(va: uint32, text: ptr cstring): csize_t {.importc, cdecl.}

## a string literal of the exe's code, by the address the disassembly shows,
## so the game's own words come from the exe (and a mod's edited ones too)
proc lit*(va: int): string =
  var text: cstring
  let n = exe_literal(uint32(va), addr text)
  result = newString(n)
  if n > 0: copyMem(addr result[0], text, n)

type
  TDXInputState* = enum
    isUp, isDown, isLeft, isRight, isButton1, isButton2, isButton3, isButton4,
    isButton5, isButton6, isButton7, isButton8, isButton9, isButton10, isButton11, isButton12
  TDXInputDevice* = enum
    Keyboard, Joystick, Bound

proc dx_input_update() {.importc, cdecl.}
proc dx_input_states(device: cint): uint32 {.importc, cdecl.}

## DXInput1.Update: the states read until the next one
proc Update*() = dx_input_update()

## Keyboard.States (player 1), Joystick.States (player 2), Bound: DXInput1.States
proc States*(device: TDXInputDevice): set[TDXInputState] =
  let bits = dx_input_states(cint(ord(device)))
  for s in TDXInputState:
    if (bits shr ord(s) and 1) != 0: result.incl s

proc dx_screen(): pointer {.importc, cdecl.}
proc canvas_fill_rect(dst: pointer, l, t, r, b: cint, color: uint32) {.importc, cdecl.}
proc canvas_frame_rect(dst: pointer, l, t, r, b: cint, color: uint32) {.importc, cdecl.}
proc canvas_round_rect(dst: pointer, l, t, r, b, w, h: cint, pen, brush: uint32) {.importc, cdecl.}
proc canvas_text_out(dst: pointer, x, y: cint, color: uint32, size: cint, text: cstring) {.importc, cdecl.}
proc canvas_text_width(size: cint, text: cstring): cint {.importc, cdecl.}
proc form_image_pixel(image, x, y: cint): uint32 {.importc, cdecl.}
proc dx_images_draw(list, index, x, y: cint) {.importc, cdecl.}
proc dx_images_draw_alpha(list, index, l, t, r, b, alpha: cint) {.importc, cdecl.}
proc dx_waves_play(list, index: cint) {.importc, cdecl.}

## DXDraw1.Surface.Canvas, with GDI's brush, pen and font
proc FillRect*(r: TRect, brush: TColor) =
  canvas_fill_rect(dx_screen(), r.Left, r.Top, r.Right, r.Bottom, brush)

proc FrameRect*(r: TRect, brush: TColor) =
  canvas_frame_rect(dx_screen(), r.Left, r.Top, r.Right, r.Bottom, brush)

proc RoundRect*(x1, y1, x2, y2, x3, y3: Integer, pen, brush: TColor) =
  canvas_round_rect(dx_screen(), x1, y1, x2, y2, x3, y3, pen, brush)

proc TextOut*(x, y: Integer, text: string, size: Integer, color: TColor) =
  canvas_text_out(dx_screen(), x, y, color, size, cstring(text))

proc TextWidth*(text: string, size: Integer): Integer =
  canvas_text_width(size, cstring(text))

proc Pixels*(image: TImage, x, y: Integer): TColor =
  form_image_pixel(cint(ord(image)), x, y)

proc GetRValue*(c: TColor): Byte = Byte(uint8(c and 0xFF))
proc GetGValue*(c: TColor): Byte = Byte(uint8(c shr 8 and 0xFF))
proc GetBValue*(c: TColor): Byte = Byte(uint8(c shr 16 and 0xFF))

## Items[index].Draw(DXDraw1.Surface, x, y, 0)
proc Draw*(list: TDXImageList, index, x, y: Integer) =
  dx_images_draw(cint(ord(list)), index, x, y)

## Items[index].DrawAlpha(DXDraw1.Surface, rect, 0, alpha)
proc DrawAlpha*(list: TDXImageList, index: Integer, r: TRect, alpha: Integer) =
  dx_images_draw_alpha(cint(ord(list)), index, r.Left, r.Top, r.Right, r.Bottom, alpha)

proc dx_images_stretch(list, index, l, t, r, b: cint) {.importc, cdecl.}

## Items[index].StretchDraw(DXDraw1.Surface, rect, 0)
proc StretchDraw*(list: TDXImageList, index: Integer, r: TRect) =
  dx_images_stretch(cint(ord(list)), index, r.Left, r.Top, r.Right, r.Bottom)

proc Play*(list: TDXWaveList, index: Integer) =
  dx_waves_play(cint(ord(list)), index)

proc dx_waves_stop(list, index: cint) {.importc, cdecl.}
proc Stop*(list: TDXWaveList, index: Integer) =
  dx_waves_stop(cint(ord(list)), index)

proc dx_terminate() {.importc, cdecl.}
proc Terminate*() = dx_terminate()

proc config_set(key, value: cstring) {.importc, cdecl.}
proc config_clear() {.importc, cdecl.}
proc config_save(): bool {.importc, cdecl.}

## the lines a TStrings held, saved as chavo8.ini, a key per line in order:
## keys past the lines are dropped, as the file ended there
proc SaveConfigLines*(keys: openArray[string], lines: openArray[string]) =
  config_clear()
  for i in 0 ..< min(keys.len, lines.len):
    config_set(cstring(keys[i]), cstring(lines[i]))
  discard config_save()

const
  VK_ESCAPE* = 0x1b
  VK_LWIN* = 0x5b
  VK_F1* = 0x70
  VK_F2* = 0x71
  VK_OEM_PLUS* = 0xbb
  VK_OEM_COMMA* = 0xbc
  VK_OEM_MINUS* = 0xbd
  VK_OEM_PERIOD* = 0xbe

## Keyboard.KeyAssigns: up to three virtual keys a state, as the DFM set them
var KeyAssigns*: array[TDXInputState, array[3, Integer]] = [
  [0x26'i32, 0, 0], [0x28'i32, 0, 0], [0x25'i32, 0, 0], [0x27'i32, 0, 0],
  [ord('Z').int32, 0, 0], [ord('X').int32, 0, 0], [ord('C').int32, 0, 0], [ord('V').int32, 0, 0],
  [0'i32, 0, 0], [0'i32, 0, 0], [0'i32, 0, 0], [0'i32, 0, 0],
  [0'i32, 0, 0], [0'i32, 0, 0], [0'i32, 0, 0], [0'i32, 0, 0]]

## DXTimer1: the frontend runs OnTimer while enabled, at Interval ms
var TimerEnabled* = true
var TimerInterval*: Integer = 1
