-- Real reduced motion
--
-- With reduced motion on, cards and text stop computing their wobble every
-- frame instead of multiplying it by zero.

--! @patch cardarea.lua 439-439
                card.T.r = 0.4*(-#self.cards/2 - 0.5 + k)/(#self.cards)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+card.T.x))
--! @endpatch

--! @patch cardarea.lua 444-444
                card.T.y = G.hand.T.y - 1.8*card.T.h - highlight_height + (G.SETTINGS.reduced_motion and 0 or 0.1*math.sin(0.666*G.TIMERS.REAL+card.T.x)) + math.abs(1.3*(-#self.cards/2 + k-0.5)/(#self.cards))^2-0.3
--! @endpatch

--! @patch cardarea.lua 454-454
                card.T.r = 0.2*(-#self.cards/2 - 0.5 + k)/(#self.cards)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+card.T.x))
--! @endpatch

--! @patch cardarea.lua 460-460
                card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height + (G.SETTINGS.reduced_motion and 0 or 0.03*math.sin(0.666*G.TIMERS.REAL+card.T.x)) + math.abs(0.5*(-#self.cards/2 + k-0.5)/(#self.cards))-0.2
--! @endpatch

--! @patch cardarea.lua 469-469
                card.T.r = 0.2*(-#self.cards/2 - 0.5 + k)/(#self.cards)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+card.T.x))
--! @endpatch

--! @patch cardarea.lua 474-474
                card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height + (G.SETTINGS.reduced_motion and 0 or 0.03*math.sin(0.666*G.TIMERS.REAL+card.T.x)) + math.abs(0.5*(-#self.cards/2 + k-0.5)/(#self.cards))-(#self.cards>1 and 0.2 or 0)
--! @endpatch

--! @patch cardarea.lua 484-484
                card.T.r = 0.2*(-#self.cards/2 - 0.5 + k)/(#self.cards)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+card.T.x+card.T.y)) + (k%2 == 0 and 1 or -1)*0.08
--! @endpatch

--! @patch cardarea.lua 489-489
                card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height + (G.SETTINGS.reduced_motion and 0 or 0.03*math.sin(0.666*G.TIMERS.REAL+card.T.x)) + math.abs(0.5*(-#self.cards/2 + k-0.5)/(#self.cards))-(#self.cards>1 and 0.2 or 0)
--! @endpatch

--! @patch cardarea.lua 512-512
                card.T.r = 0.1*(-#self.cards/2 - 0.5 + k)/(#self.cards)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+card.T.x))
--! @endpatch

--! @patch cardarea.lua 524-524
                card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height+ (G.SETTINGS.reduced_motion and 0 or 0.03*math.sin(0.666*G.TIMERS.REAL+card.T.x))
--! @endpatch

--! @patch cardarea.lua 540-540
                card.T.y = self.T.y + self.T.h/2 - card.T.h/2 - highlight_height + (not card.highlighted and (G.SETTINGS.reduced_motion and 0 or 0.05*math.sin(2*1.666*G.TIMERS.REAL+card.T.x)) or 0)
--! @endpatch

--! @patch engine/text.lua 215-215
        if self.config.rotate then letter.r = (self.config.rotate == 2 and -1 or 1)*(0.2*(-#self.strings[self.focused_string].letters/2 - 0.5 + k)/(#self.strings[self.focused_string].letters)+ (G.SETTINGS.reduced_motion and 0 or 0.02*math.sin(2*G.TIMERS.REAL+k))) end
--! @endpatch

--! @patch engine/text.lua 226-226
        if self.config.quiver and not G.SETTINGS.reduced_motion then
--! @endpatch

--! @patch engine/text.lua 234-235
        if self.config.float then letter.offset.y = (G.SETTINGS.reduced_motion and 0 or math.sqrt(self.scale)*(2+(self.font.FONTSCALE/G.TILESIZE)*2000*math.sin(2.666*G.TIMERS.REAL+200*k))) + 60*(letter.scale-1) end
        if self.config.bump then letter.offset.y = (G.SETTINGS.reduced_motion and 0 or self.bump_amount*math.sqrt(self.scale)*7*math.max(0, (5+self.bump_rate)*math.sin(self.bump_rate*G.TIMERS.REAL+200*k) - 3 - self.bump_rate)) end
--! @endpatch
