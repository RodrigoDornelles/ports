## tela 11: the thanks after the last fight, the logo of the language's
## version; 200 frames later, GAME OVER (tela 9 with no time left)

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import ../v1/[chavesvar, chavestexto, chavescentrox]

proc tela11*() =
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  texto(200, 50, $Lines(MemoTexto, 25), 'c', 15, RGB(255, 255, 255))
  if linguagem == 1: DXImageListAbertura.Draw(12, 89, 80)
  if linguagem == 2: DXImageListAbertura.Draw(13, 89, 80)
  texto(200, 240, $Lines(MemoTexto, 26), 'c', 10, RGB(255, 255, 255))
  texto(200, 260, $Lines(MemoTexto, 31), 'c', 10, RGB(255, 255, 255))
  inc contador
  if contador > 200:
    tela := 9
    piscar := 0
    contador := 0
    tempo := 0
    quadros := 0
