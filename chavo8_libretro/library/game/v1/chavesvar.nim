## ChavesVar: the game's globals, kept as the very bytes the original laid
## out from 004B4488 to 004B58B4 (Delphi allocates a unit's variables in
## their declaration order), so a snapshot is a copy of them and every
## variable sits at the address the disassembly shows. Those understood
## have a name; the others are reached as at(T, address) until they are.

import ../../platform/emu/[pascal, x86], ../../platform/delphix/delphix

const
  Base = 0x4b4488
  Size = 0x4b58b4 - Base

var
  chavesVar* {.align(16).}: array[Size, uint8]
  outrasUnits* {.align(16).}: array[0x4b5900 - 0x4b58b4, uint8]  ## the other units' variables, after these

map(Base, addr chavesVar, Size)
map(0x4b58b4, addr outrasUnits, outrasUnits.len)

template at*(T: typedesc, address: int): untyped =
  cast[ptr T](addr chavesVar[address - Base])[]

template zoom*: untyped = at(Integer, 0x4b4494)         ## the logo's half height while it grows
template opcao*: untyped = at(Byte, 0x4b449e)            ## the menus' cursor: resolution, settings, keys
template piscar*: untyped = at(Byte, 0x4b44c0)           ## a counter of frames that blinks texts
template mostravelocidade*: untyped = at(Byte, 0x4b44c1) ## frames left to show the speed bar
template contador*: untyped = at(SmallInt, 0x4b44c4)     ## frames into the current screen
template camerax*: untyped = at(Integer, 0x4b44c8)
template cameray*: untyped = at(Integer, 0x4b44cc)
template velocidade*: untyped = at(Word, 0x4b44d0)       ## DXTimer1.Interval in ms, - and + in game
template tela*: untyped = at(Byte, 0x4b44d6)             ## the screen DXTimer1Timer draws, 0 to 12
template fase*: untyped = at(Byte, 0x4b44d7)          ## a screen's step, 1 while it slides in
template cenario*: untyped = at(Byte, 0x4b44d8)          ## Cenarios/Cenario-n.bmp loaded
template niveljogo*: untyped = at(Byte, 0x4b44d9)        ## Nível do Jogo, 1 to 8
template creditos*: untyped = at(Byte, 0x4b44da)
template rounds*: untyped = at(Byte, 0x4b44dc)           ## 1 to 3
template etapa*: untyped = at(Byte, 0x4b44dd)            ## the one player ladder's fight, 1 to 8
template sangue*: untyped = at(Byte, 0x4b44e1)
template tempo*: untyped = at(Byte, 0x4b44e3)         ## seconds of the countdown on screen
template quadros*: untyped = at(Byte, 0x4b44e4)       ## frames left of its second, 25
template poder*: untyped = at(Byte, 0x4b44e2)            ## NORMAL, 1P MAX, 2P MAX, MAX
template desafiante*: untyped = at(Byte, 0x4b57fa)       ## frames left of "someone entered the fight"
template testesom*: untyped = at(Byte, 0x4b5809)         ## the Sons/Som-n.wav of the settings' test
template musicaatual*: untyped = at(Byte, 0x4b580a)      ## the Musicas/Musica-n.wav loaded
template tempodemo*: untyped = at(SmallInt, 0x4b580c)    ## frames left of the demonstration fight
template niveldano*: untyped = at(Double, 0x4b5888)      ## Nível de Dano, 0.3 to 2
template energeticos*: untyped = at(Double, 0x4b5898)    ## 0.15 with Energeticos.dat
template temporound*: untyped = at(Byte, 0x4b58a0)       ## 30, 60, 90 seconds or none
template linguagem*: untyped = at(Byte, 0x4b58a1)        ## 1 Português, 2 Español
template quantsons*: untyped = at(Byte, 0x4b58a2)
template quantmusicas*: untyped = at(Byte, 0x4b58a3)
template depuracao*: untyped = at(Byte, 0x4b58a4)        ## the author's debug HUD, never on in the release
template golpes*: untyped = at(Byte, 0x4b58a6)           ## the pause shows the moves (G)
template pausa*: untyped = at(Byte, 0x4b58a7)
template flash*: untyped = at(Byte, 0x4b58a8)
template mostrarnivel*: untyped = at(Byte, 0x4b58a9)
template mostrarcreditos*: untyped = at(Byte, 0x4b58aa)
template churros*: untyped = at(Byte, 0x4b58ab)          ## the unlocks: their .dat beside the exe
template gentalha*: untyped = at(Byte, 0x4b58ac)
template satanas*: untyped = at(Byte, 0x4b58ad)
template fadiga*: untyped = at(Byte, 0x4b58ae)
template mentecapto*: untyped = at(Byte, 0x4b58af)
template bruxa*: untyped = at(Byte, 0x4b58b0)
template sc*: untyped = at(Byte, 0x4b58b1)               ## the highest SC01..SC12.dat found

