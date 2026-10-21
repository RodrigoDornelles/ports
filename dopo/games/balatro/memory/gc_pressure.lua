-- GC by memory pressure
--
-- Collects garbage once the resident memory of the process (Linux) passes 120
-- MB, which the Lua heap alone does not show. DOPO_GC_LIMIT_MB and
-- DOPO_GC_EVERY tune it.

--! @patch main.lua 126
-- DOPO_GC_PRESSURE
DOPO_GC_LIMIT_MB = tonumber(os.getenv('DOPO_GC_LIMIT_MB') or '') or 120
DOPO_GC_EVERY = tonumber(os.getenv('DOPO_GC_EVERY') or '') or 10
local dopo_gc_tick = 0
local dopo_gc_hits = 0
function dopo_gc_pressure()
    dopo_gc_tick = dopo_gc_tick + 1
    if dopo_gc_tick % DOPO_GC_EVERY ~= 0 then return end
    local f = io.open("/proc/self/statm", "r")
    if not f then return end
    local line = f:read("*l")
    f:close()
    if not line then return end
    local res = line:match("^%d+%s+(%d+)")
    if not res then return end
    if tonumber(res) * 4 / 1024 > DOPO_GC_LIMIT_MB then
        collectgarbage('collect')
        dopo_gc_hits = dopo_gc_hits + 1
        DOPO_GC_HITS = dopo_gc_hits
    end
end

--! @endpatch

--! @patch main.lua 127
	dopo_gc_pressure()
--! @endpatch
