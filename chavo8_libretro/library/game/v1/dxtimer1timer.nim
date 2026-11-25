## DXTimer1Timer, the game's frame: each screen is an if on tela, one after
## the other, so a screen that hands over to the next lets it run in the same
## frame; then what is drawn over every screen, and the input of the frame
## kept for the next. Screens not ported yet say so; the fight's is lifted.

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import chavesvar, chavestexto, chavescentrox, lifted
import ../scenario/[tela8, tela9, tela11, tela12]

const
  branco = TColor(0xFFFFFF)
  preto = TColor(0)

proc linha(i: Integer): string = $Lines(MemoTexto, i)

proc carregarcenarioatual() =
  if cenario < 10: carregarcenario(numerado("Cenarios/Cenario-0", Integer(cenario), ".bmp"))
  if cenario >= 10: carregarcenario(numerado("Cenarios/Cenario-", Integer(cenario), ".bmp"))

proc lutador(c: int): string =
  numerado(if c < 10: "Lutadores/C0" else: "Lutadores/C", c, ".bmp")

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
  if opcao == 1: texto(200, 130, lit(0x4aebe0), 'c', 10, cor)
  if opcao == 2: texto(200, 150, lit(0x4aebf0), 'c', 10, cor)
  if opcao == 3: texto(200, 170, lit(0x4aec08), 'c', 10, cor)
  DXImageListIcone.Draw(0, 150, 220)
  DXImageListIcone.Draw(1, 165, 220)
  texto(200, 220, lit(0x4aec20), 'c', 7, RGB(255, 255, 255))
  texto(200, 230, lit(0x4aec3c), 'c', 7, RGB(255, 255, 255))
  cor = RGB(255, 100, 100)
  if piscar <= 8: cor = RGB(255, 150, 150)
  texto(200, 270, lit(0x4aec54), 'c', 8, cor)
  Update()
  if isUp in Keyboard.States and jogador[1].antes(isUp) == 0: dec opcao
  if isDown in Keyboard.States and jogador[1].antes(isDown) == 0: inc opcao
  if isUp in Joystick.States and jogador[2].antes(isUp) == 0: dec opcao
  if isDown in Joystick.States and jogador[2].antes(isDown) == 0: inc opcao
  if opcao < 1: opcao := 1
  if opcao > 3: opcao := 3
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
    tempo := 99
    quadros := 25
    at(Byte, 0x4b44de) := 72
    at(Byte, 0x4b44df) := 0
    jogador[1].jogando := 0
    jogador[1].at(Byte, 0x6b) := 1
    jogador[2].jogando := 0
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
    carregarcenarioatual()
    for p in 1 .. 2:
      n := Round(Random(150) / 10) + 1
      if n < 1: n := 1
      if n > 15: n := 15
      jogador[p].personagem := n
      for c in 1 .. 14:
        if jogador[p].personagem == c:
          carregarlutador(lutador(c), Byte(uint8(c)), Byte(uint8(p)))
  if piscar < 12 and creditos == 0: texto(200, 235, linha(6), 'c', 10, RGB(255, 255, 255))
  if piscar < 12 and creditos > 0: texto(200, 235, linha(7), 'c', 10, RGB(255, 255, 255))

