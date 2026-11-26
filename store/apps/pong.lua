local py, cy, bx, by, vx, vy, ps, cs, over, pick, dif, lm, hitF, hitX, hitY = 58, 58, 120, 74, 2, 1.5, 0, 0, false, true, 2, 0, 0, 0, 0
local tr = {}
local SPD = {1.7, 2.3, 3.0}
local CPU = {1.5, 2.2, 2.9}
local DN = {"Easy", "Normal", "Hard"}
local function resetBall(d)
  local s = SPD[dif]
  bx, by, vx, vy = 120, 74, d * s, ((cardos.millis() % 2 == 0) and 1 or -1) * (s * 0.72)
  tr = {}
end
local function start(d)
  dif = d or dif; pick = false
  math.randomseed(cardos.millis())
  py, cy, ps, cs, over, lm, hitF = 58, 58, 0, 0, false, cardos.millis(), 0
  resetBall(1)
end
function onEnter() pick = true; over = false; ps = 0; cs = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now
  if cardos.isKeyDown("up") then py = math.max(19, py - 2.9) end
  if cardos.isKeyDown("down") then py = math.min(107, py + 2.9) end
  table.insert(tr, 1, {x = bx, y = by}); if #tr > 4 then table.remove(tr) end
  if hitF > 0 then hitF = hitF - 1 end
  bx, by = bx + vx, by + vy
  if by < 20 then by = 20; vy = -vy elseif by > 129 then by = 129; vy = -vy end
  if cy + 12 < by then cy = math.min(107, cy + CPU[dif]) elseif cy + 12 > by then cy = math.max(19, cy - CPU[dif]) end
  if bx <= 16 and bx >= 9 and by >= py - 2 and by <= py + 26 then
    bx = 17; vx = math.min(4.8, math.abs(vx) * 1.04); vy = (by - (py + 12)) * 0.17; hitF, hitX, hitY = 5, 16, by
  end
  if bx >= 224 and bx <= 231 and by >= cy - 2 and by <= cy + 26 then
    bx = 223; vx = -math.min(4.8, math.abs(vx) * 1.04); vy = (by - (cy + 12)) * 0.17; hitF, hitX, hitY = 5, 224, by
  end
  if bx < 4 then cs = cs + 1; if cs >= 7 then over = true else resetBall(1) end
  elseif bx > 236 then ps = ps + 1; if ps >= 7 then over = true else resetBall(-1) end end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then
    if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end
    return
  end
  if over then
    if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Pong [" .. DN[dif] .. "] " .. ps .. "-" .. cs)
  for y = 20, 128, 10 do cardos.fillRect(119, y, 2, 5, 0, "border") end
  for i, p in ipairs(tr) do cardos.circle(math.floor(p.x), math.floor(p.y), math.max(1, 3 - i), "dim", true) end
  cardos.fillRect(10, math.floor(py), 4, 24, 1, "accent"); cardos.fillRect(226, math.floor(cy), 4, 24, 1, "err")
  if hitF > 0 then cardos.circle(hitX, math.floor(hitY), 7 - hitF, "warn", false) end
  cardos.circle(math.floor(bx), math.floor(by), 3, "text", true)
  if pick then
    cardos.fillRect(32, 36, 176, 68, 5, "surface"); cardos.drawRect(32, 36, 176, 68, 5, "accent")
    cardos.text("PONG - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold W/S or Up/Down smoothly", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(36, 44, 168, 48, 5, "surface"); cardos.drawRect(36, 44, 168, 48, 5, ps >= 7 and "ok" or "err")
    cardos.text(ps >= 7 and "YOU WIN!" or "CPU WINS!", 120, 56, ps >= 7 and "ok" or "err", 1, "C")
    cardos.text("[Ent] Again [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
