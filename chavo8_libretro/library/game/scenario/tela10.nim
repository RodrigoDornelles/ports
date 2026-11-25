## tela 10: the ending of the winner's character, 2200 frames: its scenes
## behind a black frame, and below them the texts of MemoFinais, a block of
## four lines every 200 frames from the ending's first line on: a code that
## picks the portrait ('A' the first, 'B' the sixth..., '-' none) and three
## lines beside it. Button 1 skips to the end of each 100 frames; then tela 11.
##
## Each ending is a script over the frame c: what happens at a frame, before
## one, from one, after one, or between two (both left out, as the fades).

import std/strutils
import ../../platform/emu/pascal, ../../platform/delphix/delphix
import ../v1/[chavesvar, chavestexto, chavescentrox, lifted]

template c: int = int(contador)

template em(f: int, body: untyped) =
  if c == f: body
template antes(f: int, body: untyped) =
  if c < f: body
template desde(f: int, body: untyped) =
  if c >= f: body
template depois(f: int, body: untyped) =
  if c > f: body
template ate(f: int, body: untyped) =
  if c <= f: body
template entre(a, b: int, body: untyped) =
  if c > a and c < b: body
template durante(a, b: int, body: untyped) =
  if c >= a and c < b: body

proc cenario(n: int) =
  carregarcenario(numerado(if n < 10: "Cenarios/Cenario-0" else: "Cenarios/Cenario-", n, ".bmp"))

proc lutador(p, personagem: int) =
  carregarlutador(numerado(if personagem < 10: "Lutadores/C0" else: "Lutadores/C", personagem, ".bmp"),
    Byte(personagem), Byte(p))

## the fighter p at x on the floor (210), standing unless told
proc desenha(p, x: int, estado = 1, quadro = 0) = desenhalutador(p, x, 210, estado, quadro)

proc imagem(i, x, y: int) = DXImageListFinal.Draw(i, x, y)
proc sombra(l, t, r, b: int) = DXImageListEfeitos.DrawAlpha(0, Rect(l, t, r, b), 70)
proc preto() = barra(0, 0, 400, 300, RGB(0, 0, 0))

## flashes of random colors with two sparks a frame
proc festa() =
  let b = Random(255)
  let g = Random(255)
  barra(50, 50, 350, 220, RGB(255, g, b))
  for _ in 1 .. 2:
    let x = Random(150) + 130
    let y = Random(100) + 100
    criaefeito(1, Real(x), Real(y))

## now and then, the effect of the type between x .. x+dx, y .. y+dy
proc as_vezes(tipo, x, dx, y, dy: int) =
  if Random(8) == 0:
    let ex = Random(dx) + x
    let ey = Random(dy) + y
    criaefeito(tipo, Real(ex), Real(ey))

type Final = tuple[linha: int, roteiro: proc () {.nimcall.}]

var finais: array[1 .. 15, Final]

template final(n, primeira: int, body: untyped) =
  finais[n] = (primeira, proc () {.nimcall.} = body)

final 1, 0:
  em 0:
    cenario 2; lutador 1, 1; lutador 2, 2
  desenha 1, 150; desenha 2, 250

final 2, 44:
  em 0:
    cenario 3; lutador 1, 2; lutador 2, 5
  em 390:
    cenario 1; lutador 2, 1
  antes 400:
    desenha 1, 150; desenha 2, 250
    imagem 0, 175, 165
  entre 350, 400: preto()
  depois 400:
    sombra 100, 203, 170, 217
    imagem 1, 110, 65
    desenha 2, 270

