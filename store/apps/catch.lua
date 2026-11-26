local px, sc, lv, over, lm = 104, 0, 3, false, 0
local items = {}
local function addItem() table.insert(items, {x=math.random(12, 228), y=18, bad=(math.random(4) == 1), sp=1.8 + math.min(2.2, sc / 120)}) end
function onEnter() math.randomseed(cardos.millis()); px, sc, lv, over, lm = 104, 0, 3, false, cardos.millis(); items = {}; addItem() end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  if math.random(24) == 1 and #items < 6 then addItem() end
  for i = #items, 1, -1 do
    local it = items[i]; it.y = it.y + it.sp
    if it.y >= 115 and it.y <= 128 and it.x >= px - 4 and it.x <= px + 36 then
      if it.bad then lv = lv - 1; if lv <= 0 then over = true end else sc = sc + 10 end
      table.remove(items, i)
    elseif it.y > 135 then table.remove(items, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then px = math.max(4, px - 16)
  elseif k == "right" or c == "/" or cl == "d" then px = math.min(204, px + 16) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Coin Catch  $" .. sc .. "  L:" .. lv)
  cardos.fillRect(px, 118, 32, 8, 3, "accent")
  for _, it in ipairs(items) do
    cardos.circle(math.floor(it.x), math.floor(it.y), 5, it.bad and "err" or "warn", true)
    cardos.text(it.bad and "X" or "$", math.floor(it.x), math.floor(it.y), "bg", 1, "C")
  end
  if over then
    cardos.fillRect(42, 46, 156, 44, 5, "surface"); cardos.drawRect(42, 46, 156, 44, 5, "err")
    cardos.text("GAME OVER! $" .. sc, 120, 60, "err", 1, "C"); cardos.text("[Ent] Retry  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

