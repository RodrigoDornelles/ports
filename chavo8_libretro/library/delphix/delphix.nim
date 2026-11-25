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
