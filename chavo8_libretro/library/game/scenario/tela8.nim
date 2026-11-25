## tela 8: the records, the best ten rolling up over a blue gradient, the
## first blinking; after 280 frames, back to the opening

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import ../v1/[chavesvar, chavestexto, chavescentrox]

proc tela8*() =
  for i in 0 .. 20:
    barra(0, i * 20, 400, i * 20 + 20, RGB(i * 5 + 100, i * 5 + 130, i * 5 + 160))
  for i in 1 .. 10:
    var k: Integer = 0
    if i == 1 and Int(piscar / 2.0) == piscar / 2.0: k = 80
    let y = i * 60 - 350 - cameray
    let cor = RGB(255, 255 - k, 255 - k)
    texto(50, y, (if i < 10: lit(0x4aef38) else: "") & IntToStr(Integer(i)) & lit(0x4af2ec), '-', 15, cor)
    texto(100, y, recorde[i].nome, '-', 15, cor)
    DXImageListRetrato.Draw((recorde[i].personagem - 1) * 5, 160, i * 60 - 370 - cameray)
    texto(230, y, IntToStr(Int64(recorde[i].pontos)), '-', 15, cor)
  if contador > 240:
    for i in 0 .. 15: barra(0, i * 20, 400, i * 20 + contador - 240, RGB(0, 0, 0))
  if piscar < 18: texto(200, 5, $Lines(MemoTexto, 21), 'c', 12, RGB(255, 255, 255))
  if piscar < 10: texto(200, 240, $Lines(MemoTexto, 6), 'c', 10, RGB(255, 255, 255))
  inc piscar
  if piscar > 20: piscar := 0
  inc contador
  if contador > 1000: contador := 1000
  if contador > 80: cameray := cameray - 5
  if cameray < -320: cameray := -320
  if contador > 280:
    tela := 1
    piscar := 0
    contador := 0
    cameray := 0
