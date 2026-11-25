## ChavesCentroX: the methods of TForm1, the game's only form, as their
## code reads. Variables not yet understood are at(T, address) of ChavesVar.

import ../../platform/emu/pascal, ../../platform/delphix/delphix
import chavesvar, chavestexto

const
  branco = TColor(0xFFFFFF)
  ## Chaves.dat held one setting per line, in this order; chavo8.ini names them
  chavesDat* = ["niveljogo", "temporound", "poder", "niveldano", "sangue",
    "rounds", "linguagem", "flash", "mostrarnivel", "mostrarcreditos"]

template chao*: untyped = at(SmallInt, 0x4b4498)  ## the floor's y, 300

var
  MemoConfigSalvo*: seq[string]
  versao*: string

## FillRect with the brush's color, right and bottom out
proc barra*(x1, y1, x2, y2: Integer, cor: TColor) =
  FillRect(Rect(x1, y1, x2, y2), cor)

## FrameRect, drawn again from the right when x1 is past x2
proc retangulo*(x1, y1, x2, y2: Integer, cor: TColor) =
  FrameRect(Rect(x1, y1, x2, y2), cor)
  if x1 > x2: FrameRect(Rect(x2, y1, x1, y2), cor)

proc barraredonda*(x1, y1, x2, y2, raio: Integer, borda, cor: TColor) =
  RoundRect(x1, y1, x2, y2, raio, raio, borda, cor)

## Comic Sans MS with a black shadow a pixel down and right; alinhamento 'c'
## centers on x, 'e' ends at x, anything else starts at it
proc texto*(x, y: Integer, s: string, alinhamento: char, tamanho: Integer, cor: TColor) =
  var desloca: Integer = 0
  if alinhamento == 'c': desloca := Round(TextWidth(s, tamanho) / 2)
  if alinhamento == 'e': desloca := TextWidth(s, tamanho)
  TextOut(x - desloca + 1, y + 1, s, tamanho, RGB(0, 0, 0))
  TextOut(x - desloca, y, s, tamanho, cor)

proc zerardados*(p: Byte) =
  let j = jogador[p]
  j.at(Double, 0x40) := 125.0
  j.at(Double, 0x48) := 125.0
  j.at(Byte, 0x6b) := 1
  j.at(Byte, 0x78) := 0
  j.at(Byte, 0x92) := 0
  j.at(Byte, 0x95) := 0
  j.at(Byte, 0x96) := 0
  j.at(Byte, 0x68) := 0
  j.at(Double, 0x50) := 0.0
  j.at(Byte, 0x6a) := 0
  jogador[1].x := 220.0
  jogador[2].x := 380.0
  chao := 300
  j.y := chao
  jogador[1].at(Byte, 0x6d) := 1
  jogador[2].at(Byte, 0x6d) := 2
  jogador[1].at(Byte, 0x91) := 1
  jogador[2].at(Byte, 0x91) := 0
  j.at(Double, 0x80) := 8.0
  if j.personagem == 3: j.at(Double, 0x80) := 8.5
  if j.personagem == 6: j.at(Double, 0x80) := 7.5
  if j.personagem == 7: j.at(Double, 0x80) := 7.5
  if j.personagem == 9: j.at(Double, 0x80) := 8.5
  if j.personagem == 10: j.at(Double, 0x80) := 7.5
  if j.personagem == 13: j.at(Double, 0x80) := 7.5
  if j.personagem == 14: j.at(Double, 0x80) := 7.5
  at(SmallInt, 0x4b44d2) := 0
  at(SmallInt, 0x4b44d4) := 0
  at(Byte, 0x4b57f4) := 0
  at(Integer, 0x4b44c8) := Round((jogador[1].x + jogador[2].x) / 2) - 200
  at(Integer, 0x4b44cc) := Round((jogador[1].y + jogador[2].y) / 8) - 55
  for i in 1 .. 80:
    objeto[i].at(Byte, 0x2b) := 0

proc criarecorde*(i: Byte, nome: string, personagem: Byte, pontos: Integer) =
  recorde[i].nome = nome
  recorde[i].personagem := personagem
  recorde[i].pontos := pontos

