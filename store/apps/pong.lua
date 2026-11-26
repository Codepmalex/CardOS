local py, cy, bx, by, vx, vy, ps, cs, over, lm = 58, 58, 120, 74, 2, 1.5, 0, 0, false, 0
local function resetBall(d) bx, by, vx, vy = 120, 74, d * 2.2, ((cardos.millis() % 2 == 0) and 1 or -1) * 1.6 end
function onEnter() math.randomseed(cardos.millis()); py, cy, ps, cs, over, lm = 58, 58, 0, 0, false, cardos.millis(); resetBall(1) end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  bx, by = bx + vx, by + vy
  if by < 22 then by = 22; vy = -vy elseif by > 128 then by = 128; vy = -vy end
  if cy + 12 < by then cy = math.min(106, cy + 2.2) elseif cy + 12 > by then cy = math.max(20, cy - 2.2) end
  if bx <= 16 and bx >= 10 and by >= py - 2 and by <= py + 26 then bx = 17; vx = math.abs(vx) * 1.05; vy = (by - (py + 12)) * 0.18 end
  if bx >= 224 and bx <= 230 and by >= cy - 2 and by <= cy + 26 then bx = 223; vx = -math.abs(vx) * 1.05; vy = (by - (cy + 12)) * 0.18 end
  if bx < 4 then cs = cs + 1; if cs >= 7 then over = true else resetBall(1) end
  elseif bx > 236 then ps = ps + 1; if ps >= 7 then over = true else resetBall(-1) end end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "up" or c == ";" or cl == "w" then py = math.max(20, py - 9)
  elseif k == "down" or c == "." or cl == "s" then py = math.min(106, py + 9) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Pong  " .. ps .. " - " .. cs)
  for y = 20, 128, 10 do cardos.fillRect(119, y, 2, 5, 0, "border") end
  cardos.fillRect(10, py, 4, 24, 1, "accent"); cardos.fillRect(226, cy, 4, 24, 1, "err")
  cardos.circle(math.floor(bx), math.floor(by), 3, "text", true)
  if over then
    cardos.fillRect(42, 46, 156, 46, 5, "surface"); cardos.drawRect(42, 46, 156, 46, 5, ps >= 7 and "ok" or "err")
    cardos.text(ps >= 7 and "YOU WIN!" or "CPU WINS!", 120, 60, ps >= 7 and "ok" or "err", 1, "C")
    cardos.text("[Ent] Again  [ESC] Exit", 120, 78, "dim", 1, "C")
  end
end

