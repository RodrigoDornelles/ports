-- Release the old canvas
--
-- Frees the previous screen canvas before creating a new one on resize,
-- instead of holding both until the garbage collector runs; before the
-- (disabled) high DPI branch, which would make a new AA_CANVAS.

--! @patch main.lua 370
	-- DOPO_CANVAS_RELEASE
	if G.CANVAS and G.CANVAS.release then pcall(function() G.CANVAS:release() end) end
	if G.AA_CANVAS and G.AA_CANVAS.release then pcall(function() G.AA_CANVAS:release() end) end
	collectgarbage('collect')
--! @endpatch