proc ordenarecordista*() =
  for i in 1 .. 9:
    for j in i + 1 .. 10:
      if recorde[i].pontos < recorde[j].pontos:
        var t: array[12, uint8]
        copyMem(addr t, addr recorde[i].at(uint8, 0), 12)
        copyMem(addr recorde[i].at(uint8, 0), addr recorde[j].at(uint8, 0), 12)
        copyMem(addr recorde[j].at(uint8, 0), addr t, 12)

## the loaders add their item first, so a missing file leaves it empty

proc carregarfala*(nome: string) =
  DXWaveListVitoria.Add
  var n: Byte
  n := DXWaveListVitoria.Count
  if FileSearch(nome, "") != "":
    DXWaveListVitoria.LoadFromFile(Integer(n) - 1, nome)

proc carregaranimacao*(nome: string) =
  DXImageListAnimacao.Add
  if FileSearch(nome, "") != "":
    let i = DXImageListAnimacao.Count - 1
    ImageRetrato.LoadFromFile(nome)
    DXImageListAnimacao.Assign(i, ImageRetrato, true, branco)

proc carregareftsonoro*(nome: string) =
  DXWaveListSnd.Add
  var n: Byte
  n := DXWaveListSnd.Count
  if FileSearch(nome, "") != "":
    DXWaveListSnd.LoadFromFile(Integer(n) - 1, nome)

proc carregarmusica*(n: Byte) =
  var nome: string
  if n < 10: nome = "Musicas/Musica-0" & IntToStr(n) & ".wav"
  if n >= 10: nome = "Musicas/Musica-" & IntToStr(n) & ".wav"
  if FileSearch(nome, "") != "":
    DXWaveListMusica.LoadFromFile(0, nome)
    musicaatual := n

## the mirrored copy: CopyRect from w-1 back to 0, a column less than it fills
proc espelharetrato(): (SmallInt, SmallInt) =
  var w, h: SmallInt
  w := ImageRetrato.Width
  h := ImageRetrato.Height
  ImageRetratoInvert.Width = w
  ImageRetratoInvert.Height = h
  CopyRect(ImageRetratoInvert, Rect(0, 0, w, h), ImageRetrato, Rect(w - 1, 0, 0, h))
  (w, h)

proc carregarefeitos*(nome: string) =
  DXImageListEfeitos.Add
  DXImageListEfeitosInvert.Add
  if FileSearch(nome, "") != "":
    ImageRetrato.LoadFromFile(nome)
    DXImageListEfeitos.Assign(DXImageListEfeitos.Count - 1, ImageRetrato, true, branco)
    ImageRetrato.LoadFromFile(nome)
    discard espelharetrato()
    DXImageListEfeitosInvert.Assign(DXImageListEfeitosInvert.Count - 1, ImageRetratoInvert, true, branco)

proc carregarosto*(nome: string, lado: Byte) =
  DXImageListRetrato.Add
  if FileSearch(nome, ".") != "":
    let i = DXImageListRetrato.Count - 1
    ImageRetrato.LoadFromFile(nome)
    discard espelharetrato()
    if lado == 1: DXImageListRetrato.Assign(i, ImageRetrato, true, branco)
    if lado == 2: DXImageListRetrato.Assign(i, ImageRetratoInvert, true, branco)

## a frame of the fighter's strip, all rows but the last, and its mirror
proc copiabmp*(indice, x1, y1, x2, y2: Integer) =
  let w = x2 - x1
  let h = y2 - y1
  ImageLutadorFrame.Width = w
  ImageLutadorFrame.Height = h
  CopyRect(ImageLutadorFrame, Rect(0, 0, w, h), ImageLutador, Rect(x1, y1, x2, y2))
  DXImageListLutador.Assign(indice, ImageLutadorFrame, true, branco)
  CopyRect(ImageLutadorFrame, Rect(0, 0, w, h), ImageLutador, Rect(x2 - 1, y1, x1, y2))
  DXImageListLutadorInvert.Assign(indice, ImageLutadorFrame, true, branco)

