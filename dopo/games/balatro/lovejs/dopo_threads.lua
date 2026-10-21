-- dopo_threads: love.thread for love.js, which runs no threads (a started
-- thread never runs there). Each thread becomes a coroutine of the main Lua
-- state, with globals of its own, and channels become queues in Lua that
-- copy tables as LÖVE's do. A thread runs as soon as it starts, again when
-- a channel gets a value while it waits, and once every frame.

local threads = {} -- the started threads still running, in start order
local current = nil -- the thread running now, nil on the main thread

-- a value as a channel hands it over: tables copied, so the sender can
-- change its table after the push
local function copy(value, seen)
  if type(value) ~= 'table' then return value end
  seen = seen or {}
  if seen[value] then return seen[value] end
  local result = {}
  seen[value] = result
  for k, v in pairs(value) do
    result[copy(k, seen)] = copy(v, seen)
  end
  return result
end

local function resume(thread)
  local previous = current
  current = thread
  local ok, err
  if thread.args then
    local args = thread.args
    thread.args = nil
    ok, err = coroutine.resume(thread.routine, unpack(args, 1, args.n))
  else
    ok, err = coroutine.resume(thread.routine)
  end
  current = previous
  if not ok then
    thread.error = tostring(err)
    print('thread error: ' .. thread.error)
    if love.threaderror then pcall(love.threaderror, thread, thread.error) end
  end
end

-- runs every other thread once, until each waits again or ends
local function step()
  for i = #threads, 1, -1 do
    local thread = threads[i]
    if thread ~= current and coroutine.status(thread.routine) == 'suspended' then
      resume(thread)
    end
    if coroutine.status(thread.routine) == 'dead' then
      table.remove(threads, i)
    end
  end
end

-- waits a little: a thread yields to the others, the main thread runs them
local function wait()
  if current then
    coroutine.yield()
  else
    step()
  end
end

local Channel = {}
Channel.__index = Channel

local function newChannel()
  return setmetatable({queue = {}, first = 1, last = 0, pushed = 0, read = 0}, Channel)
end

function Channel:push(value)
  self.last = self.last + 1
  self.queue[self.last] = copy(value)
  self.pushed = self.pushed + 1
  -- whoever waits for it runs now, as a real thread would
  if not current then step() end
  return self.pushed
end

function Channel:getCount()
  return self.last - self.first + 1
end

function Channel:pop()
  if self.first > self.last then return nil end
  local value = self.queue[self.first]
  self.queue[self.first] = nil
  self.first = self.first + 1
  self.read = self.read + 1
  return value
end

function Channel:peek()
  return self.queue[self.first]
end

function Channel:clear()
  self.queue, self.first, self.last = {}, 1, 0
end

function Channel:hasRead(id)
  return self.read >= id
end

-- a thread waits as long as it takes; the main thread, which would block
-- the page forever, gives up once no thread can answer
function Channel:demand(timeout)
  local start = love.timer.getTime()
  while self:getCount() == 0 do
    if timeout and love.timer.getTime() - start >= timeout then return nil end
    if not current and #threads == 0 then return nil end
    local before = self.pushed
    wait()
    if not current and not timeout and self.pushed == before then return nil end
  end
  return self:pop()
end

function Channel:supply(value, timeout)
  local id = self:push(value)
  local start = love.timer.getTime()
  while not self:hasRead(id) do
    if timeout and love.timer.getTime() - start >= timeout then return false end
    if not current and #threads == 0 then return false end
    wait()
  end
  return true
end

function Channel:performAtomic(fn, ...)
  return fn(self, ...)
end

local named = {}

function love.thread.getChannel(name)
  named[name] = named[name] or newChannel()
  return named[name]
end

function love.thread.newChannel()
  return newChannel()
end

local Thread = {}
Thread.__index = Thread

-- a file name, the code itself (LÖVE takes a string with a line break as
-- code) or FileData
function love.thread.newThread(source)
  local chunk, err
  if type(source) == 'string' then
    if source:find('\n') then
      chunk, err = loadstring(source, '=thread')
    else
      chunk, err = love.filesystem.load(source)
    end
  else
    chunk, err = loadstring(source:getString(), '=' .. source:getFilename())
  end
  if not chunk then error(err, 2) end
  return setmetatable({chunk = chunk}, Thread)
end

function Thread:start(...)
  if self.routine and coroutine.status(self.routine) ~= 'dead' then return end
  -- globals of its own, as a thread has a Lua state of its own
  setfenv(self.chunk, setmetatable({}, {__index = _G}))
  self.routine = coroutine.create(self.chunk)
  self.args = {n = select('#', ...), ...}
  self.error = nil
  table.insert(threads, self)
  resume(self)
end

function Thread:isRunning()
  return self.routine ~= nil and coroutine.status(self.routine) ~= 'dead'
end

function Thread:wait()
  while self:isRunning() and not current do step() end
end

function Thread:getError()
  return self.error
end

function Thread:release() end

-- every frame, and in place of sleeping inside a thread
local timer_step = love.timer.step
function love.timer.step(...)
  local dt = timer_step(...)
  if not current then step() end
  return dt
end

local timer_sleep = love.timer.sleep
function love.timer.sleep(seconds)
  if current then
    coroutine.yield()
  else
    return timer_sleep(seconds)
  end
end
