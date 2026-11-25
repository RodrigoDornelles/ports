## FormKeyDown, the keys Windows sent the form, as virtual key codes.

import ../../delphix/[pascal, delphix]
import chavesvar, chavescentrox

proc entrar(j: Lutador) =
  j.jogando := 1
  j.escolheu := 0
  j.at(Integer, 0x88) := 0
  j.at(Byte, 0x8c) := 0
  dec creditos

## from the opening: a coin's player in, to the selection
proc comecar(j: Lutador, personagem: Byte) =
  entrar(j)
  j.personagem := personagem
  tela := 4
  fase := 1
  etapa := 1
  if gentalha == 1: etapa := 8
  tempo := 30
  quadros := 25
  contador := 0

proc FormKeyDown*(key: Word) =
  let k = Integer(key)
  let j1 = jogador[1]
  let j2 = jogador[2]
  if key == VK_ESCAPE: Terminate()
  if key == VK_LWIN: Terminate()
  if key == VK_OEM_MINUS and velocidade < 80: inc velocidade
  if key == VK_OEM_PLUS and velocidade >= 1: dec velocidade
  if key == VK_OEM_MINUS or key == VK_OEM_PLUS: mostravelocidade := 30
  # , and . moved the window a pixel, up to 40 either way: the frontend's
  if key == ord('P'):
    if pausa == 0:
      pausa := 1
    else:
      TimerEnabled = true
      pausa := 0
      golpes := 0
  if key == ord('G') and tela == 7:
    if pausa == 0:
      pausa := 1
      golpes := 1
    else:
      TimerEnabled = true
      pausa := 0
      golpes := 0

  if key == VK_F1 and tela == 1:
    tela := 2
    opcao := 1
    testesom := 1
    musicaatual := 1
  elif key == VK_F1 and tela == 2:
    tela := 1
    var l: seq[string]
    l.add IntToStr(niveljogo)
    for t in 1 .. 4:
      if temporound == t: l.add $t
    l.add IntToStr(poder)
    l.add FloatToStr(niveldano)
    if sangue == 0: l.add "0"
    if sangue == 1: l.add "1"
    l.add IntToStr(rounds)
    l.add IntToStr(linguagem)
    for v in [flash, mostrarnivel, mostrarcreditos]:
      if v == 0: l.add "0"
      if v == 1: l.add "1"
    MemoConfigSalvo = l
    SaveConfigLines(chavesDat, MemoConfigSalvo)
    if DXWaveListSnd.Count > testesom: DXWaveListSnd.Stop(testesom)
    if DXWaveListMusica.Count > 0: DXWaveListMusica.Stop(0)
    testesom := 0
    musicaatual := 0

  if key == VK_F2 and tela == 1:
    tela := 3
    opcao := 1
  elif key == VK_F2 and tela == 3:
    tela := 1
  if tela == 3 and (k in ord('A') .. ord('Z') or k in ord('0') .. ord('9') or
      k in 0x25 .. 0x28 or k in 0x60 .. 0x69):
    for s in isUp .. isButton4:
      if opcao == ord(s) + 1: KeyAssigns[s][0] := k
    inc opcao

  if tela == 1 and creditos > 0:
    if key == ord('1') and j1.jogando == 0: comecar(j1, Byte(1))
    if key == ord('2') and j2.jogando == 0: comecar(j2, Byte(2))
  if tela == 4 and creditos > 0 and at(Byte, 0x4b44e0) == 0:
    for (c, j) in [(ord('1'), j1), (ord('2'), j2)]:
      if k == c and j.jogando == 0:
        entrar(j)
        tempo := 30
        quadros := 25
  if tela == 7 and creditos > 0 and at(Byte, 0x4b44e0) == 0:
    for (c, j) in [(ord('1'), j1), (ord('2'), j2)]:
      if k == c and j.jogando == 0:
        entrar(j)
        desafiante := 100
  if tela == 9 and creditos > 0 and tempo > 0 and quadros > 0:
    for (c, j) in [(ord('1'), j1), (ord('2'), j2)]:
      if k == c and j.jogando == 0:
        entrar(j)
        if j.adversario(0) == 0: tela := 4
        if j.adversario(0) != 0: tela := 5
        tempo := 30
        quadros := 25
        piscar := 0
        contador := 0

  if key == ord('5') and creditos < 99:
    inc creditos
    if DXWaveListSnd.Count > 0: DXWaveListSnd.Play(0)
    if tempodemo > 0 or tela == 8:
      if creditos > 0:
        tela := 1
        contador := 500
        piscar := 0
        cameray := -200
        tempodemo := 0