## the strip's frames end at columns dark (R, G and B up to 100) at the top
## and bottom rows; past the right edge Pixels reads white, so a strip ends
## at its last dark column. Player 1's frames go from item 1, player 2's 51
proc carregarlutador*(nome: string, personagem: Byte, p: Byte) =
  if FileSearch(nome, ".") != "":
    ImageLutador.LoadFromFile(nome)
    var base: Byte
    if p == 1: base := 0
    if p == 2: base := 50
    let j = jogador[p]
    j.personagem := personagem
    if personagem == 1: j.altura := 156
    if j.personagem == 2: j.altura := 162
    if j.personagem == 3: j.altura := 147
    if j.personagem == 4: j.altura := 149
    if j.personagem == 5: j.altura := 144
    if j.personagem == 6: j.altura := 190
    if j.personagem == 7: j.altura := 160
    if j.personagem == 8: j.altura := 160
    if j.personagem == 9: j.altura := 150
    if j.personagem == 10: j.altura := 155
    if j.personagem == 11: j.altura := 155
    if j.personagem == 12: j.altura := 165
    if j.personagem == 13: j.altura := 155
    if j.personagem == 14: j.altura := 160
    if j.personagem == 15: j.altura := 165
    var inicio: SmallInt
    var quadro: Byte
    inicio := 0
    quadro := 1
    var largura, altura: SmallInt
    largura := ImageLutador.Width
    altura := ImageLutador.Height
    var x: SmallInt
    x := 0
    while x <= largura:
      let cima = ImageLutador.Pixels(x, 0)
      let baixo = ImageLutador.Pixels(x, altura - 1)
      if GetRValue(cima) <= 100 and GetGValue(cima) <= 100 and GetBValue(cima) <= 100 and
          GetRValue(baixo) <= 100 and GetGValue(baixo) <= 100 and GetBValue(baixo) <= 100:
        copiabmp(quadro + base, inicio, 0, x - 1, altura - 1)
        inc quadro
        inicio := x + 1
      inc x
  TimerEnabled = true

proc carregarcenario*(nome: string) =
  TimerEnabled = false
  if FileSearch(nome, ".") != "":
    DXImageListCenario.LoadFromFile(0, nome, false, branco)
  at(Byte, 0x4b57d8 + 0x18) := 0
  if cenario == 2:
    at(Integer, 0x4b57d8) := 395
    at(Integer, 0x4b57d8 + 0x04) := 195
    at(Integer, 0x4b57d8 + 0x08) := 25
    at(Integer, 0x4b57d8 + 0x0c) := 25
    at(Integer, 0x4b57d8 + 0x10) := 110
    at(Integer, 0x4b57d8 + 0x14) := 90
    at(Byte, 0x4b57d8 + 0x18) := 6
  TimerEnabled = true

proc numerado*(prefixo: string, i: int, sufixo: string): string =
  prefixo & $i & sufixo

## the settings, from chavo8.ini or else the original's Chaves.dat; StrToInt
## and StrToFloat raised on a bad line and the rest of FormCreate was lost,
## here only the rest of the settings is
proc lerconfiguracao() =
  MemoConfigSalvo = ConfigLines(chavesDat)
  if MemoConfigSalvo.len == 0:
    if FileSearch("Chaves.dat", ".") == "": return
    MemoConfigSalvo = LoadLines("Chaves.dat")
  let l = MemoConfigSalvo
  try:
    if l.len >= 1: niveljogo := StrToInt(l[0])
    if l.len >= 2:
      if l[1] == "1": temporound := 1
      if l[1] == "2": temporound := 2
      if l[1] == "3": temporound := 3
      if l[1] == "4": temporound := 4
    if l.len >= 3: poder := StrToInt(l[2])
    if l.len >= 4: niveldano := StrToFloat(l[3])
    if l.len >= 5:
      if l[4] == "0": sangue := 0
      if l[4] == "1": sangue := 1
    if l.len >= 6: rounds := StrToInt(l[5])
    if l.len >= 7: linguagem := StrToInt(l[6])
    if l.len >= 8:
      if l[7] == "0": flash := 0
      if l[7] == "1": flash := 1
    if l.len >= 9:
      if l[8] == "0": mostrarnivel := 0
      if l[8] == "1": mostrarnivel := 1
    if l.len >= 10:
      if l[9] == "0": mostrarcreditos := 0
      if l[9] == "1": mostrarcreditos := 1
  except EConvertError:
    discard