final 3, 88:
  em 0:
    cenario 1; lutador 1, 9; lutador 2, 4
  em 390:
    lutador 2, 1
  antes 400:
    desenha 1, 150, 20, 18; desenha 2, 250
  entre 350, 400: preto()
  entre 400, 1000:
    desenha 1, 150, 20, 18; desenha 2, 300
    depois 800:
      sombra 180, 203, 250, 217
      imagem 2, 170, 65
  durante 1000, 1400: festa()
  durante 1400, 1600:
    desenha 1, 150, 20, 18
    sombra 170, 197, 240, 211
    imagem 3, 165, 90
  desde 1600:
    sombra 120, 197, 190, 211
    imagem 4, 135, 80
    desenha 2, 220
  depois 1800: as_vezes(24, 150, 100, 60, 30)

final 4, 125:
  em 0:
    cenario 1; lutador 1, 4; lutador 2, 1
  if c < 1400 or c >= 1600:
    desenha 1, 150; desenha 2, 250
  if c in 500 ..< 600 or c in 1100 ..< 1200: festa()
  durante 1400, 1600:
    desenha 1, 150
    sombra 200, 197, 270, 211
    imagem 5, 190, 90

final 5, 166:
  em 0:
    cenario 2; lutador 1, 5; lutador 2, 2
  em 590:
    cenario 1; lutador 2, 5
  antes 600:
    desenha 1, 125
    sombra 220, 207, 290, 221
    imagem 6, 195, 120
    antes 530: as_vezes(25, 250, 60, 110, 20)
  entre 560, 600: preto()
  desde 600:
    desenha 2, 280
    sombra 120, 197, 190, 211
    imagem 7, 120, 48

final 6, 210:
  em 0:
    cenario 11; lutador 1, 6; lutador 2, 7
  em 590:
    cenario 3; lutador 2, 5
  antes 600:
    desenha 1, 125; desenha 2, 245, 20, 18
  entre 550, 600: preto()
  desde 600:
    desenha 2, 280
    sombra 120, 197, 190, 211
    imagem 7, 120, 48

final 7, 254:
  em 0:
    cenario 2; lutador 1, 7; lutador 2, 2
  em 1790:
    cenario 1
  antes 1700:
    desenha 1, 125; desenha 2, 235
  durante 1700, 1800: preto()
  desde 1800:
    sombra 120, 197, 190, 211
    imagem 8, 125, 60

final 8, 298:
  em 0:
    cenario 1; lutador 1, 8; lutador 2, 2
  em 1690:
    cenario 2; lutador 1, 2
  antes 1700:
    desenha 1, 125; desenha 2, 235
  durante 1600, 1700: preto()
  desde 1700:
    desenha 1, 105
    sombra 240, 197, 310, 211
    imagem 9, 248, 67
    imagem 10, 180, 190

final 9, 342:
  em 0:
    cenario 2; lutador 1, 9; lutador 2, 3
  em 790:
    cenario 1; lutador 1, 9; lutador 2, 1
  antes 800:
    desenha 1, 125; desenha 2, 235, 20, 18
  entre 700, 800: preto()
  desde 800:
    sombra 120, 197, 190, 211
    imagem 4, 128, 80
    desenha 2, 215

final 10, 386:
  em 0:
    cenario 1; lutador 1, 10; lutador 2, 1
  em 390:
    cenario 3; lutador 1, 10; lutador 2, 4
  antes 400:
    desenha 1, 125; desenha 2, 235, 20, 18
  entre 300, 400: preto()
  desde 400:
    desenha 1, 125; desenha 2, 235

final 11, 430:
  em 0:
    cenario 2; lutador 1, 11; lutador 2, 3
  em 590:
    cenario 5; lutador 1, 1; lutador 2, 11
  antes 600:
    desenha 1, 115; desenha 2, 245
  entre 500, 600: preto()
  desde 600:
    desenha 1, 135; desenha 2, 225

final 12, 474:
  em 0:
    cenario 1; lutador 1, 1; lutador 2, 2
  desenha 1, 135; desenha 2, 225

final 13, 518:
  em 0:
    cenario 1; lutador 1, 13; lutador 2, 1
  desenha 1, 135; desenha 2, 225

