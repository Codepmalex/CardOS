local py, sc, over, pick, dif, lm, lastShot, frm = 68, 0, false, true, 2, 0, 0, 0
local bul, en, st, ex = {}, {}, {}, {}
local SPN = {26, 18, 11}
local DN = {"Easy", "Normal", "Hard"}
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  py, sc, over, lm, lastShot, frm = 68, 0, false, cardos.millis(), 0, 0; bul, en, ex, st = {}, {}, {}, {}
  for i = 1, 14 do table.insert(st, {x = math.random(240), y = math.random(20, 128), s = math.random(1, 3)}) end
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now; frm = frm + 1
  if cardos.isKeyDown("up") then py = math.max(22, py - 2.9) end
  if cardos.isKeyDown("down") then py = math.min(122, py + 2.9) end
  if cardos.isKeyDown("space") and now - lastShot >= 180 and #bul < 6 then lastShot = now; table.insert(bul, {x = 26, y = py}) end
  for _, s in ipairs(st) do s.x = s.x - s.s; if s.x < 0 then s.x = 240; s.y = math.random(20, 128) end end
  for i = #ex, 1, -1 do ex[i].t = ex[i].t - 1; if ex[i].t <= 0 then table.remove(ex, i) end end
  if math.random(SPN[dif]) == 1 then table.insert(en, {x = 242, y = math.random(24, 118), sp = (1.5 + dif * 0.45) + math.min(2.2, sc / 250)}) end
  for i = #bul, 1, -1 do bul[i].x = bul[i].x + 6; if bul[i].x > 242 then table.remove(bul, i) end end
  for i = #en, 1, -1 do
    local e = en[i]; e.x = e.x - e.sp
    if e.x < 24 and math.abs(e.y - py) < 10 then over = true end
    local hit = false
    for j = #bul, 1, -1 do
      if math.abs(bul[j].x - e.x) < 10 and math.abs(bul[j].y - e.y) < 10 then
        table.remove(bul, j); hit = true; sc = sc + 10 * dif; table.insert(ex, {x = e.x, y = e.y, t = 7}); break
      end
    end
    if hit or e.x < 0 then table.remove(en, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end; return end
  if (c == " " or k == "enter") and #bul < 6 then table.insert(bul, {x = 26, y = py}) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Star Blaster [" .. DN[dif] .. "] " .. sc)
  for _, s in ipairs(st) do cardos.fillRect(math.floor(s.x), s.y, 2, 1, 0, "dim") end
  if not pick then
    local ipy = math.floor(py)
    cardos.fillRect(5, ipy - 2, (frm % 2 == 0) and 5 or 3, 4, 0, "warn")
    cardos.fillRect(10, ipy - 5, 14, 10, 2, "accent")
    for _, b in ipairs(bul) do cardos.fillRect(math.floor(b.x), math.floor(b.y) - 1, 6, 3, 1, "warn") end
    for _, e in ipairs(en) do cardos.fillRect(math.floor(e.x), math.floor(e.y) - 5, 11, 10, 2, "err") end
    for _, x in ipairs(ex) do cardos.circle(math.floor(x.x), math.floor(x.y), 9 - x.t, "warn", false) end
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("STAR BLASTER - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold W/S smoothly + Space fire", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("DESTROYED! S:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Ent] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