## the selection: the portraits slide in, each player moves a cursor over
## them and confirms with the first button, before the countdown ends
proc tela4() =
  at(Byte, 0x4b44e0) := 0
  if musicaatual != 0 and DXWaveListMusica.Count > 0: DXWaveListMusica.Stop(0)
  barra(0, 0, 400, 300, RGB(0, 0, 0))
  barra(0, 300 - contador, 200, 600 - contador, RGB(120, 120, 255))
  barra(200, contador - 300, 400, contador, RGB(255, 100, 100))
  for i in 0 .. 3:
    for j in 0 .. 1:
      for (l, icone) in [(1, 11), (2, 12)]:
        if linguagem == l:
          DXImageListIcone.Draw(icone, i * 145 - 67, j * 200 - 200 + quadros * 8)
          DXImageListIcone.Draw(icone, i * 145, j * 200 - 100 + quadros * 8)
  let k = Round(contador * 1.35)
  barra(k - 410, 156, k, 290, RGB(30, 50, 70))
  barra(k - 410, 158, k, 160, RGB(255, 255, 255))
  barra(k - 410, 286, k, 288, RGB(255, 255, 255))
  for i in 0 .. 14:
    var x: Integer
    if i < 8: x := k + i * 49 - 402
    if i >= 8: x := k + (i - 8) * 49 - 402
    if DXImageListRetrato.Count > i * 5:
      if i < 8: DXImageListRetrato.DrawAlpha(i * 5, Rect(x, 163, x + 50, 223), 60)
      if i >= 8: DXImageListRetrato.DrawAlpha(i * 5, Rect(x, 223, x + 50, 283), 60)
  if fase == 1:
    contador := contador + 20
    if contador >= 300:
      contador := 300
      fase := 2
  if fase == 2:
    for (p, grande, nome) in [(1, 52, 100), (2, 252, 300)]:
      let j = jogador[p]
      if j.jogando == 1:
        let r = (j.personagem - 1) * 5
        if DXImageListRetrato.Count > r + 4: DXImageListRetrato.Draw(r + 4, grande, 25)
        if DXImageListRetrato.Count > r and j.escolheu == 0:
          if j.personagem <= 8: DXImageListRetrato.Draw(r, Integer(Round(contador * 1.35)) + (j.personagem - 1) * 49 - 402, 163)
          if j.personagem > 8: DXImageListRetrato.Draw(r, Integer(Round(contador * 1.35)) + (j.personagem - 9) * 49 - 402, 223)
        texto(nome, 130 - Round(Cos(piscar / 4.3) * 4.0), $Lines(MemoNomes, j.personagem), 'c', 12,
          RGB(180, 220, 160))
    var v: Byte
    v := Round(Cos(quadros / 4.0) * 20.0) + 230
    var cor = RGB(v, v, 255)
    texto(200, Round(contador * 0.3) - 96, linha(8), 'c', 18, cor)
    cor = RGB(250, 150, 150)
    if tempo <= 5 and Int(quadros / 2) == quadros / 2: cor = RGB(250, 220, 220)
    texto(200, 15, IntToStr(tempo), 'c', 22, cor)
    if tempo >= 1:
      dec quadros
      if quadros == 0:
        dec tempo
        quadros := 25
    inc piscar
    if piscar >= 25: piscar := 0
    for p in 1 .. 2:
      let j = jogador[p]
      if j.jogando == 1 and j.escolheu == 0:
        var x: Integer = 10
        zoom := 20
        if j.personagem <= 8:
          x := (j.personagem - 1) * 49 + 3
          zoom := 160
        if j.personagem > 8:
          x := (j.personagem - 9) * 49 + 3
          zoom := 220
        var sombra: TColor
        if Int(piscar / 2) == piscar / 2: sombra = RGB(50, 50, 50) else: sombra = RGB(0, 0, 0)
        if p == 1: texto(x, zoom, IntToStr(p) & lit(0x4aeef8), '-', 10, RGB(100, 100, 200) + sombra)
        if p == 2: texto(x + 35, zoom, IntToStr(p) & lit(0x4aeef8), '-', 10, RGB(200, 50, 50) + sombra)
    Update()
    for (p, d) in [(1, Keyboard), (2, Joystick)]:
      let j = jogador[p]
      if j.escolheu == 0 and j.jogando == 1:
        let s = d.States
        if isLeft in s and j.antes(isLeft) == 0:
          dec j.personagem
          if DXWaveListSnd.Count - 1 > 0: DXWaveListSnd.Play(1)
        if isRight in s and j.antes(isRight) == 0:
          inc j.personagem
          if DXWaveListSnd.Count - 1 > 0: DXWaveListSnd.Play(1)
        if isUp in s and j.antes(isUp) == 0 and j.personagem > 8:
          j.personagem := j.personagem - 8
          if DXWaveListSnd.Count - 1 > 0: DXWaveListSnd.Play(1)
        if isDown in s and j.antes(isDown) == 0 and j.personagem <= 8:
          j.personagem := j.personagem + 8
          if DXWaveListSnd.Count - 1 > 0: DXWaveListSnd.Play(1)
        if isButton1 in s and j.antes(isButton1) == 0:
          j.escolheu := 1
          criaadversario(Byte(uint8(p)), j.personagem)
          if DXWaveListSnd.Count > 2: DXWaveListSnd.Play(2)
    for p in 1 .. 2:
      if jogador[p].personagem < 1: jogador[p].personagem := 15
      if jogador[p].personagem > 15: jogador[p].personagem := 1
    if jogador[1].jogando == 0: jogador[1].escolheu := 1
    if jogador[2].jogando == 0: jogador[2].escolheu := 1
  if tempo == 0:
    jogador[1].escolheu := 1
    if jogador[1].jogando == 1: criaadversario(Byte(1), jogador[1].personagem)
    jogador[2].escolheu := 1
    if jogador[2].jogando == 1: criaadversario(Byte(2), jogador[2].personagem)
    if DXWaveListSnd.Count > 2: DXWaveListSnd.Play(2)
  if jogador[1].escolheu == 1 and jogador[2].escolheu == 1:
    tela := 5
    piscar := 0
    contador := 0
    jogador[1].at(Byte, 0x8c) := 0
    jogador[2].at(Byte, 0x8c) := 0
    at(Byte, 0x4b44db) := 1
    if jogador[1].jogando == 1 and jogador[2].jogando == 1: tela := 6

