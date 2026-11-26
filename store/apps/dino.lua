local dy, vy, sc, sp, over, lm = 96, 0, 0, 3.2, false, 0
local obs = {}
function onEnter() math.randomseed(cardos.millis()); dy, vy, sc, sp, over, lm = 96, 0, 0, 3.2, false, cardos.millis(); obs = {{x=240, w=10, h=18}, {x=370, w=12, h=14}} end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  dy = dy + vy; vy = vy + 0.9
  if dy >= 96 then dy, vy = 96, 0 end
  sc = sc + 1; sp = math.min(6.5, 3.2 + sc / 450)
  for _, o in ipairs(obs) do
    o.x = o.x - sp
    if o.x < -16 then o.x = 240 + math.random(50, 130); o.w = math.random(8, 14); o.h = math.random(12, 22) end
    if o.x < 44 and o.x + o.w > 26 and dy + 18 > 114 - o.h then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r" or c == " ") then onEnter(); return end
  if (k == "up" or c == ";" or cl == "w" or c == " ") and dy >= 95 then vy = -9.6 end
end
function onRender()
  cardos.clear(); cardos.statusBar("Dino Run  Score:" .. sc)
  cardos.fillRect(0, 114, 240, 3, 0, "border")
  cardos.fillRect(26, math.floor(dy), 18, 18, 3, "ok")
  cardos.fillRect(38, math.floor(dy) + 3, 3, 3, 0, "bg")
  for _, o in ipairs(obs) do cardos.fillRect(math.floor(o.x), 114 - o.h, o.w, o.h, 2, "err") end
  if over then
    cardos.fillRect(40, 44, 160, 44, 5, "surface"); cardos.drawRect(40, 44, 160, 44, 5, "err")
    cardos.text("CRASHED! Score:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Spc/Ent] Retry  [ESC] Exit", 120, 74, "dim", 1, "C")
  else
    cardos.text("[Space / Up] Jump", 120, 123, "dim", 1, "C")
  end
end

