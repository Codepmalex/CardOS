local lane, sc, over, lm = 2, 0, false, 0
local cars = {}
local LX = {84, 120, 156}
function onEnter() math.randomseed(cardos.millis()); lane, sc, over, lm = 2, 0, false, cardos.millis(); cars = {{l=1,y=18},{l=3,y=-40}} end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  sc = sc + 1; local sp = math.min(5.2, 2.2 + sc / 350)
  for _, c in ipairs(cars) do
    c.y = c.y + sp
    if c.y > 136 then c.y = -20; c.l = math.random(3) end
    if c.l == lane and math.abs(c.y - 110) < 16 then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then lane = math.max(1, lane - 1)
  elseif k == "right" or c == "/" or cl == "d" then lane = math.min(3, lane + 1) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Turbo Racer  M:" .. sc)
  cardos.fillRect(64, 16, 112, 119, 0, "surface"); cardos.drawRect(64, 16, 112, 119, 0, "border")
  for y = 20, 128, 16 do cardos.fillRect(101, y, 2, 8, 0, "dim"); cardos.fillRect(137, y, 2, 8, 0, "dim") end
  cardos.fillRect(LX[lane] - 9, 102, 18, 22, 3, "accent")
  for _, c in ipairs(cars) do cardos.fillRect(LX[c.l] - 9, math.floor(c.y), 18, 22, 3, "err") end
  if over then
    cardos.fillRect(42, 46, 156, 44, 5, "surface"); cardos.drawRect(42, 46, 156, 44, 5, "err")
    cardos.text("CRASH! Dist:" .. sc, 120, 60, "err", 1, "C"); cardos.text("[Ent] Retry  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

