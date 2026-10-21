-- bit: LuaJIT's bit library in plain Lua 5.1, for love.js, which has no
-- LuaJIT. Numbers are 32-bit signed integers as LuaJIT's bit gives them, so
-- what Balatro hashes with it (its seeds) comes out the same.

local bit = {}

local MOD = 4294967296 -- 2^32

-- x as the signed 32-bit integer LuaJIT works on
local function tobit(x)
  x = math.floor(x + 0.5) % MOD
  if x >= 2147483648 then x = x - MOD end
  return x
end

local function unsigned(x)
  return tobit(x) % MOD
end

-- a bitwise operation of two numbers, bit by bit
local function combine(a, b, op)
  a, b = unsigned(a), unsigned(b)
  local result, place = 0, 1
  for _ = 1, 32 do
    local x, y = a % 2, b % 2
    if op(x, y) then result = result + place end
    a, b = (a - x) / 2, (b - y) / 2
    place = place * 2
  end
  return tobit(result)
end

local function fold(op)
  return function(x, ...)
    local result = tobit(x)
    for i = 1, select('#', ...) do
      result = combine(result, select(i, ...), op)
    end
    return result
  end
end

bit.tobit = tobit
bit.band = fold(function(x, y) return x == 1 and y == 1 end)
bit.bor = fold(function(x, y) return x == 1 or y == 1 end)
bit.bxor = fold(function(x, y) return x ~= y end)

function bit.bnot(x)
  return tobit(MOD - 1 - unsigned(x))
end

function bit.lshift(x, n)
  return tobit(unsigned(x) * 2 ^ (n % 32))
end

function bit.rshift(x, n)
  return tobit(math.floor(unsigned(x) / 2 ^ (n % 32)))
end

function bit.arshift(x, n)
  return tobit(math.floor(tobit(x) / 2 ^ (n % 32)))
end

function bit.rol(x, n)
  n = n % 32
  return bit.bor(bit.lshift(x, n), bit.rshift(x, 32 - n))
end

function bit.ror(x, n)
  n = n % 32
  return bit.bor(bit.rshift(x, n), bit.lshift(x, 32 - n))
end

function bit.bswap(x)
  x = unsigned(x)
  local b1, b2, b3, b4 = x % 256, math.floor(x / 256) % 256, math.floor(x / 65536) % 256, math.floor(x / 16777216)
  return tobit(((b1 * 256 + b2) * 256 + b3) * 256 + b4)
end

function bit.tohex(x, n)
  n = n or 8
  local upper = n < 0
  n = math.abs(n)
  local hex = string.format('%08x', unsigned(x)):sub(9 - n)
  return upper and hex:upper() or hex
end

return bit
