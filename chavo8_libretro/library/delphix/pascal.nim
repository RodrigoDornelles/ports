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

type Small* = Byte | Word | ShortInt | SmallInt

proc `:=`*[T: Small, V: SomeInteger | Small](dst: var T, v: V) =
  let n = int64(v)
  when T is Byte: dst = Byte(uint8(n and 0xFF))
  elif T is Word: dst = Word(uint16(n and 0xFFFF))
  elif T is ShortInt: dst = ShortInt(cast[int8](uint8(n and 0xFF)))
  else: dst = SmallInt(cast[int16](uint16(n and 0xFFFF)))

proc `:=`*[V: SomeInteger | Small](dst: var Integer, v: V) =
  dst = cast[int32](uint32(int64(v) and 0xFFFF_FFFF))

proc `:=`*[V: SomeInteger | Small](dst: var Real, v: V) = dst = Real(int64(v))
proc `:=`*(dst: var Real, v: Real) = dst = v

proc `$`*(x: Byte | Word | ShortInt | SmallInt): string = $Integer(x)

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
  doAssert Cos(0.0) == 1.0 and Cos(1e19) == 0.0
