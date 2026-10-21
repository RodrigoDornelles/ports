-- 1x textures, no mipmaps
--
-- Loads the 1x sprite atlases whatever the save says, without mipmaps.
-- DOPO_TEXTURE_SCALE overrides the scale.

--! @patch game.lua 51-51
        self.SETTINGS.GRAPHICS.texture_scaling = DOPO_TEXTURE_SCALE
--! @endpatch

--! @patch game.lua 153-153
        self.SETTINGS.GRAPHICS.texture_scaling = DOPO_TEXTURE_SCALE
--! @endpatch

--! @patch game.lua 999-999
    self.SETTINGS.GRAPHICS.texture_scaling = DOPO_TEXTURE_SCALE
--! @endpatch

--! @patch game.lua 1091-1091
        self.ANIMATION_ATLAS[self.animation_atli[i].name].image = love.graphics.newImage(self.animation_atli[i].path, {mipmaps = false, dpiscale = self.SETTINGS.GRAPHICS.texture_scaling})
--! @endpatch

--! @patch game.lua 1100-1100
        self.ASSET_ATLAS[self.asset_atli[i].name].image = love.graphics.newImage(self.asset_atli[i].path, {mipmaps = false, dpiscale = self.SETTINGS.GRAPHICS.texture_scaling})
--! @endpatch

--! @patch game.lua 1108-1108
        self.ASSET_ATLAS[self.asset_images[i].name].image = love.graphics.newImage(self.asset_images[i].path, {mipmaps = false, dpiscale = 1})
--! @endpatch

--! @patch globals.lua 1
DOPO_TEXTURE_SCALE = tonumber(os.getenv('DOPO_TEXTURE_SCALE') or '') or 1
--! @endpatch

--! @patch globals.lua 190-190
            texture_scaling = 1,
--! @endpatch