## the one player ladder: the next opponent among the eight, then the scene
## of the fight, drawn by the one the computer plays (Fadiga.dat skips it)
proc tela5() =
  for i in 0 .. 60:
    barra(0, i * 10, 400, i * 10 + 10, RGB(i * 2 + 10, i * 3 + 20, i * 4 + 30))
  if etapa < 1: etapa := 1
  if etapa > 8: etapa := 8
  if musicaatual != 0 and DXWaveListMusica.Count > 0: DXWaveListMusica.Stop(0)
  for p in 1 .. 2:
    let j = jogador[p]
    j.at(Byte, 0x8c) := 0
    j.at(Double, 0x60) := 0.0
    j.at(Word, 0x58) := 0
    j.at(Byte, 0x97) := 0
  for p in 1 .. 2:
    let j = jogador[p]
    if j.jogando == 1:
      j.escolheu := 1
      let cor = RGB(150, 255, 180)
      if contador < 30: texto(200 - (contador - 30) * 20, 135, linha(20), 'c', 26, cor)
      if contador >= 30 and contador <= 100: texto(200, 135, linha(20), 'c', 26, cor)
      if contador > 100: texto(200 - (contador - 100) * 20, 135, linha(20), 'c', 26, cor)
      jogador[3 - p].personagem := j.adversario(etapa - 1)
      if contador > 40:
        texto(200, 180, $Lines(MemoNomes, jogador[3 - p].personagem), 'c', 15, RGB(255, 255, 255))
      for k in 1 .. 8:
        let r = j.adversario(k - 1) * 5 - 4
        let x = k * 48 - 40
        if DXImageListRetrato.Count > r:
          if contador < 50 and etapa >= k: DXImageListRetrato.Draw(r, x, 470 - contador * 5)
          if contador >= 50 and etapa >= k: DXImageListRetrato.Draw(r, x, 220)
          if contador < 50 and etapa > k: DXImageListIcone.Draw(10, x + 5, 485 - contador * 5)
          if contador >= 50 and etapa > k: DXImageListIcone.Draw(10, x + 5, 235)
        if churros == 0:
          if contador < 50 and etapa < k: DXImageListIcone.Draw(9, x, 470 - contador * 5)
          if contador >= 50 and etapa < k: DXImageListIcone.Draw(9, x, 220)
        if churros == 1 and etapa < k:
          if contador < 50: DXImageListRetrato.Draw(r, x, 470 - contador * 5)
          if contador >= 50: DXImageListRetrato.Draw(r, x, 220)
  if contador >= 50 and piscar <= 10: DXImageListIcone.Draw(8, etapa * 48 - 20, 215)
  if contador > 60 and DXImageListCenario.Count > 0:
    DXImageListCenario.StretchDraw(0, Rect(80, 6, 320, 134))
    texto(200, 115, $Lines(MemoCenarios, cenario - 1), 'c', 9, RGB(255, 255, 255))
  if contador == 0:
    const
      proprio = [1, 2, 2, 3, 3, 4, 11, 1, 1, 11, 6, 1, 1, 2, 1]
      outro = [0, 7, 10, 10, 8, 8, 5, 9, 10, 10, 5, 9, 5, 1, 6]
    for p in 1 .. 2:
      let j = jogador[p]
      if j.jogando == 0:
        for c in 1 .. 15:
          if j.personagem == c: cenario := proprio[c - 1]
        if Random(5) - 1 <= 0 and etapa < 8:
          for c in 2 .. 15:
            if j.personagem == c: cenario := outro[c - 1]
        if Random(6) - 1 <= 0 and etapa < 8 and Integer(j.personagem) in [3, 4, 9, 10, 11, 14]:
          cenario := 12
        if sc != 0 and etapa < 8:
          if sc < 1: sc := 1
          if sc > 12: sc := 12
          cenario := sc
        carregarcenarioatual()
  inc piscar
  if piscar > 15: piscar := 0
  inc contador
  if contador > 1000: contador := 1000
  Update()
  if (isButton1 in Keyboard.States and jogador[1].jogando == 1) or
      (isButton1 in Joystick.States and jogador[2].jogando == 1):
    if contador > 0: contador := contador + 5
  if contador > 120:
    for i in 0 .. 15:
      barra(0, i * 20, 400, i * 20 + contador - 120, RGB(0, 0, 0))
    if contador > 150: tela := 6
  if fadiga == 1:
    tela := 10
    piscar := 0
    contador := 0

