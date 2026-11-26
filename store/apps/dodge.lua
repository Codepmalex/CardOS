local px, sc, over, pick, dif, lm, frm = 114, 0, false, true, 2, 0, 0
local met = {}
local CNT = {5, 7, 10}
local MUL = {0.8, 1.0, 1.35}
local DN = {"Easy", "Normal", "Hard"}
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  px, sc, over, lm, frm = 114, 0, false, cardos.millis(), 0; met = {}
  for i = 1, CNT[dif] do table.insert(met, {x = math.random(8, 224), y = 20 - i * 16, r = math.random(4, 7), sp = (1.6 + math.random() * 1.4) * MUL[dif]}) end
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now; frm = frm + 1
  if cardos.isKeyDown("left") then px = math.max(6, px - 3.4) end
  if cardos.isKeyDown("right") then px = math.min(220, px + 3.4) end
  sc = sc + 1
  for _, m in ipairs(met) do
    m.y = m.y + m.sp + sc * 0.0018 * dif
    if m.y > 136 then m.y = 18; m.x = math.random(8, 224); m.r = math.random(4, 8) end
    if math.abs(m.x - (px + 7)) < m.r + 5 and math.abs(m.y - 122) < m.r + 5 then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Meteor Dodge [" .. DN[dif] .. "] " .. sc)
  if not pick then
    local ipx = math.floor(px)
    cardos.fillRect(ipx, 117, 14, 10, 3, "accent")
    cardos.fillRect(ipx + 4, 127, 6, (frm % 2 == 0) and 4 or 6, 1, "warn")
    for _, m in ipairs(met) do
      local mx, my = math.floor(m.x), math.floor(m.y)
      cardos.fillRect(mx - 1, my - m.r - 5, 2, 5, 0, "err")
      cardos.circle(mx, my, m.r, "warn", true)
    end
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("METEOR DODGE - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold A/D or Left/Right smoothly", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("HIT! Score:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Ent] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
