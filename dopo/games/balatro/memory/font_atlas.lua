-- Smaller font atlas
--
-- Rasterizes the fonts at half resolution and draws them twice as big, so the
-- glyph atlas takes a quarter of the memory. DOPO_FONT_DIV sets the divisor.

--! @patch game.lua 969-977
            {file = "resources/fonts/m6x11plus.ttf", render_scale = self.TILESIZE*10/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.83, TEXT_OFFSET = {x=10,y=-20}, FONTSCALE = 0.1*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/NotoSansSC-Bold.ttf", render_scale = self.TILESIZE*7/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.7, TEXT_OFFSET = {x=0,y=-35}, FONTSCALE = 0.12*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1.1},
            {file = "resources/fonts/NotoSansTC-Bold.ttf", render_scale = self.TILESIZE*7/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.7, TEXT_OFFSET = {x=0,y=-35}, FONTSCALE = 0.12*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1.1},
            {file = "resources/fonts/NotoSansKR-Bold.ttf", render_scale = self.TILESIZE*7/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.8, TEXT_OFFSET = {x=0,y=-20}, FONTSCALE = 0.12*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/NotoSansJP-Bold.ttf", render_scale = self.TILESIZE*7/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.8, TEXT_OFFSET = {x=0,y=-20}, FONTSCALE = 0.12*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/NotoSans-Bold.ttf", render_scale = self.TILESIZE*7/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.65, TEXT_OFFSET = {x=0,y=-40}, FONTSCALE = 0.12*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/m6x11plus.ttf", render_scale = self.TILESIZE*10/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.9, TEXT_OFFSET = {x=10,y=15}, FONTSCALE = 0.1*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/GoNotoCurrent-Bold.ttf", render_scale = self.TILESIZE*10/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.8, TEXT_OFFSET = {x=10,y=-20}, FONTSCALE = 0.1*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
            {file = "resources/fonts/GoNotoCJKCore.ttf", render_scale = self.TILESIZE*10/DOPO_FONT_DIV, TEXT_HEIGHT_SCALE = 0.8, TEXT_OFFSET = {x=10,y=-20}, FONTSCALE = 0.1*DOPO_FONT_DIV, squish = 1, DESCSCALE = 1},
--! @endpatch

--! @patch globals.lua 1
DOPO_FONT_DIV = tonumber(os.getenv('DOPO_FONT_DIV') or '') or 2
--! @endpatch
