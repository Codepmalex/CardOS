local px, sx, sy, sOn, ex, ey, dir, sc, over, win, lm = 114, 0, 0, false, 0, 0, 1, 0, false, false, 0
local al, ab = {}, {}
local function mkWave()
  al = {}; for r = 1, 3 do for c = 1, 7 do table.insert(al, {x=22 + (c-1)*26, y=24 + (r-1)*13, alive=true}) end end
end
function onEnter() math.randomseed(cardos.millis()); px, sOn, sc, over, win, dir, lm = 114, false, 0, false, false, 1, cardos.millis(); ab = {}; mkWave() end
function onTick()
  local now = cardos.millis(); if over or win or now - lm < 30 then return end; lm = now
  if sOn then
    sy = sy - 5
    if sy < 18 then sOn = false else
      for _, a in ipairs(al) do if a.alive and sx >= a.x and sx <= a.x + 14 and sy >= a.y and sy <= a.y + 8 then a.alive = false; sOn = false; sc = sc + 20; break end end
    end
  end
  local edge, rem = false, 0
  for _, a in ipairs(al) do if a.alive then rem = rem + 1; a.x = a.x + dir * 0.6; if a.x > 218 or a.x < 8 then edge = true end; if a.y >= 112 then over = true end end end
  if rem == 0 then win = true; return end
  if edge then dir = -dir; for _, a in ipairs(al) do a.y = a.y + 5 end end
  if math.random(28) == 1 and #ab < 3 then
    local live = {}; for _, a in ipairs(al) do if a.alive then table.insert(live, a) end end
    if #live > 0 then local pick = live[math.random(#live)]; table.insert(ab, {x=pick.x + 7, y=pick.y + 9}) end
  end
  for i = #ab, 1, -1 do
    ab[i].y = ab[i].y + 2.5
    if ab[i].x >= px and ab[i].x <= px + 14 and ab[i].y >= 118 and ab[i].y <= 128 then over = true end
    if ab[i].y > 134 then table.remove(ab, i) end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (over or win) and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then px = math.max(6, px - 12)
  elseif k == "right" or c == "/" or cl == "d" then px = math.min(220, px + 12)
  elseif c == " " or k == "enter" or k == "up" or cl == "w" then if not sOn then sOn, sx, sy = true, px + 7, 116 end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Invaders  Score:" .. sc)
  for _, a in ipairs(al) do if a.alive then cardos.fillRect(math.floor(a.x), math.floor(a.y), 14, 8, 2, "ok") end end
  if sOn then cardos.fillRect(sx - 1, sy, 2, 6, 0, "warn") end
  for _, b in ipairs(ab) do cardos.fillRect(math.floor(b.x), math.floor(b.y), 2, 5, 0, "err") end
  cardos.fillRect(px, 121, 14, 6, 2, "accent"); cardos.fillRect(px + 5, 117, 4, 4, 1, "accent")
  if over or win then
    cardos.fillRect(36, 48, 168, 42, 5, "surface"); cardos.drawRect(36, 48, 168, 42, 5, win and "ok" or "err")
    cardos.text(win and "EARTH SAVED! " .. sc or "GAME OVER " .. sc, 120, 62, win and "ok" or "err", 1, "C")
    cardos.text("[Ent] Again  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

