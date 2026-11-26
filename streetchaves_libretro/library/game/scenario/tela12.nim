## tela 12: a new record, its three letters chosen by each player past the
## tenth: up and down change the letter, left and right or button 1 move
## along; past the third (or at 1000 frames) the record is kept and the
## game goes on, to the continue if the player was out, else to tela 10.
## The keyboard moves the first player's letters and the joystick the
## second's, once for each player with a record, as the original did.

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import ../v1/[chavesvar, chavestexto, chavescentrox]

proc tela12*() =
  let branco = RGB(255, 255, 255)
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  inc piscar
  if piscar > 20: piscar := 0
  inc contador
  if contador > 1000: contador := 1000
  for p in 1 .. 2:
    let j = jogador[p]
    if recorde[10].pontos >= j.pontos: continue
    for k in 1 .. 3:
      texto((p - 1) * 200 + 5 + k * 15, 5, $char(j.letra(k)), '-', 15, branco)
    if piscar < 15: texto((p - 1) * 200 + 20 + (j.cursor - 1) * 15, 4, lit(0x4af398), '-', 27, branco)
    Update()
    for (q, d) in [(1, Keyboard), (2, Joystick)]:
      let o = jogador[q]
      if isLeft in d.States and o.antes(isLeft) == 0: dec o.cursor
      if isRight in d.States and o.antes(isRight) == 0: inc o.cursor
    if j.cursor < 1: j.cursor := 1
    if j.cursor > 3: j.cursor := 3
    for (q, d) in [(1, Keyboard), (2, Joystick)]:
      let o = jogador[q]
      if isUp in d.States and o.antes(isUp) == 0: o.letra(o.cursor) := o.letra(o.cursor) + 1
      if isDown in d.States and o.antes(isDown) == 0: o.letra(o.cursor) := o.letra(o.cursor) - 1
    if j.letra(j.cursor) < 0x30: j.letra(j.cursor) := 0x30
    if j.letra(j.cursor) > 0x5a: j.letra(j.cursor) := 0x5a
    for (q, d) in [(1, Keyboard), (2, Joystick)]:
      if isButton1 in d.States and jogador[q].antes(isButton1) == 0: inc jogador[q].cursor
    if (j.cursor > 3 or contador == 1000) and j.jogando == 0:
      tela := 9
      piscar := 0
      contador := 0
      tempo := 9
      quadros := 25
      criarecorde(Byte(10), j.nome, j.personagem, Integer(j.pontos))
      ordenarecordista()
    if (j.cursor > 3 or contador == 1000) and j.jogando == 1:
      tela := 10
      piscar := 0
      contador := 0
      criarecorde(Byte(10), j.nome, j.personagem, Integer(j.pontos))
      ordenarecordista()
