## tela 9: continue? counting down from tempo, a second every 25 frames;
## at zero GAME OVER flashes in and, 150 frames later, the records

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import ../v1/[chavesvar, chavestexto, chavescentrox]

proc tela9*() =
  for i in 0 .. 60:
    barra(0, i * 5, 400, i * 5 + 5, RGB(i * 3 + 10, i * 3 + 50, i * 3 + 30))
  if tempo > 0 or quadros > 0:
    texto(200, 20, $Lines(MemoTexto, 24), 'c', 35, RGB(255, 120, 120))
    let cor = RGB(255, tempo * 25, tempo * 25)
    if tempo > 3: texto(320, 70, IntToStr(Integer(tempo)), 'c', 120, cor)
    if tempo <= 3 and piscar < 8: texto(320, 70, IntToStr(Integer(tempo)), 'c', 120, cor)
    inc piscar
    if piscar > 10: piscar := 0
  if quadros > 0: dec quadros
  if quadros == 0 and tempo > 0:
    dec tempo
    quadros := 25
  if tempo == 0 and quadros == 0:
    texto(200, 110, $Lines(MemoTexto, 23), 'c', 36, RGB(255, 0, 0))
    if piscar < 15: barra(0, piscar * 10, 400, 300 - piscar * 10, RGB(255, 255, 255))
    inc piscar
    if piscar > 150:
      tela := 8
      cameray := 0
      piscar := 0
      contador := 0
      jogador[1].jogando := 0
      jogador[2].jogando := 0
