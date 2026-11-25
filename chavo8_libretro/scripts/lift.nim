## lift: the x86 of game/v1/dump (scripts/dump.nim) as C, at build time.
##
## usage: lift <module> <dir> <dump.s>...
##
## Writes <dir>/<module>.c, a function per dumped one over the machine of
## include/chavo8/x86.h, and <dir>/<module>.nim, their declarations for the
## Nim ports. An instruction is a line or two on the registers and memory;
## reads of a fixed address that is not a variable (constants, the pointers
## to the units' variables) take the value the dump recorded; the flags are
## only kept when something reads them, a cmp or test right before the jump
## that reads them becoming the comparison itself. Big functions are cut in
## chunks of blocks, as compilers slow down a lot on huge ones: a jump within
## a chunk is a goto, out of it the chunk returns where to go. An instruction
## not understood stops the build.

import std/[algorithm, os, sequtils, sets, strformat, strutils, tables]

type
  Insn = object
    va: uint32
    m: string
    ops: seq[string]
    text: string
  Fn = object
    name: string
    entry: uint32
    insns: seq[Insn]

const chunk = 150

var
  data: Table[uint32, string]
  strings: Table[uint32, string]
  fns: seq[Fn]
  byEntry: Table[uint32, string]

proc fail(i: Insn, why: string) =
  quit &"lift: {i.va:x}: {i.text}: {why}"

proc hexBytes(s: string): string =
  for i in countup(0, s.len - 2, 2): result.add char(parseHexInt(s[i .. i + 1]))

proc parse(path: string) =
  var fn = Fn(name: path.splitFile.name)
  for line in lines(path):
    if line.startsWith("; entry "):
      fn.entry = parseHexInt(line[8 .. ^1]).uint32
    elif line.startsWith(".data "):
      let p = line.splitWhitespace
      data[parseHexInt(p[1]).uint32] = hexBytes(p[2])
    elif line.startsWith(".string "):
      let p = line.splitWhitespace
      strings[parseHexInt(p[1]).uint32] = (if p.len > 2: hexBytes(p[2]) else: "")
    elif line.len > 0 and line[0] != ';':
      let colon = line.find(": ")
      let text = line[colon + 2 .. ^1]
      let sp = text.find(' ')
      var i = Insn(va: parseHexInt(line[0 ..< colon]).uint32, text: text)
      i.m = if sp < 0: text else: text[0 ..< sp]
      if sp >= 0:
        var depth = 0
        var cur = ""
        for c in text[sp + 1 .. ^1]:
          if c == '(' or c == '[': inc depth
          if c == ')' or c == ']': dec depth
          if c == ',' and depth == 0:
            i.ops.add cur
            cur = ""
          else: cur.add c
        i.ops.add cur
      fn.insns.add i
  fns.add fn
  byEntry[fn.entry] = fn.name

# operands

const
  regs32 = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"]
  regs16 = ["ax", "cx", "dx", "bx", "sp", "bp", "si", "di"]
  regs8l = ["al", "cl", "dl", "bl"]
  regs8h = ["ah", "ch", "dh", "bh"]

proc u(v: uint32): string = &"0x{v:x}u"

## a C string literal; octal escapes, as hex ones run on into the next digits
proc cstr(s: string): string =
  result = "\""
  for c in s:
    if c in {' ' .. '~'} and c notin {'"', '\\', '?'}: result.add c
    else: result.add "\\" & toOct(ord(c), 3)
  result.add "\""

proc regSize(r: string): int =
  if r in regs32: 32 elif r in regs16: 16 elif r in regs8l or r in regs8h: 8 else: 0

proc full(r: string): string =
  if r in regs32: r
  elif r in regs16: regs32[regs16.find(r)]
  elif r in regs8l: regs32[regs8l.find(r)]
  else: regs32[regs8h.find(r)]

proc sizeOf(op: string): int =
  if op.startsWith("BYTE"): 8
  elif op.startsWith("WORD"): 16
  elif op.startsWith("DWORD"): 32
  elif op.startsWith("QWORD"): 64
  elif op.startsWith("TBYTE"): 80
  else: regSize(op)

