## DXTimer1Timer, the game's frame: each screen is an if on tela, one after
## the other, so a screen that hands over to the next lets it run in the same
## frame; then what is drawn over every screen, and the input of the frame
## kept for the next. Screens not ported yet say so.

import ../../delphix/[pascal, delphix]
import chavesvar, chavestexto, chavescentrox

const
  branco = TColor(0xFFFFFF)
  preto = TColor(0)

proc linha(i: Integer): string = $Lines(MemoTexto, i)

proc tela0() =
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  texto(200, 10, lit(0x4aeb94), 'c', 10, RGB(100, 100, 255))
  texto(200, 25, versao, 'c', 9, RGB(100, 100, 255))
  if piscar < 12: texto(200, 100, lit(0x4aebc0), 'c', 10, RGB(255, 255, 255))
  texto(200, 130, lit(0x4aebe0), 'c', 10, RGB(120, 120, 120))
  texto(200, 150, lit(0x4aebf0), 'c', 10, RGB(120, 120, 120))
  texto(200, 170, lit(0x4aec08), 'c', 10, RGB(120, 120, 120))
  var cor: TColor
  if piscar <= 5: cor = RGB(255, 255, 255)
  if piscar > 5 and piscar <= 10: cor = RGB(225, 225, 225)
  if piscar > 10: cor = RGB(195, 195, 195)
  if resolucao == 1: texto(200, 130, lit(0x4aebe0), 'c', 10, cor)
  if resolucao == 2: texto(200, 150, lit(0x4aebf0), 'c', 10, cor)
  if resolucao == 3: texto(200, 170, lit(0x4aec08), 'c', 10, cor)
  DXImageListIcone.Draw(0, 150, 220)
  DXImageListIcone.Draw(1, 165, 220)
  texto(200, 220, lit(0x4aec20), 'c', 7, RGB(255, 255, 255))
  texto(200, 230, lit(0x4aec3c), 'c', 7, RGB(255, 255, 255))
  cor = RGB(255, 100, 100)
  if piscar <= 8: cor = RGB(255, 150, 150)
  texto(200, 270, lit(0x4aec54), 'c', 8, cor)
  Update()
  if isUp in Keyboard.States and jogador[1].antes(isUp) == 0: dec resolucao
  if isDown in Keyboard.States and jogador[1].antes(isDown) == 0: inc resolucao
  if isUp in Joystick.States and jogador[2].antes(isUp) == 0: dec resolucao
  if isDown in Joystick.States and jogador[2].antes(isDown) == 0: inc resolucao
  if resolucao < 1: resolucao := 1
  if resolucao > 3: resolucao := 3
  if isButton1 in Bound.States:
    # TELA CHEIA (1) and (2) set the display mode: the frontend's
    tela := 1
    cameray := 0
    piscar := 0
    contador := 0
  inc piscar
  if piscar > 15: piscar := 0

