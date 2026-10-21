-- Low graphics defaults
--
-- New settings start with the CRT effect, bloom and shadows off.

--! @patch globals.lua 191-193
            shadows = 'Off',
            crt = 0,
            bloom = 0
--! @endpatch
