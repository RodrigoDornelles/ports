-- Render scale
--
-- Draws the game at 2/3 of the window size and stretches it to fill, cutting
-- the fill rate. DOPO_CANV_SCALE sets the scale.

--! @patch game.lua 2956
        if not G.AA_CANVAS then love.graphics.scale(1/G.CANV_SCALE) end
--! @endpatch

--! @patch globals.lua 1
DOPO_CANV_SCALE = tonumber(os.getenv('DOPO_CANV_SCALE') or '') or 0.6667
--! @endpatch

--! @patch main.lua 368-368
	G.CANV_SCALE = DOPO_CANV_SCALE
--! @endpatch