## the two fighters, array[1..2] of 176 bytes at 004B44E8
type
  Jogadores = object
  Lutador* = distinct Integer

const jogador* = Jogadores()

template `[]`*(j: Jogadores, p: untyped): Lutador = Lutador(p)
template at*(l: Lutador, T: typedesc, offset: int): untyped =
  at(T, 0x4b44e8 + (Integer(l) - 1) * 176 + offset)

template x*(l: Lutador): untyped = l.at(Double, 0x00)
template y*(l: Lutador): untyped = l.at(Double, 0x08)
template personagem*(l: Lutador): untyped = l.at(Byte, 0x10)
template altura*(l: Lutador): untyped = l.at(SmallInt, 0x7a)
template repetesoco*(l: Lutador): untyped = l.at(Byte, 0x9c)
template jogando*(l: Lutador): untyped = l.at(Byte, 0x9e)      ## a person plays it (1 and 2 are refused then)
template escolheu*(l: Lutador): untyped = l.at(Byte, 0x9f)
template adversario*(l: Lutador, k: untyped): untyped = l.at(Byte, 0xa0 + k)  ## 0 to 6, the ladder
template chefe*(l: Lutador): untyped = l.at(Byte, 0xa7)          ## the last opponent
template repetechute*(l: Lutador): untyped = l.at(Byte, 0x9d)
template pontos*(l: Lutador): untyped = l.at(Cardinal, 0x88)
template cursor*(l: Lutador): untyped = l.at(Byte, 0x8e)          ## the letter of the name being chosen, 1 to 3
template letra*(l: Lutador, k: untyped): untyped = l.at(Byte, 0x11 + k)  ## the name, String[3] at 11h

proc nome*(l: Lutador): string =
  let n = min(int(l.at(uint8, 0x11)), 3)
  for i in 1 .. n: result.add char(l.at(uint8, 0x11 + i))

## the device's states of the frame before: isButton1..4 at A8h, isUp,
## isDown, isLeft, isRight at ACh
template antes*(l: Lutador, s: untyped): untyped =
  l.at(Byte, (if s >= isButton1: 0xa8 + ord(s) - ord(isButton1) else: 0xac + ord(s)))

## array[1..80] of 56 bytes at 004B4648
type
  Objetos = object
  Objeto* = distinct Integer

const objeto* = Objetos()

template `[]`*(o: Objetos, i: untyped): Objeto = Objeto(i)
template at*(o: Objeto, T: typedesc, offset: int): untyped =
  at(T, 0x4b4648 + (Integer(o) - 1) * 56 + offset)

## the best ten, array[1..10] of record nome: String[3]; personagem: Byte;
## pontos: Cardinal end at 004B5810
type
  Recordes = object
  Recorde* = distinct Integer

const recorde* = Recordes()

template `[]`*(r: Recordes, i: untyped): Recorde = Recorde(i)
template at*(r: Recorde, T: typedesc, offset: int): untyped =
  at(T, 0x4b5810 + (Integer(r) - 1) * 12 + offset)

template personagem*(r: Recorde): untyped = r.at(Byte, 4)
template pontos*(r: Recorde): untyped = r.at(Cardinal, 8)

proc nome*(r: Recorde): string =
  let n = min(int(r.at(uint8, 0)), 3)
  for i in 1 .. n: result.add char(r.at(uint8, i))

## a short string assignment copies the length byte and the characters kept
proc `nome=`*(r: Recorde, s: string) =
  let n = min(s.len, 3)
  r.at(uint8, 0) = uint8(n)
  for i in 1 .. n: r.at(uint8, i) = uint8(s[i - 1])