final 14, 562:
  em 0:
    cenario 1; lutador 1, 13; lutador 2, 14
  desenha 1, 135; desenha 2, 255

final 15, 606:
  em 0:
    cenario 2; lutador 1, 15; lutador 2, 3
  em 490:
    cenario 1; lutador 2, 2
  em 1800:
    lutador 1, 2; lutador 2, 4
  ate 400:
    desenha 1, 135; desenha 2, 255, 20, 18
  entre 400, 500: preto()
  durante 500, 1800:
    desenha 1, 135, 2; desenha 2, 255
  entre 1800, 1850: preto()
  desde 1850:
    desenha 1, 135; desenha 2, 255

const retratos = "ABCDEFGHIJKLMNO"

proc tela10*() =
  let branco = RGB(255, 255, 255)
  let preta = RGB(0, 0, 0)
  quadro()
  camerax := 0
  cameray := 0
  chao := 210
  jogador[1].x := 100.0
  jogador[1].lado := 1
  jogador[2].x := 200.0
  jogador[2].lado := 2
  if contador == 0:
    if jogador[1].jogando == 1: vencedor := jogador[1].personagem
    if jogador[2].jogando == 1: vencedor := jogador[2].personagem
  if DXImageListCenario.Count > 0: DXImageListCenario.Draw(0, -80, -100)
  var linha = 0
  if Integer(vencedor) in 1 .. 15:
    linha = finais[Integer(vencedor)].linha
    finais[Integer(vencedor)].roteiro()
  desenhaefeitos()
  barra(0, 0, 400, 50, preta)
  barra(0, 220, 400, 300, preta)
  barra(0, 50, 50, 220, preta)
  barra(350, 50, 400, 220, preta)
  var k = 0
  for i in 0 .. 10:
    durante i * 200, i * 200 + 200: k = i
  let codigo = $Lines(MemoFinais, linha + k * 4)
  if codigo == "-": zoom := 99
  elif codigo.len == 1 and codigo[0] in retratos: zoom := retratos.find(codigo[0]) * 5
  if zoom != 99 and DXImageListRetrato.Count > zoom and c < 2200:
    DXImageListRetrato.Draw(zoom, 20, 222)
  if zoom != 99 and c < 2200:
    for j in 1 .. 3:
      texto(80, 211 + j * 17, $Lines(MemoFinais, linha + k * 4 + j), '-', 12, branco)
  inc contador
  if contador > 2200:
    piscar := 0
    contador := 0
    tela := 11
  Update()
  var pulou = 0
  if (isButton1 in Keyboard.States and jogador[1].jogando == 1) or
      (isButton1 in Joystick.States and jogador[2].jogando == 1):
    for i in 0 .. 20:
      entre i * 100 + 5, i * 100 + 85: contador := i * 100 + 85
    pulou = 21
  if depuracao == 1:
    texto(10, 10, "A=" & IntToStr(Integer(pulou)), '-', 8, branco)
    texto(10, 20, "B=" & IntToStr(Integer(linha)), '-', 8, branco)
    texto(10, 30, "C=" & IntToStr(Integer(k)), '-', 8, branco)
    texto(10, 40, "D=" & IntToStr(zoom), '-', 8, branco)
    texto(100, 10, "CICLO=" & IntToStr(Integer(piscar)), '-', 8, branco)
    texto(100, 20, "CICLO2=" & IntToStr(Integer(contador)), '-', 8, branco)
    texto(200, 10, "JOG1=" & IntToStr(Integer(cast[int8](jogador[1].personagem))), '-', 8, branco)
    texto(200, 20, "JOG2=" & IntToStr(Integer(cast[int8](jogador[2].personagem))), '-', 8, branco)
    if jogador[1].jogando == 1: texto(300, 10, "ATIVO", '-', 8, branco)
    if jogador[2].jogando == 1: texto(300, 20, "ATIVO", '-', 8, branco)
