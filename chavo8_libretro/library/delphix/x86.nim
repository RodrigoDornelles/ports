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