proc bitsOf(i: Insn): int =
  for op in i.ops:
    let n = sizeOf(op)
    if n > 0: return n
  32

proc isImm(op: string): bool = op.startsWith("0x") or (op.len > 0 and op.allIt(it in Digits))
proc imm(op: string): uint32 = uint32(parseHexInt(op) and 0xffff_ffff)
proc mask(bits: int): string = (if bits == 32: "" else: " & 0x" & toHex((1'u64 shl bits) - 1).strip(trailing = false, chars = {'0'}) & "u")

## an absolute address: ds:N or [N]
proc absolute(op: string): int64 =
  let i = op.find("ds:")
  if i >= 0: return parseHexInt(op[i + 3 .. ^1])
  let a = op.find('[')
  if a < 0: return -1
  let inner = op[a + 1 ..< op.find(']')]
  if inner.startsWith("0x") and "+" notin inner and "-" notin inner: parseHexInt(inner) else: -1

proc address(i: Insn, op: string): string =
  if "fs:" in op: i.fail("fs is reached as fs0")
  let abs = absolute(op)
  if abs >= 0: return u(uint32(abs))
  let inner = op[op.find('[') + 1 ..< op.find(']')]
  var terms: seq[string]
  var cur = ""
  for c in inner:
    if (c == '+' or c == '-') and cur.len > 0:
      terms.add cur
      cur = ""
    cur.add c
  terms.add cur
  var e = ""
  for t in terms:
    let neg = t.startsWith("-")
    let v = t.strip(chars = {'+', '-'})
    var term: string
    if "*" in v:
      let f = v.split("*")
      term = &"{f[0]} * {f[1]}u"
    elif v in regs32: term = v
    elif v.startsWith("0x"): term = u(imm(v))
    else: i.fail("address term " & v)
    e.add (if e.len == 0: (if neg: "0u - " & term else: term) else: (if neg: " - " else: " + ") & term)
  "(uint32_t)(" & e & ")"

proc bss(a: uint32): bool = a >= 0x4b2000'u32 and a < 0x4b6000'u32

proc constant(i: Insn, a: uint32, bits: int): uint64 =
  if a notin data: i.fail(&"no data recorded at {a:x}")
  let b = data[a]
  for k in countdown(min(bits div 8, 8) - 1, 0): result = result shl 8 or uint64(b[k].uint8)

proc read(i: Insn, op: string): string =
  if op in regs32: return op
  if op in regs16: return &"({full(op)} & 0xffffu)"
  if op in regs8l: return &"({full(op)} & 0xffu)"
  if op in regs8h: return &"(({full(op)} >> 8) & 0xffu)"
  if isImm(op): return u(imm(op))
  if "fs:" in op: return "x86.fs0"
  let n = if sizeOf(op) > 0: sizeOf(op) else: bitsOf(i)
  let abs = absolute(op)
  if abs >= 0 and not bss(uint32(abs)):
    return u(uint32(constant(i, uint32(abs), n)))
  let a = address(i, op)
  case n
  of 8: &"rd8({a})"
  of 16: &"rd16({a})"
  of 32: &"rd32({a})"
  else: (i.fail("read size"); "")

proc write(i: Insn, op, v: string): string =
  if op in regs32: return &"{op} = {v};"
  if op in regs16: return &"{full(op)} = ({full(op)} & 0xffff0000u) | (({v}) & 0xffffu);"
  if op in regs8l: return &"{full(op)} = ({full(op)} & 0xffffff00u) | (({v}) & 0xffu);"
  if op in regs8h: return &"{full(op)} = ({full(op)} & 0xffff00ffu) | ((({v}) & 0xffu) << 8);"
  if "fs:" in op: return &"x86.fs0 = {v};"
  let abs = absolute(op)
  if abs >= 0 and not bss(uint32(abs)): i.fail("a write to a constant")
  let a = address(i, op)
  case (if sizeOf(op) > 0: sizeOf(op) else: bitsOf(i))
  of 8: &"wr8({a}, {v});"
  of 16: &"wr16({a}, {v});"
  of 32: &"wr32({a}, {v});"
  else: (i.fail("write size"); "")

# the x87 operands

proc extended(b: string): string =
  var m: uint64 = 0
  for k in countdown(7, 0): m = m shl 8 or uint64(b[k].uint8)
  let e = int(b[9].uint8) shl 8 or int(b[8].uint8)
  if (e and 0x7fff) == 0: return "0.0"
  var v = float64(m)
  var k = (e and 0x7fff) - 16383 - 63
  while k > 0: (v *= 2.0; dec k)
  while k < 0: (v /= 2.0; inc k)
  (if (e and 0x8000) != 0: "-" else: "") & formatFloat(v, ffScientific, 17)

proc fval(i: Insn, op: string): string =
  if op == "st": return "ST(0)"
  if op.startsWith("st("): return &"ST({op[3]})"
  let n = sizeOf(op)
  let abs = absolute(op)
  if abs >= 0 and not bss(uint32(abs)):
    case n
    of 32: return &"f32({u(uint32(constant(i, uint32(abs), 32)))})"
    of 64: return &"f64(0x{constant(i, uint32(abs), 64):x}ull)"
    of 80: return extended(data[uint32(abs)])
    else: i.fail("x87 size")
  let a = address(i, op)
  case n
  of 32: &"f32(rd32({a}))"
  of 64: &"f64(rd64({a}))"
  else: (i.fail("x87 size"); "")

proc fint(i: Insn, op: string): string =
  let a = address(i, op)
  case sizeOf(op)
  of 16: &"(double)(int16_t)rd16({a})"
  of 32: &"(double)(int32_t)rd32({a})"
  of 64: &"(double)(int64_t)rd64({a})"
  else: (i.fail("fild size"); "")

proc cond(m: string): string =
  case (if m.startsWith("set"): "j" & m[3 .. ^1] else: m)
  of "je", "jz": "zf()"
  of "jne", "jnz": "!zf()"
  of "jb", "jc": "cf()"
  of "jae", "jnc": "!cf()"
  of "jbe": "(cf() || zf())"
  of "ja": "(!cf() && !zf())"
  of "jl": "(sf() != of())"
  of "jge": "(sf() == of())"
  of "jle": "(zf() || sf() != of())"
  of "jg": "(!zf() && sf() == of())"
  of "js": "sf()"
  of "jns": "!sf()"
  of "jp": "pf()"
  of "jnp": "!pf()"
  of "jo": "of()"
  of "jno": "!of()"
  else: ""

proc target(i: Insn): uint32 = imm(i.ops[0])

const setters = ["cmp", "test", "add", "sub", "and", "or", "xor", "neg", "sahf", "adc", "sbb", "shl", "shr", "sar"]

proc reads(i: Insn): bool =
  (i.m.startsWith("j") and i.m != "jmp") or i.m.startsWith("set") or i.m in ["adc", "sbb"]

## whether the flags i sets are read before others replace them; a call
## ends them (Delphi never reads them across one), a block's end keeps them
proc live(fn: Fn, k: int, labels: HashSet[uint32]): bool =
  for n in k + 1 ..< fn.insns.len:
    let i = fn.insns[n]
    if i.va in labels or reads(i): return true
    if i.m in setters or i.m == "call": return false
    if i.m == "jmp" or i.m == "ret": return true
  true

proc firstAt(fn: Fn, va: uint32): Insn =
  for i in fn.insns:
    if i.va == va: return i

## a cmp or test then the one jump that reads it
proc fused(fn: Fn, k: int, labels: HashSet[uint32]): bool =
  if k + 1 >= fn.insns.len: return false
  let i = fn.insns[k]
  let j = fn.insns[k + 1]
  if i.m notin ["cmp", "test"] or not j.m.startsWith("j") or j.m == "jmp" or j.va in labels: return false
  if k + 2 < fn.insns.len and reads(fn.insns[k + 2]): return false
  not reads(firstAt(fn, target(j)))

proc compare(i, j: Insn): string =
  let bits = bitsOf(i)
  let a = read(i, i.ops[0])
  let b = read(i, i.ops[1])
  if i.m == "test":
    let t = &"(({a} & {b}){mask(bits)})"
    return case j.m
      of "je", "jz": &"{t} == 0"
      of "jne", "jnz": &"{t} != 0"
      of "js": &"({t} >> {bits - 1}) != 0"
      of "jns": &"({t} >> {bits - 1}) == 0"
      else: (i.fail("test then " & j.m); "")
  let s = &"int{bits}_t"
  let us = &"uint{bits}_t"
  let sa = &"({s})({us})({a})"
  let sb = &"({s})({us})({b})"
  let ua = &"({us})({a})"
  let ub = &"({us})({b})"
  case j.m
  of "je", "jz": &"{ua} == {ub}"
  of "jne", "jnz": &"{ua} != {ub}"
  of "jb", "jc": &"{ua} < {ub}"
  of "jae", "jnc": &"{ua} >= {ub}"
  of "jbe": &"{ua} <= {ub}"
  of "ja": &"{ua} > {ub}"
  of "jl": &"{sa} < {sb}"
  of "jge": &"{sa} >= {sb}"
  of "jle": &"{sa} <= {sb}"
  of "jg": &"{sa} > {sb}"
  else: (i.fail("cmp then " & j.m); "")

## where a jump goes: within the chunk a goto, else the chunk returns it
var
  chunkOf: Table[uint32, int]
  current: int

proc goto(t: uint32): string =
  if chunkOf.getOrDefault(t, -1) == current: &"goto L{t:x};" else: &"return {u(t)};"

proc arith(i: Insn, op, kind: string, carryIn = "", keep = true): seq[string] =
  let bits = bitsOf(i)
  result.add &"uint32_t a = {read(i, i.ops[0])}, b = {read(i, i.ops[1])}{carryIn}, r = (a {op} b){mask(bits)};"
  if kind != "": result.add &"set_flags({kind}, a, b, r, {bits});"
  if keep: result.add write(i, i.ops[0], "r")

proc lift(fn: Fn, i: Insn, next: uint32, labels: HashSet[uint32]): seq[string] =
  let o = i.ops
  case i.m
  of "nop", "fwait", "wait": discard
  of "mov", "movzx": result.add write(i, o[0], read(i, o[1]))
  of "movsx":
    let s = if sizeOf(o[1]) == 8: "int8_t" else: "int16_t"
    result.add write(i, o[0], &"(uint32_t)(int32_t)({s})({read(i, o[1])})")
  of "lea": result.add write(i, o[0], address(i, o[1]))
  of "push": result.add &"push32({read(i, o[0])});"
  of "pop": result.add write(i, o[0], "pop32()")
  of "xchg":
    result.add &"uint32_t t = {read(i, o[0])};"
    result.add write(i, o[0], read(i, o[1]))
    result.add write(i, o[1], "t")
  of "xor", "and", "or":
    let bits = bitsOf(i)
    if i.m == "xor" and o[0] == o[1]:
      result.add write(i, o[0], "0u")
      result.add &"set_flags(FLAG_LOGIC, 0, 0, 0, {bits});"
    else:
      let c = case i.m
        of "xor": "^"
        of "and": "&"
        else: "|"
      result.add arith(i, c, "")
      result.add &"set_flags(FLAG_LOGIC, 0, 0, r, {bits});"
  of "test":
    let bits = bitsOf(i)
    result.add &"set_flags(FLAG_LOGIC, 0, 0, ({read(i, o[0])} & {read(i, o[1])}){mask(bits)}, {bits});"
  of "add": result.add arith(i, "+", "FLAG_ADD")
  of "sub": result.add arith(i, "-", "FLAG_SUB")
  of "cmp": result.add arith(i, "-", "FLAG_SUB", keep = false)
  of "adc": result.add arith(i, "+", "FLAG_ADD", " + cf()")
  of "sbb": result.add arith(i, "-", "FLAG_SUB", " + cf()")
  of "inc", "dec":
    let bits = bitsOf(i)
    let up = i.m == "inc"
    result.add &"uint32_t a = {read(i, o[0])}, r = (a {(if up: \"+\" else: \"-\")} 1){mask(bits)};"
    result.add &"set_flags({(if up: \"FLAG_INC\" else: \"FLAG_DEC\")}, a, 1, r, {bits});"
    result.add write(i, o[0], "r")
  of "neg":
    let bits = bitsOf(i)
    result.add &"uint32_t a = {read(i, o[0])}, r = (0u - a){mask(bits)};"
    result.add &"set_flags(FLAG_SUB, 0, a, r, {bits});"
    result.add write(i, o[0], "r")
  of "not": result.add write(i, o[0], &"~{read(i, o[0])}")
  of "shl", "shr", "sar":
    let bits = bitsOf(i)
    let n = if o.len > 1: &"({read(i, o[1])} & 31)" else: "1"
    let v = read(i, o[0])
    let e = case i.m
      of "shl": &"{v} << {n}"
      of "shr": &"{v} >> {n}"
      else: &"(uint32_t)((int{bits}_t)(uint{bits}_t)({v}) >> {n})"
    result.add &"uint32_t r = ({e}){mask(bits)};"
    result.add &"set_flags(FLAG_LOGIC, 0, 0, r, {bits});"
    result.add write(i, o[0], "r")
  of "imul":
    if o.len == 3:
      result.add write(i, o[0], &"(uint32_t)((int32_t){read(i, o[1])} * (int32_t){read(i, o[2])})")
    elif o.len == 2:
      result.add write(i, o[0], &"(uint32_t)((int32_t){read(i, o[0])} * (int32_t){read(i, o[1])})")
    else:
      result.add &"int64_t p = (int64_t)(int32_t)eax * (int32_t){read(i, o[0])};"
      result.add "eax = (uint32_t)p;"
      result.add "edx = (uint32_t)((uint64_t)p >> 32);"
  of "mul":
    result.add &"uint64_t p = (uint64_t)eax * {read(i, o[0])};"
    result.add "eax = (uint32_t)p;"
    result.add "edx = (uint32_t)(p >> 32);"
  of "cdq": result.add "edx = (eax & 0x80000000u) ? 0xffffffffu : 0;"
  of "cwde": result.add "eax = (uint32_t)(int32_t)(int16_t)eax;"
  of "idiv":
    result.add &"int64_t d = (int32_t){read(i, o[0])}, n = (int64_t)((uint64_t)edx << 32 | eax);"
    result.add "eax = (uint32_t)(int32_t)(n / d);"
    result.add "edx = (uint32_t)(int32_t)(n % d);"
  of "div":
    result.add &"uint64_t d = {read(i, o[0])}, n = (uint64_t)edx << 32 | eax;"
    result.add "eax = (uint32_t)(n / d);"
    result.add "edx = (uint32_t)(n % d);"
  of "sahf": result.add "sahf();"
  of "fnstsw": result.add write(i, o[0], "fnstsw()")
  of "fld": result.add &"fpush({fval(i, o[0])});"
  of "fild": result.add &"fpush({fint(i, o[0])});"
  of "fld1": result.add "fpush(1.0);"
  of "fldz": result.add "fpush(0.0);"
  of "fchs": result.add "ST(0) = -ST(0);"
  of "fabs": result.add "ST(0) = fabs(ST(0));"
  of "fxch":
    let other = fval(i, (if o.len > 0: o[^1] else: "st(1)"))
    result.add &"double t = ST(0); ST(0) = {other}; {other} = t;"
  of "fst", "fstp":
    if o[0].startsWith("st"):
      result.add &"{fval(i, o[0])} = ST(0);"
    else:
      let a = address(i, o[0])
      case sizeOf(o[0])
      of 32: result.add &"wr32({a}, bits32(ST(0)));"
      of 64: result.add &"wr64({a}, bits64(ST(0)));"
      else: i.fail("fst size")
    if i.m == "fstp": result.add "fpop();"
  of "fistp", "fist":
    let a = address(i, o[0])
    case sizeOf(o[0])
    of 16: result.add &"wr16({a}, (uint32_t)x86_round(ST(0)));"
    of 32: result.add &"wr32({a}, (uint32_t)x86_round(ST(0)));"
    of 64: result.add &"wr64({a}, (uint64_t)x86_round(ST(0)));"
    else: i.fail("fistp size")
    if i.m == "fistp": result.add "fpop();"
  of "fadd", "fsub", "fsubr", "fmul", "fdiv", "fdivr", "faddp", "fsubp", "fsubrp", "fmulp", "fdivp", "fdivrp":
    let base = i.m.strip(leading = false, chars = {'p'})
    let opc = case base
      of "fadd": "+"
      of "fsub", "fsubr": "-"
      of "fmul": "*"
      else: "/"
    var dst, src: string
    if o.len == 2:
      dst = fval(i, o[0])
      src = fval(i, o[1])
    elif o.len == 1:
      dst = "ST(0)"
      src = fval(i, o[0])
    else:
      dst = "ST(1)"
      src = "ST(0)"
    result.add (if base.endsWith("r"): &"{dst} = {src} {opc} {dst};" else: &"{dst} = {dst} {opc} {src};")
    if i.m.endsWith("p"): result.add "fpop();"
  of "fcom", "fcomp":
    result.add &"fcom(ST(0), {(if o.len > 0: fval(i, o[0]) else: \"ST(1)\")});"
    if i.m == "fcomp": result.add "fpop();"
  of "fcompp":
    result.add "fcom(ST(0), ST(1));"
    result.add "fpop();"
    result.add "fpop();"
  of "jmp":
    let t = target(i)
    if t in labels: result.add goto(t)
    elif t in byEntry: result.add &"{byEntry[t]}(); return 0;"
    else: result.add &"x86_fail(\"{fn.name} leaves\", {u(t)});"
  of "call":
    let t = target(i)
    if t in byEntry:
      result.add &"push32({u(next)});"
      result.add &"{byEntry[t]}();"
    else:
      result.add &"x86_rtl({u(t)});"
  of "ret":
    let n = if o.len > 0: imm(o[0]) else: 0'u32
    let finals = fn.insns.filterIt(it.m == "push" and it.ops[0].isImm and imm(it.ops[0]) in labels)
    if finals.len == 0: result.add "pop32();"
    else:
      result.add "uint32_t back = pop32();"
      for p in finals: result.add &"if (back == {u(imm(p.ops[0]))}) {goto(imm(p.ops[0]))}"
    result.add &"esp += {u(n)};"
    result.add "return 0;"
  else:
    let c = cond(i.m)
    if c == "": i.fail("not understood")
    if i.m.startsWith("set"): result.add write(i, o[0], &"{c} ? 1u : 0u")
    else: result.add &"if ({c}) {goto(target(i))}"

proc emit(fn: Fn): string =
  var labels = initHashSet[uint32]()
  labels.incl fn.entry
  let at = fn.insns.mapIt(it.va).toHashSet
  for k, i in fn.insns:
    if i.m.startsWith("j") or (i.m == "push" and i.ops[0].isImm and imm(i.ops[0]) in at):
      let t = if i.m.startsWith("j"): target(i) else: imm(i.ops[0])
      if t in at: labels.incl t
    if (i.m == "jmp" or i.m == "ret") and k + 1 < fn.insns.len:
      labels.incl fn.insns[k + 1].va
  chunkOf.clear
  var starts: seq[uint32]
  var count = 0
  for i in fn.insns:
    if i.va in labels:
      if count mod chunk == 0: starts.add i.va
      inc count
    chunkOf[i.va] = starts.len - 1
  var o: seq[string]
  var k = 0
  for c in 0 ..< starts.len:
    current = c
    o.add &"static uint32_t {fn.name}_{c}(uint32_t pc)"
    o.add "{"
    o.add "    switch (pc) {"
    for i in fn.insns:
      if i.va in labels and chunkOf[i.va] == c: o.add &"    case {u(i.va)}: goto L{i.va:x};"
    o.add "    default: return pc;"
    o.add "    }"
    var skip = false
    while k < fn.insns.len and chunkOf[fn.insns[k].va] == c:
      let i = fn.insns[k]
      let next = if k + 1 < fn.insns.len: fn.insns[k + 1].va else: 0'u32
      if skip:
        skip = false
        inc k
        continue
      if i.va in labels: o.add &"L{i.va:x}:"
      var lines: seq[string]
      var text = i.text
      var last = k
      if fused(fn, k, labels):
        let j = fn.insns[k + 1]
        text &= " / " & j.text
        lines = @[&"if ({compare(i, j)}) {goto(target(j))}"]
        skip = true
        last = k + 1
      else:
        lines = lift(fn, i, next, labels)
        if i.m in setters and not live(fn, k, labels):
          lines = lines.filterIt(not it.startsWith("set_flags"))
      o.add &"    /* {i.va:x}: {text.replace(\"*/\", \"* /\")} */"
      if lines.anyIt(it.startsWith("uint32_t ") or it.startsWith("int64_t ") or
          it.startsWith("uint64_t ") or it.startsWith("double ")):
        o.add "    { " & lines.join(" ") & " }"
      else:
        for l in lines: o.add "    " & l
      let ends = i.m == "jmp" or i.m == "ret"
      if not ends and last + 1 < fn.insns.len and chunkOf[fn.insns[last + 1].va] != c:
        o.add &"    return {u(fn.insns[last + 1].va)};"
      elif not ends and last + 1 == fn.insns.len:
        o.add &"    x86_fail(\"{fn.name} runs past its end\", {u(i.va)});"
      inc k
    o.add "}"
    o.add ""
  o.add &"void {fn.name}(void)"
  o.add "{"
  o.add &"    uint32_t pc = {u(fn.entry)};"
  o.add "    while (pc)"
  var cases: seq[string]
  for c, s in starts:
    let e = if c + 1 < starts.len: starts[c + 1] else: 0xffff_ffff'u32
    cases.add &"pc >= {u(s)} && pc < {u(e)} ? {fn.name}_{c}(pc)"
  o.add "        pc = " & cases.join("\n            : ") & "\n            : (x86_fail(\"" & fn.name & " has no code\", pc), 0);"
  o.add "}"
  o.join("\n")

proc main() =
  let args = commandLineParams()
  if args.len < 3: quit "usage: lift <module> <dir> <dump.s>..."
  let module = args[0]
  for p in args[2 .. ^1].sorted: parse(p)
  var c = @[&"/* generated by scripts/lift.nim from the dumps; do not edit */",
    "", "#include <math.h>", "", "#include \"chavo8/x86.h\"", ""]
  for r in regs32: c.add &"#define {r} x86.r[X86_{r.toUpperAscii}]"
  c.add ""
  for fn in fns: c.add &"void {fn.name}(void);"
  c.add ""
  c.add "static const char *literal(uint32_t at, uint32_t *len)"
  c.add "{"
  c.add "    switch (at) {"
  for a in toSeq(strings.keys).sorted:
    c.add &"    case {u(a)}: *len = {strings[a].len}; return {cstr(strings[a])};"
  c.add "    default: x86_fail(\"no string\", at);"
  c.add "    }"
  c.add "}"
  c.add ""
  c.add &"void {module}_lifted(void)"
  c.add "{"
  c.add "    x86_literal = literal;"
  c.add "}"
  c.add ""
  for fn in fns: c.add emit(fn) & "\n"
  var n = @[&"## generated by scripts/lift.nim: the functions of {module}.c", ""]
  n.add &"proc {module}_lifted*() {{.importc, cdecl.}}"
  for fn in fns: n.add &"proc {fn.name}*() {{.importc, cdecl.}}"
  for (path, text) in [(args[1] / module & ".c", c.join("\n") & "\n"), (args[1] / module & ".nim", n.join("\n") & "\n")]:
    if not fileExists(path) or readFile(path) != text: writeFile(path, text)

main()