## the opening: the village scrolls down to Chaves punching Seu Madruga, then
## the logo grows; past 650 frames a demonstration fight begins
proc tela1() =
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  if contador < 100:
    barra(0, 0, 400, 300, RGB(120, 120, 250))
    DXImageListAbertura.Draw(14, 110, 10)
    texto(200, 210, linha(13), 'c', 16, RGB(250, 200, 0))
  if contador >= 100 and contador <= 370:
    if contador == 100: cameray := 0
    for i in 0 .. 20:
      barra(0, i * 15, 400, i * 15 + 15, RGB(120 + 2 * i, 120 + 2 * i, 200))
    DXImageListAbertura.Draw(2, 50, 50 - Round(cameray / 2))
    DXImageListAbertura.Draw(2, 350, 150 - Round(cameray / 2))
    DXImageListAbertura.Draw(2, 300, 180 - Round(cameray / 2))
    for j in 0 .. 4:
      DXImageListAbertura.Draw(3, 50, j * 47 + 73 - Round(cameray / 2))
      DXImageListAbertura.Draw(3, 350, j * 47 + 173 - Round(cameray / 2))
      DXImageListAbertura.Draw(3, 300, j * 47 + 203 - Round(cameray / 2))
    for j in 0 .. 6:
      DXImageListAbertura.Draw(0, 100, j * 80 - 200 - cameray)
      DXImageListAbertura.Draw(0, 305, j * 80 - 200 - cameray)
    for i in 0 .. 4:
      for j in 0 .. 15:
        DXImageListAbertura.Draw(1, i * 39 + 113, j * 32 - 200 - cameray)
    barra(147, -155 - cameray, 270, -48 - cameray, RGB(0, 0, 0))
    retangulo(149, -153 - cameray, 268, -50 - cameray, RGB(255, 255, 255))
    for i in 0 .. 7:
      DXImageListAbertura.Draw(4, i * 50, 220 - cameray)
    for i in 0 .. 8:
      var d = 0
      if piscar < 10: d = 4
      DXImageListAbertura.Draw(7, i * 50, 245 - cameray + d)
      DXImageListAbertura.Draw(5, i * 50 - 15, 245 - cameray + d)
      DXImageListAbertura.Draw(6, i * 50 + 15, 245 - cameray + d)
    var d = 0
    if piscar > 10: d = 5
    if contador < 200:
      DXImageListAbertura.Draw(8, 210, d + 185 - cameray)
      DXImageListAbertura.Draw(9, 50, d + 178 - cameray)
    if contador == 200 and DXWaveListSnd.Count > 9: DXWaveListSnd.Play(9)
    if contador >= 200:
      DXImageListAbertura.Draw(11, 60 - (contador - 200) * 4, (contador - 200) * 2 + 180 - cameray)
      DXImageListAbertura.Draw(10, 100, 190 - cameray)

  var z: Integer = 0
  if contador < 400: z := 4
  if contador >= 400 and contador <= 420: z := contador - 396
  if contador >= 420 and contador <= 438: z := 440 - contador
  if contador >= 438: z := 2
  var w: Integer
  if z != 0: w := Round(223 / z)
  if z != 0: zoom := Round(117 / z)
  let logo = Rect(205 - w, -100 - cameray - zoom, 205 + w, -100 - cameray + zoom)
  if linguagem == 1: DXImageListAbertura.DrawAlpha(12, logo, 255)
  if linguagem == 2: DXImageListAbertura.DrawAlpha(13, logo, 255)
  if contador > 440:
    texto(320, 140, versao, '-', 8, RGB(255, 255, 255))
  if contador > 440:
    if contador < 500:
      if Int(contador / 2) == contador / 2:
        texto((500 - contador) * 4 + 200, 170, linha(22), 'c', 12, RGB(90, 90, 255))
      else:
        texto(200 - (500 - contador) * 4, 170, linha(22), 'c', 12, RGB(90, 90, 255))
    if contador >= 500:
      texto(200, 170, linha(22), 'c', 12, RGB(90, 90, 255))
    DXImageListAbertura.Draw(15, 5, 280)
    texto(200, 253, linha(26), 'c', 8, RGB(255, 255, 255))
    texto(200, 267, linha(31), 'c', 8, RGB(255, 255, 255))
  if creditos > 0:
    contador := 500
    cameray := -200
  inc piscar
  if piscar > 20: piscar := 0
  if contador < 1000: inc contador
  if contador > 235: cameray := cameray - 2
  if cameray < -200: cameray := -200
  if contador > 650:
    tela := 7
    tempodemo := 600
    at(Byte, 0x4b44e0) := 0
    at(Byte, 0x4b44e3) := 99
    at(Byte, 0x4b44e4) := 25
    at(Byte, 0x4b44de) := 72
    at(Byte, 0x4b44df) := 0
    jogador[1].at(Byte, 0x9e) := 0
    jogador[1].at(Byte, 0x6b) := 1
    jogador[2].at(Byte, 0x9e) := 0
    jogador[2].at(Byte, 0x6b) := 1
    zerardados(Byte(1))
    zerardados(Byte(2))
    at(Byte, 0x4b57f9) := 0
    at(Byte, 0x4b57f8) := 0
    at(Byte, 0x4b449b) := 0
    at(Byte, 0x4b449a) := 0
    at(SmallInt, 0x4b44d2) := -120
    at(SmallInt, 0x4b44d4) := 60
    camerax := 100
    at(Byte, 0x4b57f4) := 1
    var n: Integer
    n := Round(Random(120) / 10) + 1
    if n < 1: n := 1
    if n > 12: n := 12
    cenario := n
    if cenario < 10: carregarcenario(numerado("Cenarios/Cenario-0", Integer(cenario), ".bmp"))
    if cenario >= 10: carregarcenario(numerado("Cenarios/Cenario-", Integer(cenario), ".bmp"))
    for p in 1 .. 2:
      n := Round(Random(150) / 10) + 1
      if n < 1: n := 1
      if n > 15: n := 15
      jogador[p].personagem := n
      for c in 1 .. 14:
        if jogador[p].personagem == c:
          carregarlutador(numerado(if c < 10: "Lutadores/C0" else: "Lutadores/C", c, ".bmp"),
            Byte(uint8(c)), Byte(uint8(p)))
  if piscar < 12 and creditos == 0: texto(200, 235, linha(6), 'c', 10, RGB(255, 255, 255))
  if piscar < 12 and creditos > 0: texto(200, 235, linha(7), 'c', 10, RGB(255, 255, 255))