proc FormCreate*() =
  Randomize()
  churros := 0
  gentalha := 0
  satanas := 0
  energeticos := 0.0
  fadiga := 0
  mentecapto := 0
  bruxa := 0
  sc := 0
  if FileSearch("Churros.dat", ".") != "": churros := 1
  if FileSearch("Gentalha.dat", ".") != "": gentalha := 1
  if FileSearch("Energeticos.dat", ".") != "": energeticos := 0.15
  if FileSearch("Satanas.dat", ".") != "": satanas := 1
  if FileSearch("Fadiga.dat", ".") != "": fadiga := 1
  if FileSearch("Mentecapto.dat", ".") != "": mentecapto := 1
  if FileSearch("Bruxa.dat", ".") != "": bruxa := 1
  for i in 1 .. 12:
    if i < 10 and FileSearch(numerado("SC0", i, ".dat"), ".") != "": sc := i
    if i >= 10 and FileSearch(numerado("SC", i, ".dat"), ".") != "": sc := i

  versao = lit(0x49dff4)
  at(Integer, 0x4b4488) := 0
  at(Integer, 0x4b448c) := 0
  at(Integer, 0x4b4490) := 0
  at(Integer, 0x4b4494) := 0
  at(Byte, 0x4b44d6) := 0
  at(Byte, 0x4b44c0) := 0
  at(Word, 0x4b44c4) := 0
  at(Integer, 0x4b44c8) := 0
  at(Integer, 0x4b44cc) := 0
  fase := 1
  at(Byte, 0x4b449e) := 1
  tempo := 30
  quadros := 25
  at(Word, 0x4b57f6) := 0
  depuracao := 0
  at(Byte, 0x4b58a5) := 0
  at(Byte, 0x4b58a6) := 0
  at(Byte, 0x4b58a7) := 0
  at(Word, 0x4b580c) := 0
  at(Word, 0x4b57d2) := 330
  at(Double, 0x4b5890) := 2.0
  at(Byte, 0x4b5808) := 0
  at(Byte, 0x4b44db) := 1
  at(Byte, 0x4b44dd) := 1
  at(Byte, 0x4b44de) := 0
  at(Byte, 0x4b44df) := 0
  at(Byte, 0x4b44e0) := 1
  for p in 1 .. 2:
    let j = jogador[p]
    j.at(Double, 0x60) := 0.0
    j.at(Word, 0x58) := 0
    j.at(Byte, 0x8c) := 0
    j.at(Integer, 0x88) := 0
    for k in 0 .. 7: j.at(Byte, 0xa0 + k) := 1
  velocidade := 28
  at(Byte, 0x4b449c) := 4
  poder := 1
  niveljogo := 4
  niveldano := 0.9
  sangue := 1
  rounds := 2
  temporound := 3
  linguagem := 1
  flash := 1
  mostrarnivel := 1
  mostrarcreditos := 1

  lerconfiguracao()
  if niveljogo < 1: niveljogo := 1
  if niveljogo > 8: niveljogo := 8
  if temporound < 1: temporound := 1
  if temporound > 4: temporound := 4
  if poder < 1: poder := 1
  if poder > 4: poder := 4
  if 0.3 > niveldano: niveldano := 0.3
  if niveldano > 2.0: niveldano := 2.0
  if rounds < 1: rounds := 1
  if rounds > 3: rounds := 3
  if linguagem < 1: linguagem := 1
  if linguagem > 2: linguagem := 2
  CarregarTexto(linguagem)

  at(Byte, 0x4b44da) := 0
  jogador[1].personagem := 1
  jogador[2].personagem := 2
  jogador[1].escolheu := 0
  jogador[2].escolheu := 0
  jogador[1].jogando := 0
  jogador[2].jogando := 0
  zerardados(Byte(1))
  zerardados(Byte(2))
  criarecorde(Byte(1), "CHV", Byte(1), 10000)
  criarecorde(Byte(2), "SMD", Byte(2), 8000)
  criarecorde(Byte(3), "CHQ", Byte(3), 7500)
  criarecorde(Byte(4), "QUI", Byte(4), 7000)
  criarecorde(Byte(5), "DFL", Byte(5), 6500)
  criarecorde(Byte(6), "PGR", Byte(6), 6000)
  criarecorde(Byte(7), "SBR", Byte(7), 5500)
  criarecorde(Byte(8), "B71", Byte(8), 5000)
  criarecorde(Byte(9), "PTY", Byte(9), 4500)
  criarecorde(Byte(10), "NHO", Byte(10), 4000)
  ordenarecordista()
  # the form and DXDraw1 were sized to 400x300 and centered here: the frontend's

  for i in 1 .. 78:
    if i < 10: carregaranimacao(numerado("Animacoes/Anim-0", i, ".bmp"))
    if i >= 10: carregaranimacao(numerado("Animacoes/Anim-", i, ".bmp"))
  for i in 1 .. 102:
    if i < 10: carregarefeitos(numerado("Efeitos/Efeito-00", i, ".bmp"))
    if i >= 10 and i < 100: carregarefeitos(numerado("Efeitos/Efeito-0", i, ".bmp"))
    if i >= 100: carregarefeitos(numerado("Efeitos/Efeito-", i, ".bmp"))

  # the waves are counted with the name of the one before, so the loop loads
  # one past the last file and stops after it; the count drops those two
  var i = 1
  var nome = numerado(if i < 10: "Sons/Som-0" else: "Sons/Som-", i, ".wav")
  while FileSearch(nome, "") != "":
    nome = numerado(if i < 10: "Sons/Som-0" else: "Sons/Som-", i, ".wav")
    carregareftsonoro(nome)
    inc i
  quantsons := i
  if quantsons > 3: quantsons := quantsons - 2
  i = 1
  nome = numerado(if i < 10: "Musicas/Musica-0" else: "Musicas/Musica-", i, ".wav")
  while FileSearch(nome, "") != "":
    nome = numerado(if i < 10: "Musicas/Musica-0" else: "Musicas/Musica-", i, ".wav")
    inc i
  quantmusicas := i
  if quantmusicas > 3: quantmusicas := quantmusicas - 2

  for i in 1 .. 15:
    if i < 10:
      carregarfala(numerado("Falas/Fala-0", i, "-a.wav"))
      carregarfala(numerado("Falas/Fala-0", i, "-b.wav"))
    if i >= 10:
      carregarfala(numerado("Falas/Fala-", i, "-a.wav"))
      carregarfala(numerado("Falas/Fala-", i, "-b.wav"))
  for i in 1 .. 15:
    let r = if i < 10: "Rostos/R0" else: "Rostos/R"
    carregarosto(numerado(r, i, "-a.bmp"), Byte(1))
    carregarosto(numerado(r, i, "-a.bmp"), Byte(2))
    carregarosto(numerado(r, i, "-b.bmp"), Byte(1))
    carregarosto(numerado(r, i, "-b.bmp"), Byte(2))
    carregarosto(numerado(r, i, "-c.bmp"), Byte(1))
  for k in 1 .. 150:
    DXImageListLutador.Add
    DXImageListLutadorInvert.Add

