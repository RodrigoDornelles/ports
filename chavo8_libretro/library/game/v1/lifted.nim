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

## a path of the lifted code not done yet: back to the opening, records and
## settings kept
proc falhou() =
  tela := 1
  piscar := 0
  contador := 0
  cameray := 0
  jogador[1].jogando := 0
  jogador[2].jogando := 0

## the frame's start for the lifted code: the strings of the last gone, the
## language and input of this one
proc quadro*() =
  x86_heap_reset()
  vcl_language(cint(linguagem))
  vcl_input()

## tela 7 as DXTimer1Timer had it: its locals zeroed as its prologue left them
proc tela7*() =
  quadro()
  ebp[] = frame
  esp[] = frame - 0x520
  esi[] = form
  for a in countup(esp[], frame - 4, 4): wr32(a, 0)
  if not call(x86v1.tela7): falhou()

## a lifted routine called from Nim as DXTimer1Timer called it: eax, edx and
## ecx, then the stack's arguments pushed in order, then a return address
proc chamar(fn: proc () {.cdecl.}, a, d, c: uint32, pilha: varargs[uint32]) =
  esp[] = frame - 0x520
  for v in pilha: push(v)
  push(0)
  eax[] = a
  edx[] = d
  ecx[] = c
  esi[] = form
  if not call(fn): falhou()

proc alto(v: Real): uint32 = uint32(cast[uint64](float64(v)) shr 32)
proc baixo(v: Real): uint32 = uint32(cast[uint64](float64(v)) and 0xffffffff'u64)

## TForm1.desenhalutador(p, x, y, estado, quadro): the fighter p at x on the
## floor y, in the state (6Bh) and frame (74h) given
proc desenhalutador*(p, x, y, estado, quadro: int) =
  chamar(x86v1.desenhalutador, form, uint32(p), uint32(x) and 0xffff,
    uint32(y), uint32(estado), uint32(quadro))

proc desenhaefeitos*() =
  chamar(x86v1.desenhaefeitos, form, 0, 0)

## ChavesGolpes' effect of the type at x, y (476A04h)
proc criaefeito*(tipo: int, x, y: Real) =
  chamar(x86v1.f476a04, uint32(tipo), 0, 0, alto(x), baixo(x), alto(y), baixo(y))