## the fight is set: the countdown, both fighters' strips, the scene when two
## people play, and the music, by who the computer plays or else drawn
proc tela6() =
  tela := 7
  piscar := 0
  contador := 0
  at(Byte, 0x4b5808) := 1
  at(Byte, 0x4b44e0) := 1
  at(Word, 0x4b57f6) := 0
  if temporound == 1: tempo := 30
  if temporound == 2: tempo := 60
  if temporound == 3: tempo := 90
  if temporound == 4: tempo := 99
  quadros := 25
  at(Byte, 0x4b44de) := 0
  at(Byte, 0x4b44df) := 0
  camerax := 100
  at(Byte, 0x4b449a) := 0
  at(Byte, 0x4b57f8) := 0
  at(Byte, 0x4b57f9) := 0
  at(Byte, 0x4b57f4) := 0
  zerardados(Byte(1))
  zerardados(Byte(2))
  for p in 1 .. 2:
    jogador[p].at(Double, 0x60) := 0.0
    jogador[p].at(Word, 0x58) := 0
  for i in 1 .. 80: objeto[i].at(Byte, 0x2b) := 0
  TimerEnabled = false
  for p in 1 .. 2:
    for c in 1 .. 15:
      if jogador[p].personagem == c: carregarlutador(lutador(c), Byte(uint8(c)), Byte(uint8(p)))
  if jogador[1].jogando == 1 and jogador[2].jogando == 1:
    var n: Integer
    n := Round(Random(120) / 10) + 1
    cenario := n
    if cenario < 1: cenario := 1
    if cenario > 12: cenario := 12
    if sc != 0:
      if sc < 1: sc := 1
      if sc > 12: sc := 12
      cenario := sc
    carregarcenarioatual()
  musicaatual := 0
  for p in 1 .. 2:
    let j = jogador[p]
    if j.personagem == 1 and j.jogando == 0: musicaatual := 4
    if j.personagem == 4 and j.jogando == 0: musicaatual := 2
    if j.personagem == 2 and j.jogando == 0: musicaatual := 3
    if cenario == 12: musicaatual := 6
  if musicaatual == 0:
    musicaatual := Round(Random(quantmusicas * 10) / 10) + 1
    if musicaatual < 1: musicaatual := 1
    if musicaatual > quantmusicas: musicaatual := quantmusicas
  if musicaatual == 6 and cenario != 12: musicaatual := 1
  carregarmusica(musicaatual)
  TimerEnabled = true

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
      fase := 1
      tempo := 20
      quadros := 25

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
  if tela == 4: tela4()
  if tela == 5: tela5()
  if tela == 6: tela6()
  if tela == 7: lifted.tela7()
  if tela == 8: tela8()
  if tela == 9: tela9()
  if Integer(tela) in [2, 3, 10]: naoportada()
  if tela == 11: tela11()
  if tela == 12: tela12()
  sobreposicao()
  if pausa == 1: TimerEnabled = false
  repeticoes()
  lembrar()
