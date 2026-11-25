## Delphi 7 semantics for the faithful ports. Byte, Word, ShortInt and
## SmallInt widen to Integer in expressions as Pascal does, so `b1 + b2`
## does not wrap at 255; `:=` stores back truncating, as Delphi does with
## range checks off. Compile with overflow and range checks off, and the C
## with -fwrapv, so Integer arithmetic wraps too.
##
## Random, Round and Trunc give Delphi's exact results: its LCG on
## RandSeed, and round half to even (Nim's round goes away from zero).

type
  Integer* = int32
  LongInt* = int32
  Cardinal* = uint32
  LongWord* = uint32
  Int64* = int64
  Real* = float64
  Double* = float64
  Single* = float32
  Extended* = float64
  Byte* = distinct uint8
  Word* = distinct uint16
  ShortInt* = distinct int8
  SmallInt* = distinct int16

converter toInteger*(x: Byte): Integer = Integer(uint8(x))
converter toInteger*(x: Word): Integer = Integer(uint16(x))
converter toInteger*(x: ShortInt): Integer = Integer(int8(x))
converter toInteger*(x: SmallInt): Integer = Integer(int16(x))

## an Int64 (Round's result) or a Nim int (a for loop's) given where an
## Integer goes keeps its low 32 bits, as Delphi passes it unchecked
converter toInteger*(x: Int64): Integer = cast[int32](uint32(x and 0xFFFF_FFFF))
converter toInteger*(x: int): Integer = cast[int32](uint32(int64(x) and 0xFFFF_FFFF))

type Small* = Byte | Word | ShortInt | SmallInt

proc `:=`*[T: Small, V: SomeInteger | Small](dst: var T, v: V) =
  let n = int64(v)
  when T is Byte: dst = Byte(uint8(n and 0xFF))
  elif T is Word: dst = Word(uint16(n and 0xFFFF))
  elif T is ShortInt: dst = ShortInt(cast[int8](uint8(n and 0xFF)))
  else: dst = SmallInt(cast[int16](uint16(n and 0xFFFF)))

proc `:=`*[V: SomeInteger | Small](dst: var Integer, v: V) =
  dst = cast[int32](uint32(int64(v) and 0xFFFF_FFFF))

proc `:=`*[V: SomeInteger | Small](dst: var Cardinal, v: V) =
  dst = uint32(int64(v) and 0xFFFF_FFFF)

proc `:=`*[V: SomeInteger | Small](dst: var Real, v: V) = dst = Real(int64(v))
proc `:=`*(dst: var Real, v: Real) = dst = v

proc `$`*(x: Byte | Word | ShortInt | SmallInt): string = $Integer(x)

## / is always a real division in Pascal
proc `/`*[A, B: SomeInteger | Small](a: A, b: B): Real = Real(int64(a)) / Real(int64(b))
proc `/`*[A: SomeInteger | Small](a: A, b: Real): Real = Real(int64(a)) / b
proc `/`*[B: SomeInteger | Small](a: Real, b: B): Real = a / Real(int64(b))

var RandSeed*: LongInt

