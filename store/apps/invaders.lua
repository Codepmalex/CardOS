local px, sx, sy, sOn, dir, sc, over, win, pick, dif, lm, frm = 114, 0, 0, false, 1, 0, false, false, true, 2, 0, 0
local al, ab, ex = {}, {}, {}
local ASP = {0.45, 0.72, 1.05}
local FIR = {34, 22, 13}
local DN = {"Easy", "Normal", "Hard"}
local function mkWave() al = {}; for r = 1, 3 do for c = 1, 7 do table.insert(al, {x=22+(c-1)*26, y=24+(r-1)*13, alive=true}) end end end
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  px, sOn, sc, over, win, dir, lm, frm = 114, false, 0, false, false, 1, cardos.millis(), 0
  ab, ex = {}, {}; mkWave()
end
function onEnter() pick = true; over = false; win = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or win or now - lm < 25 then return end; lm = now; frm = frm + 1
  if cardos.isKeyDown("left") then px = math.max(6, px - 3.1) end
  if cardos.isKeyDown("right") then px = math.min(220, px + 3.1) end
  if cardos.isKeyDown("space") and not sOn then sOn, sx, sy = true, math.floor(px) + 7, 116 end
  for i = #ex, 1, -1 do ex[i].t = ex[i].t - 1; if ex[i].t <= 0 then table.remove(ex, i) end end
  if sOn then
    sy = sy - 5.5
    if sy < 18 then sOn = false else
      for _, a in ipairs(al) do if a.alive and sx >= a.x and sx <= a.x + 14 and sy >= a.y and sy <= a.y + 8 then
        a.alive = false; sOn = false; sc = sc + 20 * dif; table.insert(ex, {x = a.x + 7, y = a.y + 4, t = 8}); break
      end end
    end
  end
  local edge, rem = false, 0
  for _, a in ipairs(al) do if a.alive then rem = rem + 1; a.x = a.x + dir * ASP[dif]; if a.x > 218 or a.x < 8 then edge = true end; if a.y >= 112 then over = true end end end
  if rem == 0 then win = true; return end
  if edge then dir = -dir; for _, a in ipairs(al) do a.y = a.y + 5 end end
  if math.random(FIR[dif]) == 1 and #ab < 2 + dif then
    local live = {}; for _, a in ipairs(al) do if a.alive then table.insert(live, a) end end
    if #live > 0 then local pickA = live[math.random(#live)]; table.insert(ab, {x = pickA.x + 7, y = pickA.y + 9}) end
  end
  for i = #ab, 1, -1 do
    ab[i].y = ab[i].y + 2.1 + dif * 0.4
    if ab[i].x >= px and ab[i].x <= px + 14 and ab[i].y >= 118 and ab[i].y <= 128 then over = true end
    if ab[i].y > 134 then table.remove(ab, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over or win then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end; return end
  if (c == " " or k == "enter" or k == "up" or cl == "w") and not sOn then sOn, sx, sy = true, math.floor(px) + 7, 116 end
end
function onRender()
  cardos.clear(); cardos.statusBar("Invaders [" .. DN[dif] .. "] " .. sc)
  local flap = (math.floor(frm / 10) % 2 == 0)
  for _, a in ipairs(al) do if a.alive then
    local ax, ay = math.floor(a.x), math.floor(a.y)
    cardos.fillRect(ax + 2, ay, 10, 6, 1, "ok")
    cardos.fillRect(ax, flap and ay + 2 or ay + 4, 14, 3, 1, "accent2")
  end end
  for _, e in ipairs(ex) do cardos.circle(math.floor(e.x), math.floor(e.y), 9 - e.t, "warn", false) end
  if sOn then cardos.fillRect(sx - 1, math.floor(sy), 2, 6, 0, "warn") end
  for _, b in ipairs(ab) do cardos.fillRect(math.floor(b.x), math.floor(b.y), 2, 5, 0, "err") end
  local ipx = math.floor(px)
  cardos.fillRect(ipx, 121, 14, 6, 2, "accent"); cardos.fillRect(ipx + 5, 117, 4, 4, 1, "accent")
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("INVADERS - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold A/D smoothly + Space fire", 120, 88, "dim", 1, "C")
  elseif over or win then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, win and "ok" or "err")
    cardos.text(win and "EARTH SAVED! " .. sc or "GAME OVER " .. sc, 120, 58, win and "ok" or "err", 1, "C")
    cardos.text("[Ent] Again [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