## the one player ladder: seven opponents drawn among those left, never the
## fighter itself, its last opponent nor the ones each fighter is spared,
## then that last one
proc criaadversario*(p: Byte, personagem: Byte) =
  const
    chefes = [2, 5, 9, 1, 2, 7, 2, 5, 3, 1, 3, 1, 5, 5, 3]
    poupados = [@[3, 9], @[3, 8, 9, 14, 15], @[1, 2, 14], @[5, 6], @[4, 6], @[4, 5, 15],
      @[10, 8], @[2, 7], @[1, 2, 15], @[7, 9], @[1, 9], @[5, 8], @[1, 8, 14], @[2, 3, 13], @[1, 2, 9]]
  let j = jogador[p]
  for k in 0 .. 7: j.adversario(k) := 1
  for c in 1 .. 15:
    if personagem == c: j.chefe := chefes[c - 1]
  var livre: array[1 .. 15, bool]
  for c in 1 .. 15: livre[c] = true
  livre[Integer(j.chefe)] = false
  for c in 1 .. 15:
    if personagem == c:
      for n in poupados[c - 1]: livre[n] = false
  for k in 0 .. 6:
    var n = Random(15) + 1
    while not livre[n] or personagem == n:
      n = Random(15) + 1
    j.adversario(k) := n
    livre[n] = false
