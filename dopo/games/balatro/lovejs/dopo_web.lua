-- dopo_web: what LÖVE games take for granted on the desktop and the
-- browser does differently, smoothed over for love.js.

-- the page sizes the game, and a browser only goes fullscreen from a click:
-- a game asking for fullscreen at start (as Balatro does) gets a window the
-- size of the page instead of an error
local function windowed(flags)
  if type(flags) ~= 'table' then return flags end
  local copy = {}
  for k, v in pairs(flags) do copy[k] = v end
  copy.fullscreen = false
  return copy
end

for _, name in ipairs({'setMode', 'updateMode'}) do
  local original = love.window[name]
  love.window[name] = function(width, height, flags)
    return original(width, height, windowed(flags))
  end
end

function love.window.setFullscreen()
  return false
end

-- WebGL 1 makes no mipmaps of textures whose sides are not powers of two,
-- which most game atlases are: images come without them
local newImage = love.graphics.newImage
function love.graphics.newImage(source, settings)
  if type(settings) == 'table' and settings.mipmaps then
    local copy = {}
    for k, v in pairs(settings) do copy[k] = v end
    copy.mipmaps = false
    settings = copy
  end
  return newImage(source, settings)
end
