## The games the core runs: one Nim program, so they share one runtime,
## built into one static library (dopo_nim_library) and called from C
## through include/chavo8/game.h.

import std/strutils
import game/v1/v1

proc chavo8_game_name(version: cint): cstring {.exportc, cdecl.} =
  case version
  of 1: cstring(v1.name)
  else: nil

proc chavo8_v1_line(idioma: cint, memo: cint, index: cint): cstring {.exportc, cdecl.} =
  CarregarTexto(Byte(uint8(idioma)))
  Lines(Memo(memo), index)

proc chavo8_v1_create() {.exportc, cdecl.} =
  FormCreate()

var estado: string

## what FormCreate left, a line each, for the viewer
proc chavo8_v1_state(): cstring {.exportc, cdecl.} =
  var l: seq[string]
  l.add "RandSeed " & $RandSeed & "  velocidade " & $velocidade & " ms  Lines Texto " & $Count(MemoTexto)
  l.add "nivel " & $niveljogo & "  tempo " & $temporound & "  poder " & $poder &
    "  dano " & formatFloat(niveldano, ffDecimal, 2) & "  sangue " & $sangue & "  rounds " & $rounds
  l.add "linguagem " & $linguagem & "  flash " & $flash & "  mostrar nivel " & $mostrarnivel &
    "  creditos " & $mostrarcreditos & "  (" & $MemoConfigSalvo.len & " linhas salvas)"
  l.add "desbloqueios: churros " & $churros & " gentalha " & $gentalha & " satanas " & $satanas &
    " fadiga " & $fadiga & " mentecapto " & $mentecapto & " bruxa " & $bruxa & " sc " & $sc &
    " energeticos " & formatFloat(energeticos, ffDecimal, 2)
  l.add "listas: animacao " & $DXImageListAnimacao.Count & "  efeitos " & $DXImageListEfeitos.Count &
    "/" & $DXImageListEfeitosInvert.Count & "  retrato " & $DXImageListRetrato.Count &
    "  lutador " & $DXImageListLutador.Count & "  icone " & $DXImageListIcone.Count
  l.add "sons " & $DXWaveListSnd.Count & " (quantsons " & $quantsons & ")  musicas " & $quantmusicas &
    "  falas " & $DXWaveListVitoria.Count & "  abertura " & $DXImageListAbertura.Count
  l.add "jogador 1: x " & $jogador[1].x & " y " & $jogador[1].y & " personagem " & $jogador[1].personagem &
    "   jogador 2: x " & $jogador[2].x & " personagem " & $jogador[2].personagem
  for i in 1 .. 10:
    l.add "recorde " & $i & ": " & recorde[i].nome & "  personagem " & $recorde[i].personagem &
      "  " & $recorde[i].pontos & " pts"
  estado = l.join("\n")
  cstring(estado)
