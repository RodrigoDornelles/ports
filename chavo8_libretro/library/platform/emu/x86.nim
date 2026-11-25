## The Nim side of include/chavo8/x86.h, the machine the lifted code runs on:
## its registers, its stack and its memory map.

type X86 {.importc: "x86_t", header: "chavo8/x86.h".} = object
  r: array[8, uint32]

var x86 {.importc: "x86", header: "chavo8/x86.h".}: X86

type Reg* = enum
  eax, ecx, edx, ebx, esp, ebp, esi, edi

proc `[]`*(r: Reg): uint32 = x86.r[ord(r)]
proc `[]=`*(r: Reg, v: uint32) = x86.r[ord(r)] = v

proc push*(v: uint32) {.importc: "push32", header: "chavo8/x86.h".}
proc pop*(): uint32 {.importc: "pop32", header: "chavo8/x86.h".}
proc x86_map(base: uint32, mem: pointer, size: uint32) {.importc, header: "chavo8/x86.h".}

## the bytes of the code's address base..base+size are mem's
proc map*(base: uint32, mem: pointer, size: int) = x86_map(base, mem, uint32(size))

proc rd32*(a: uint32): uint32 {.importc, header: "chavo8/x86.h".}
proc wr32*(a, v: uint32) {.importc, header: "chavo8/x86.h".}
proc x86_heap_reset*() {.importc, header: "chavo8/x86.h".}
proc x86_string(at: uint32, len: var uint32): cstring {.importc, header: "chavo8/x86.h".}
proc x86_hook(target: uint32, fn: proc () {.cdecl.}) {.importc, header: "chavo8/x86.h".}
proc vcl_create*() {.importc, header: "chavo8/x86.h".}
proc vcl_input*() {.importc, header: "chavo8/x86.h".}
proc vcl_language*(language: cint) {.importc, header: "chavo8/x86.h".}

## the AnsiString at at, whether the lifted code made it or the exe has it
proc str*(at: uint32): string =
  var n: uint32
  let p = x86_string(at, n)
  result = newString(n)
  if n > 0: copyMem(addr result[0], p, n)

## the i-th argument on the stack, the callee being Nim's
proc arg*(i: int): uint32 = rd32(esp[] + uint32(4 * i))
proc drop*(bytes: int) = esp[] = esp[] + uint32(bytes)

## a call of the lifted code to target goes to fn instead
proc hook*(target: uint32, fn: proc () {.cdecl.}) = x86_hook(target, fn)
