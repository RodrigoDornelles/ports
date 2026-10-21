-- CRT only when on
--
-- Skips the CRT shader while its intensity is zero, and gives the shader a
-- cheap path when all of its effects are off.

--! @patch game.lua 2952-2952
        if G.SETTINGS.GRAPHICS.crt > 0 then love.graphics.setShader( G.SHADERS['CRT']) else love.graphics.setShader() end
--! @endpatch
