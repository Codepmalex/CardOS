local px, sc, lv, over, pick, dif, lm, frm = 104, 0, 3, false, true, 2, 0, 0
local items, pops = {}, {}
local SPN = {28, 21, 14}
local DN = {"Easy", "Normal", "Hard"}
local function addItem()
  table.insert(items, {x = math.random(12, 228), y = 18, bad = (math.random(5 - dif) == 1), sp = (1.5 + dif * 0.45) + math.min(2.2, sc / 150)})
end
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  px, sc, lv, over, lm, frm = 104, 0, 3, false, cardos.millis(), 0; items, pops = {}, {}; addItem()
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now; frm = frm + 1
  if cardos.isKeyDown("left") then px = math.max(4, px - 3.8) end
  if cardos.isKeyDown("right") then px = math.min(204, px + 3.8) end
  for i = #pops, 1, -1 do pops[i].y = pops[i].y - 0.8; pops[i].t = pops[i].t - 1; if pops[i].t <= 0 then table.remove(pops, i) end end
  if math.random(SPN[dif]) == 1 and #items < 4 + dif * 2 then addItem() end
  for i = #items, 1, -1 do
    local it = items[i]; it.y = it.y + it.sp
    if it.y >= 115 and it.y <= 128 and it.x >= px - 4 and it.x <= px + 36 then
      if it.bad then lv = lv - 1; table.insert(pops, {x = it.x, y = 110, txt = "BOOM!", c = "err", t = 12}); if lv <= 0 then over = true end
      else sc = sc + 10 * dif; table.insert(pops, {x = it.x, y = 110, txt = "+$" .. (10 * dif), c = "ok", t = 12}) end
      table.remove(items, i)
    elseif it.y > 135 then table.remove(items, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Coin Catch [" .. DN[dif] .. "] $" .. sc .. " L:" .. lv)
  if not pick then
    cardos.fillRect(math.floor(px), 118, 32, 8, 3, "accent")
    local shim = (math.floor(frm / 3) % 2 == 0) and 5 or 4
    for _, it in ipairs(items) do
      cardos.circle(math.floor(it.x), math.floor(it.y), it.bad and 5 or shim, it.bad and "err" or "warn", true)
      cardos.text(it.bad and "X" or "$", math.floor(it.x), math.floor(it.y), "bg", 1, "C")
    end
    for _, p in ipairs(pops) do cardos.text(p.txt, math.floor(p.x), math.floor(p.y), p.c, 1, "C") end
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("COIN CATCH - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold A/D or Left/Right smoothly", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("GAME OVER! $" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Ent] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