proc naoportada() =
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  texto(200, 140, "tela " & $tela & " ainda nao portada", 'c', 10, RGB(255, 255, 255))

## over every screen but the settings
proc sobreposicao() =
  if tela == 2 or tela == 3: return
  if mostravelocidade > 0:
    texto(200, 260, lit(0x4af4c8), 'c', 8, RGB(255, 255, 255))
    let k = 238 - Integer(Round(velocidade / 1.05))
    barra(160, 278, 240, 280, RGB(255, 255, 255))
    barra(k, 275, k + 2, 283, RGB(255, 50, 50))
    dec mostravelocidade
  if mostrarnivel == 1:
    texto(200, 285, linha(18) & IntToStr(niveljogo), 'c', 10, RGB(255, 255, 255))
  if mostrarcreditos == 1:
    texto(300, 285, linha(19), '-', 10, RGB(255, 255, 255))
    texto(395, 285, IntToStr(creditos), 'e', 10, RGB(255, 255, 255))
  if desafiante > 0 and tela == 7:
    barraredonda(80, 100, 320, 170, 10, RGB(255, 255, 255), RGB(0, 0, 0))
    texto(200, 100, linha(10), 'c', 15, RGB(250, 140, 140))
    texto(200, 120, linha(11), 'c', 15, RGB(250, 140, 140))
    texto(200, 140, linha(12), 'c', 15, RGB(250, 140, 140))
    at(Byte, 0x4b44e0) := 1
    dec desafiante
    if desafiante == 0:
      tela := 4
      at(Byte, 0x4b44d7) := 1
      at(Byte, 0x4b44e3) := 20
      at(Byte, 0x4b44e4) := 25

## a punch or kick pressed now adds 6 to its repeat count, which falls by
## one a frame out of a blow and stays up to 15
proc repeticoes() =
  for (p, d) in [(1, Keyboard), (2, Joystick)]:
    let s = d.States
    let j = jogador[p]
    if (isButton1 in s and j.antes(isButton1) == 0) or (isButton3 in s and j.antes(isButton3) == 0):
      j.repetesoco := j.repetesoco + 6
    if (isButton2 in s and j.antes(isButton2) == 0) or (isButton4 in s and j.antes(isButton4) == 0):
      j.repetechute := j.repetechute + 6
  for p in 1 .. 2:
    let j = jogador[p]
    if j.repetesoco > 0 and at(Byte, 0x4b57f8) == 0 and at(Byte, 0x4b57f9) == 0: dec j.repetesoco
    if j.repetechute > 0 and at(Byte, 0x4b57f8) == 0 and at(Byte, 0x4b57f9) == 0: dec j.repetechute
    if j.repetesoco > 15: j.repetesoco := 15
    if j.repetechute > 15: j.repetechute := 15

proc lembrar() =
  for (p, d) in [(1, Keyboard), (2, Joystick)]:
    let s = d.States
    for k in [isUp, isDown, isLeft, isRight, isButton1, isButton2, isButton3, isButton4]:
      jogador[p].antes(k) := (if k in s: 1 else: 0)

proc DXTimer1Timer*() =
  if mentecapto == 1:
    at(SmallInt, 0x4b44d2) := -90
    at(SmallInt, 0x4b44d4) := 40
  TimerInterval = velocidade
  if tela == 0: tela0()
  if tela == 1: tela1()
  if tela > 1 and tela <= 12: naoportada()
  sobreposicao()
  if pausa == 1: TimerEnabled = false
  repeticoes()
  lembrar()
