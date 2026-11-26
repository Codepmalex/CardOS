local py, sc, over, lm = 68, 0, false, 0
local bul, en = {}, {}
function onEnter() math.randomseed(cardos.millis()); py, sc, over, lm = 68, 0, false, cardos.millis(); bul, en = {}, {} end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  if math.random(20) == 1 then table.insert(en, {x=242, y=math.random(24, 118), sp=1.8 + math.min(2, sc / 250)}) end
  for i = #bul, 1, -1 do bul[i].x = bul[i].x + 6; if bul[i].x > 242 then table.remove(bul, i) end end
  for i = #en, 1, -1 do
    local e = en[i]; e.x = e.x - e.sp
    if e.x < 24 and math.abs(e.y - py) < 10 then over = true end
    local hit = false
    for j = #bul, 1, -1 do
      if math.abs(bul[j].x - e.x) < 10 and math.abs(bul[j].y - e.y) < 10 then table.remove(bul, j); hit = true; sc = sc + 10; break end
    end
    if hit or e.x < 0 then table.remove(en, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "up" or c == ";" or cl == "w" then py = math.max(22, py - 10)
  elseif k == "down" or c == "." or cl == "s" then py = math.min(122, py + 10)
  elseif c == " " or k == "enter" then if #bul < 5 then table.insert(bul, {x=26, y=py}) end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Star Blaster  S:" .. sc)
  cardos.fillRect(10, py - 5, 14, 10, 2, "accent")
  for _, b in ipairs(bul) do cardos.fillRect(math.floor(b.x), math.floor(b.y) - 1, 6, 3, 1, "warn") end
  for _, e in ipairs(en) do cardos.fillRect(math.floor(e.x), math.floor(e.y) - 5, 11, 10, 2, "err") end
  if over then
    cardos.fillRect(42, 46, 156, 44, 5, "surface"); cardos.drawRect(42, 46, 156, 44, 5, "err")
    cardos.text("DESTROYED! S:" .. sc, 120, 60, "err", 1, "C"); cardos.text("[Ent] Retry  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