proc nextSeed() {.inline.} =
  RandSeed = cast[int32](cast[uint32](RandSeed) * 0x08088405'u32 + 1)

proc Random*(range: Integer): Integer =
  nextSeed()
  Integer((uint64(cast[uint32](RandSeed)) * uint64(cast[uint32](range))) shr 32)

proc Random*(): Extended =
  nextSeed()
  float64(cast[uint32](RandSeed)) / 4294967296.0

{.passl: "-lm".}
proc nearbyint(x: cdouble): cdouble {.importc, header: "<math.h>".}
proc trunc(x: cdouble): cdouble {.importc, header: "<math.h>".}
proc cos(x: cdouble): cdouble {.importc, header: "<math.h>".}

proc Round*(x: Extended): Int64 = Int64(nearbyint(x))
proc Trunc*(x: Extended): Int64 = Int64(x)
proc Int*(x: Extended): Extended = trunc(x)

## fcos leaves an operand past 2^63 alone and flags it, which Delphi's
## Cos turns into 0; within range libm may differ from the x87 in the last
## bit before the result is stored as a Double
proc Cos*(x: Extended): Extended =
  if abs(x) >= 9223372036854775808.0: 0.0 else: cos(x)

type EConvertError* = object of CatchableError

proc strip_blanks(s: string): string =
  var a = 0
  var b = s.len
  while a < b and s[a] == ' ': inc a
  while b > a and s[b - 1] == ' ': dec b
  s[a ..< b]

## Val as StrToInt reads: blanks before, a sign, decimal or hex ($ or 0x),
## nothing after; out of Integer's range is an error too
proc StrToInt*(s: string): Integer =
  var i = 0
  while i < s.len and s[i] == ' ': inc i
  var neg = false
  if i < s.len and s[i] in {'+', '-'}:
    neg = s[i] == '-'
    inc i
  var base = 10
  if i < s.len and s[i] == '$':
    base = 16
    inc i
  elif i + 1 < s.len and s[i] == '0' and s[i + 1] in {'x', 'X'}:
    base = 16
    i += 2
  if i >= s.len: raise newException(EConvertError, "'" & s & "' is not a valid integer value")
  var n: int64 = 0
  while i < s.len:
    let c = s[i]
    let d = case c
      of '0'..'9': ord(c) - ord('0')
      of 'a'..'f': ord(c) - ord('a') + 10
      of 'A'..'F': ord(c) - ord('A') + 10
      else: 99
    if d >= base: raise newException(EConvertError, "'" & s & "' is not a valid integer value")
    n = n * base + d
    if n > 0xFFFF_FFFF'i64: raise newException(EConvertError, "'" & s & "' is not a valid integer value")
    inc i
  if base == 10 and n > (if neg: 0x8000_0000'i64 else: 0x7FFF_FFFF'i64):
    raise newException(EConvertError, "'" & s & "' is not a valid integer value")
  cast[int32](uint32(if neg: -n and 0xFFFF_FFFF else: n))

proc strtod(s: cstring, e: ptr cstring): cdouble {.importc, header: "<stdlib.h>".}

## TextToFloat takes the decimal separator of Windows' locale, ',' on the
## Portuguese and Spanish ones the game was for; '.' is taken too
proc StrToFloat*(s: string): Extended =
  var t = s.strip_blanks()
  for c in t.mitems:
    if c == ',': c = '.'
  var e: cstring
  result = strtod(cstring(t), addr e)
  if t.len == 0 or e != cast[cstring](cast[uint](cstring(t)) + uint(t.len)):
    raise newException(EConvertError, "'" & s & "' is not a valid floating point value")

proc IntToStr*(n: Int64): string = $n
proc IntToStr*(n: Integer): string = $n

when isMainModule:
  RandSeed = 0
  doAssert Random(100) == 0
  doAssert Random(100) == 3
  doAssert RandSeed == 0x08088406
  var b: Byte
  b := 250
  inc b, 10
  doAssert b == 4
  doAssert Byte(200) + Byte(100) == 300
  var s: ShortInt
  s := 200
  doAssert s == -56
  var w: Word
  w := s
  doAssert w == 65480
  b := w
  doAssert b == 200
  var r: Real
  r := b
  doAssert r == 200.0
  var i: Integer = high(Integer)
  i := i + 1
  doAssert i == low(Integer)
  doAssert Round(2.5) == 2 and Round(3.5) == 4 and Round(-2.5) == -2 and Round(2.6) == 3
  doAssert Trunc(-2.7) == -2 and Trunc(2.7) == 2
  doAssert Int(-2.7) == -2.0 and Int(2.7) == 2.0
  doAssert SmallInt(7) / 2 == 3.5 and Byte(1) / 4 == 0.25
  doAssert Cos(0.0) == 1.0 and Cos(1e19) == 0.0
  doAssert StrToInt(" 42") == 42 and StrToInt("-7") == -7 and StrToInt("$1F") == 31 and StrToInt("0x10") == 16
  doAssert StrToInt("-2147483648") == low(Integer)
  for bad in ["", "4 ", "x", "2147483648", "1.5"]:
    doAssertRaises(EConvertError): discard StrToInt(bad)
  doAssert StrToFloat("0,9") == 0.9 and StrToFloat(" 1.5 ") == 1.5
  doAssertRaises(EConvertError): discard StrToFloat("0,9x")
