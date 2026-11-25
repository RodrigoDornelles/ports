## The fight, lifted from the exe (game/assembly.tar.xz, scripts/lift.nim): tela 7
## of DXTimer1Timer and the methods it calls, run on the form vcl.c lays out,
## with the methods already ported taking their calls back.

import ../../platform/emu/[pascal, x86], ../../platform/delphix/delphix
import chavesvar, chavescentrox, x86v1

proc int32(v: uint32): Integer = Integer(cast[int32](v))

proc hookBarra() {.cdecl.} =
  barra(int32(edx[]), int32(ecx[]), int32(arg(2)), int32(arg(1)), TColor(arg(0)))
  drop(12)

proc hookRetangulo() {.cdecl.} =
  retangulo(int32(edx[]), int32(ecx[]), int32(arg(2)), int32(arg(1)), TColor(arg(0)))
  drop(12)

proc hookBarraredonda() {.cdecl.} =
  barraredonda(int32(edx[]), int32(ecx[]), int32(arg(4)), int32(arg(3)), int32(arg(2)),
    TColor(arg(1)), TColor(arg(0)))
  drop(20)

proc hookTexto() {.cdecl.} =
  texto(int32(edx[]), int32(ecx[]), str(arg(3)), char(arg(2) and 0xff), int32(arg(1)), TColor(arg(0)))
  drop(16)

proc hookZerardados() {.cdecl.} =
  zerardados(Byte(edx[] and 0xff))

const
  form = 0x50000000'u32
  frame = 0x10f000'u32  ## DXTimer1Timer's ebp; its locals down to ebp-520h

proc criar*() =
  vcl_create()
  hook(0x48e444, hookBarra)
  hook(0x48e4c0, hookRetangulo)
  hook(0x48e570, hookBarraredonda)
  hook(0x48e720, hookTexto)
  hook(0x49a218, hookZerardados)
  x86v1_lifted()

## tela 7 as DXTimer1Timer had it: its locals zeroed as its prologue left
## them, the strings it made gone with the frame
proc tela7*() =
  x86_heap_reset()
  vcl_language(cint(linguagem))
  vcl_input()
  ebp[] = frame
  esp[] = frame - 0x520
  esi[] = form
  for a in countup(esp[], frame - 4, 4): wr32(a, 0)
  if not call(x86v1.tela7):
    # a path of the fight not done yet: back to the opening, records and
    # settings kept
    tela := 1
    piscar := 0
    contador := 0
    cameray := 0
    jogador[1].jogando := 0
    jogador[2].jogando := 0
