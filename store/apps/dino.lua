local dy, vy, sc, sp, over, pick, dif, lm, frm, gOff = 96, 0, 0, 3.0, false, true, 2, 0, 0, 0
local obs, dust = {}, {}
local BASE = {2.5, 3.3, 4.3}
local DN = {"Easy", "Normal", "Hard"}
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  dy, vy, sc, sp, over, lm, frm, gOff = 96, 0, 0, BASE[dif], false, cardos.millis(), 0, 0
  obs = {{x = 240, w = 10, h = 18}, {x = 370, w = 12, h = 14}}; dust = {}
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or now - lm < 25 then return end; lm = now; frm = frm + 1
  local wasAir = dy < 95
  dy = dy + vy; vy = vy + 0.9
  if dy >= 96 then
    dy, vy = 96, 0
    if wasAir then for i = 1, 3 do table.insert(dust, {x = 34 - i * 3, y = 112, t = 7}) end end
  end
  sc = sc + 1; sp = math.min(7.5, BASE[dif] + sc / (550 - dif * 80))
  gOff = (gOff + sp) % 24
  for i = #dust, 1, -1 do dust[i].x = dust[i].x - sp; dust[i].t = dust[i].t - 1; if dust[i].t <= 0 then table.remove(dust, i) end end
  for _, o in ipairs(obs) do
    o.x = o.x - sp
    if o.x < -16 then o.x = 240 + math.random(45, 130 - dif * 12); o.w = math.random(8, 14); o.h = math.random(12, 22) end
    if o.x < 43 and o.x + o.w > 27 and dy + 18 > 114 - o.h then over = true end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" or c == " " then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" or c == " " then start(dif) elseif cl == "d" then pick = true end; return end
  if (k == "up" or c == ";" or cl == "w" or c == " ") and dy >= 95 then
    vy = -9.6; for i = 1, 3 do table.insert(dust, {x = 28 + i * 2, y = 112, t = 8}) end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Dino [" .. DN[dif] .. "] " .. sc)
  cardos.fillRect(0, 114, 240, 3, 0, "border")
  for gx = -math.floor(gOff), 240, 24 do cardos.fillRect(gx, 119, 10, 2, 0, "dim") end
  for _, d in ipairs(dust) do cardos.fillRect(math.floor(d.x), d.y, 2, 2, 0, "dim") end
  local idy = math.floor(dy)
  cardos.fillRect(26, idy, 18, 14, 3, "ok"); cardos.fillRect(38, idy + 3, 3, 3, 0, "bg")
  local leg = (dy >= 95 and math.floor(frm / 4) % 2 == 0)
  cardos.fillRect(28, idy + 14, 4, leg and 4 or 2, 0, "ok"); cardos.fillRect(37, idy + 14, 4, leg and 2 or 4, 0, "ok")
  for _, o in ipairs(obs) do cardos.fillRect(math.floor(o.x), 114 - o.h, o.w, o.h, 2, "err") end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("DINO RUN - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("[Space / W / Up] Jump", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("CRASHED! Score:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Spc] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
