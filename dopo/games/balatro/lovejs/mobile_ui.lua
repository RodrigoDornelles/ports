-- Mobile UI
--
-- Phones and small screens: on every resize, the screen standing or its
-- short side under 600 pixels turns on the bigger card text of the
-- F_MOBILE_UI the game already has; and a standing screen keeps its height,
-- which the game would cut to a square at the top, so the table sits in
-- the middle of it (cards then fly in from above and below).

--! @patch main.lua 325-327
	G.F_MOBILE_UI = w < h or math.min(w, h) < 600
--! @endpatch
