local px, sc, over, lm = 114, 0, false, 0
local met = {}
function onEnter()
  math.randomseed(cardos.millis()); px, sc, over, lm = 114, 0, false, cardos.millis(); met = {}
  for i = 1, 7 do table.insert(met, {x=math.random(8, 224), y=20 - i * 18, r=math.random(4, 7), sp=1.8 + math.random() * 1.6}) end
end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  sc = sc + 1
  for _, m in ipairs(met) do
    m.y = m.y + m.sp + sc * 0.002
    if m.y > 136 then m.y = 18; m.x = math.random(8, 224); m.r = math.random(4, 8) end
    if math.abs(m.x - (px + 7)) < m.r + 5 and math.abs(m.y - 122) < m.r + 5 then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then px = math.max(6, px - 14)
  elseif k == "right" or c == "/" or cl == "d" then px = math.min(220, px + 14) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Meteor Dodge  S:" .. sc)
  cardos.fillRect(px, 117, 14, 10, 3, "accent")
  for _, m in ipairs(met) do cardos.circle(math.floor(m.x), math.floor(m.y), m.r, "warn", true) end
  if over then
    cardos.fillRect(42, 46, 156, 44, 5, "surface"); cardos.drawRect(42, 46, 156, 44, 5, "err")
    cardos.text("HIT! Score:" .. sc, 120, 60, "err", 1, "C"); cardos.text("[Ent] Retry  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

