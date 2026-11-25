## dump: the game's x86 code, function by function, for lift.nim to turn into
## C at build time, so building needs no Chaves.exe.
##
## usage: dump <Chaves.exe> <assembly.tar.xz> <first>-<end>[=name] ...
##
## Every function whose entry lies in a range given (hex addresses), called
## from anywhere in the exe, is disassembled by objdump following its jumps,
## as Delphi leaves strings and constants between functions; a name given
## to a range's first address names that function. <name>.s, in the archive
## (GNU tar and xz, owners, dates and order fixed and xz on one thread, so the
## same code packs into the same bytes), gets:
##   ; entry <hex>
##   ; exit <hex>                          where the code leaves the range given,
##                                        a fragment of a function (a screen of
##                                        DXTimer1Timer) ending there
##   <hex>: <instruction>                 (Intel syntax, as objdump writes it)
##   .data <hex> <hex bytes>              memory the code reads at a fixed
##                                        address outside the BSS: constants,
##                                        the pointers to the units' variables
##   .string <hex> <hex bytes>            a string literal its code points to

import std/[algorithm, os, osproc, sequtils, sets, strformat, strutils, tables, tempfiles]

type
  Section = object
    va, vsize, size, raw: uint32
  Exe = object
    data: string
    base: uint32
    sections: seq[Section]

proc u16(s: string, o: int): uint32 = uint32(s[o].uint8) or uint32(s[o + 1].uint8) shl 8
proc u32(s: string, o: int): uint32 = u16(s, o) or u16(s, o + 2) shl 16

proc load(path: string): Exe =
  result.data = readFile(path)
  let nt = int(u32(result.data, 0x3c))
  let opt = nt + 24
  result.base = u32(result.data, opt + 28)
  let count = int(u16(result.data, nt + 6))
  let first = opt + int(u16(result.data, nt + 20))
  for i in 0 ..< count:
    let s = first + 40 * i
    result.sections.add Section(va: result.base + u32(result.data, s + 12),
      vsize: u32(result.data, s + 8), size: u32(result.data, s + 16), raw: u32(result.data, s + 20))

proc bytesAt(e: Exe, va: uint32, n: int): string =
  for s in e.sections:
    if va >= s.va and uint64(va) + uint64(n) <= uint64(s.va) + uint64(s.size):
      return e.data[int(s.raw + va - s.va) ..< int(s.raw + va - s.va) + n]
  ""

