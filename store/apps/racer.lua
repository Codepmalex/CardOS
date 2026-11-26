local lane, cx, sc, over, pick, dif, lm, rOff = 2, 120, 0, false, true, 2, 0, 0
local cars = {}
local LX = {84, 120, 156}
local BASE = {1.9, 2.5, 3.3}
local DN = {"Easy", "Normal", "Hard"}
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  lane, cx, sc, over, lm, rOff = 2, LX[2], 0, false, cardos.millis(), 0
  cars = {{l = 1, y = 18}, {l = 3, y = -40}}
  if dif == 3 then table.insert(cars, {l = 2, y = -95}) end
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now
  local tx = LX[lane]
  if math.abs(tx - cx) > 0.5 then cx = cx + (tx - cx) * 0.35 else cx = tx end
  sc = sc + 1; local sp = math.min(6.0, BASE[dif] + sc / (420 - dif * 60))
  rOff = (rOff + sp * 1.4) % 16
  for _, c in ipairs(cars) do
    c.y = c.y + sp
    if c.y > 136 then c.y = -24; c.l = math.random(3) end
    if math.abs(LX[c.l] - cx) < 14 and math.abs(c.y - 110) < 16 then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end; return end
  if k == "left" or c == "," or cl == "a" then lane = math.max(1, lane - 1)
  elseif k == "right" or c == "/" or cl == "d" then lane = math.min(3, lane + 1) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Turbo Racer [" .. DN[dif] .. "] " .. sc .. "m")
  cardos.fillRect(64, 16, 112, 119, 0, "surface"); cardos.drawRect(64, 16, 112, 119, 0, "border")
  for y = math.floor(rOff) + 16, 132, 16 do cardos.fillRect(101, y, 2, 8, 0, "dim"); cardos.fillRect(137, y, 2, 8, 0, "dim") end
  if not pick then
    local icx = math.floor(cx)
    cardos.fillRect(icx - 9, 102, 18, 22, 3, "accent")
    cardos.fillRect(icx - 6, 124, 3, (sc % 2 == 0) and 3 or 5, 0, "warn")
    cardos.fillRect(icx + 3, 124, 3, (sc % 2 == 0) and 3 or 5, 0, "warn")
    for _, c in ipairs(cars) do cardos.fillRect(LX[c.l] - 9, math.floor(c.y), 18, 22, 3, "err") end
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("TURBO RACER - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Smooth animated lane glide", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("CRASH! Dist:" .. sc .. "m", 120, 58, "err", 1, "C"); cardos.text("[Ent] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