proc code(e: Exe): Section = e.sections[0]
proc bss(e: Exe, va: uint32): bool =
  for s in e.sections:
    if s.size == 0 and va >= s.va and va < s.va + max(s.vsize, 0x4000'u32): return true

type Insn = object
  va: uint32
  size: int
  text: string

## objdump from va on; what starts before stop, decoded past it so the last
## instruction is whole
proc disasm(exe, va: string, stop: uint32): seq[Insn] =
  let start = parseHexInt(va).uint32
  let output = execProcess("objdump", args = ["-d", "-M", "intel", "--insn-width=16",
    &"--start-address=0x{start:x}", &"--stop-address=0x{stop + 16:x}", exe], options = {poUsePath})
  for line in output.splitLines:
    let parts = line.strip.split("\t")
    if parts.len == 3 and parts[0].endsWith(":") and parts[0][0 ..< ^1].allIt(it in HexDigits):
      let va = parseHexInt(parts[0][0 ..< ^1]).uint32
      if va < stop:
        result.add Insn(va: va, size: parts[1].splitWhitespace.len, text: parts[2].splitWhitespace.join(" "))

proc target(text: string): int64 =
  let p = text.splitWhitespace
  if p.len == 2 and p[1].startsWith("0x") and p[1][2 .. ^1].allIt(it in HexDigits): parseHexInt(p[1])
  else: -1

proc isJump(m: string): bool = m.startsWith("j") or m == "call" or m == "loop" or m.startsWith("loop")

proc main() =
  let args = commandLineParams()
  if args.len < 3:
    quit "usage: dump <Chaves.exe> <assembly.tar.xz> <first>-<end>[=name] ..."
  let path = args[0]
  let exe = load(path)
  let archive = absolutePath(args[1])
  let dir = createTempDir("chavo8", "dump")
  var ranges: seq[(uint32, uint32)]
  var names: Table[uint32, string]
  for a in args[2 .. ^1]:
    let named = a.split("=", 1)
    let r = named[0].split("-")
    let first = parseHexInt(r[0]).uint32
    ranges.add (first, parseHexInt(r[1]).uint32)
    if named.len == 2: names[first] = named[1]
  proc inside(va: uint32): bool = ranges.anyIt(va >= it[0] and va < it[1])

  # every call into the ranges, from the whole code section
  let c = exe.code
  var entries = initOrderedSet[uint32]()
  for r in ranges: entries.incl r[0]
  for i in disasm(path, &"{c.va:x}", c.va + c.size):
    if i.text.startsWith("call "):
      let t = target(i.text)
      if t >= 0 and inside(uint32(t)): entries.incl uint32(t)

  var done = initHashSet[uint32]()
  var queue = toSeq(entries)
  while queue.len > 0:
    let entry = queue.pop
    if entry in done: continue
    done.incl entry
    var known: Table[uint32, Insn]
    var reach = @[entry]
    var seen = initHashSet[uint32]()
    var pushed = initHashSet[uint32]()
    var exits = initOrderedSet[uint32]()
    while reach.len > 0 or pushed.len > 0:
      if reach.len == 0:
        for p in pushed:
          if p notin seen: reach.add p
        pushed.clear
        continue
      let va = reach.pop
      if va in seen: continue
      if va notin known:
        for i in disasm(path, &"{va:x}", va + 0x200): known[i.va] = i
        if va notin known: quit &"dump: nothing decodes at {va:x} (from {entry:x})"
      seen.incl va
      let i = known[va]
      let m = i.text.splitWhitespace[0]
      let next = va + uint32(i.size)
      if next notin known:
        for j in disasm(path, &"{next:x}", next + 0x200): known[j.va] = j
      let t = target(i.text)
      if m == "call":
        if t >= 0 and inside(uint32(t)) and uint32(t) notin done: queue.add uint32(t)
        if inside(next): reach.add next else: exits.incl next
      elif m == "jmp":
        if t < 0: quit &"dump: indirect jump at {va:x}"
        if inside(uint32(t)): reach.add uint32(t) else: exits.incl uint32(t)
      elif m == "ret":
        discard
      elif isJump(m):
        if inside(uint32(t)): reach.add uint32(t) else: exits.incl uint32(t)
        if inside(next): reach.add next else: exits.incl next
      else:
        # push of a label: Delphi's finally returns there with ret
        if m == "push" and t >= int64(entry) and t < int64(entry) + 0x20000 and
            u32(exe.bytesAt(uint32(t) - 8, 4) & "\0\0\0\0", 0) != 0xFFFF_FFFF'u32:
          pushed.incl uint32(t)
        if inside(next): reach.add next else: exits.incl next
    var insns = toSeq(seen).sorted.mapIt(known[it])
    var lines = @[&"; entry {entry:x}"]
    for x in exits: lines.add &"; exit {x:x}"
    var datas = initOrderedSet[uint32]()
    var strs = initOrderedSet[uint32]()
    for i in insns:
      lines.add &"{i.va:x}: {i.text}"
      for tok in i.text.multiReplace(("[", " "), ("]", " "), (",", " "), ("+", " "), ("-", " ")).splitWhitespace:
        let fixed = tok.startsWith("ds:")
        let t = tok.replace("ds:", "")
        if t.startsWith("0x") and t.len >= 8:
          let a = parseHexInt(t).uint32
          if a < exe.base or bss(exe, a): continue
          if exe.bytesAt(a - 8, 8).len == 8 and u32(exe.bytesAt(a - 8, 4), 0) == 0xFFFF_FFFF'u32:
            strs.incl a
          elif ("PTR" in i.text or fixed) and exe.bytesAt(a, 10).len == 10:
            datas.incl a
    for a in datas:
      lines.add &".data {a:x} " & exe.bytesAt(a, 10).toHex.toLowerAscii
    for a in strs:
      let n = int(u32(exe.bytesAt(a - 4, 4), 0))
      lines.add &".string {a:x} " & exe.bytesAt(a, n).toHex.toLowerAscii
    let name = names.getOrDefault(entry, &"f{entry:x}")
    writeFile(dir / name & ".s", lines.join("\n") & "\n")
    echo &"{name}: {insns.len} instructions"

  var files: seq[string]
  for f in walkFiles(dir / "*.s"): files.add f.extractFilename
  let z = startProcess("tar", dir, @["--sort=name", "--mtime=@0", "--owner=0", "--group=0", "--numeric-owner",
    "--mode=a=r,u+w", "--format=gnu", "--use-compress-program=xz -9e -T1", "-cf", archive] & files.sorted,
    options = {poUsePath, poParentStreams})
  let code = z.waitForExit
  z.close
  removeDir(dir)
  if code != 0: quit "dump: tar failed"

main()
