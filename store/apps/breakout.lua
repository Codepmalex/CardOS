local px, pw, bx, by, vx, vy, sc, lv, over, win, pick, dif, lm = 100, 38, 120, 108, 2, -2, 0, 3, false, false, true, 2, 0
local br, spk, tr = {}, {}, {}
local COLS = {"err","warn","ok","accent"}
local PW = {46, 38, 30}
local BSP = {1.7, 2.2, 2.8}
local DN = {"Easy", "Normal", "Hard"}
local function mkBricks() br = {}; for r = 1, 4 do br[r] = {}; for c = 1, 8 do br[r][c] = true end end end
local function start(d)
  dif = d or dif; pick = false; pw = PW[dif]
  math.randomseed(cardos.millis())
  px, sc, lv, over, win, lm = 120 - pw / 2, 0, 3, false, false, cardos.millis()
  mkBricks(); spk, tr = {}, {}
  local s = BSP[dif]; bx, by, vx, vy = 120, 108, s, -s
end
function onEnter() pick = true; over = false; win = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over or win or now - lm < 25 then return end; lm = now
  if cardos.isKeyDown("left") then px = math.max(4, px - 3.6) end
  if cardos.isKeyDown("right") then px = math.min(236 - pw, px + 3.6) end
  table.insert(tr, 1, {x = bx, y = by}); if #tr > 4 then table.remove(tr) end
  for i = #spk, 1, -1 do
    spk[i].x = spk[i].x + spk[i].vx; spk[i].y = spk[i].y + spk[i].vy; spk[i].t = spk[i].t - 1
    if spk[i].t <= 0 then table.remove(spk, i) end
  end
  bx, by = bx + vx, by + vy
  if bx < 6 then bx = 6; vx = -vx elseif bx > 234 then bx = 234; vx = -vx end
  if by < 20 then by = 20; vy = -vy end
  if by >= 119 and by <= 125 and bx >= px - 2 and bx <= px + pw + 2 and vy > 0 then
    vy = -math.abs(vy); vx = (bx - (px + pw / 2)) * (0.14 + dif * 0.02)
  end
  local rem = 0
  for r = 1, 4 do for c = 1, 8 do if br[r][c] then
    rem = rem + 1
    local rx, ry = 8 + (c - 1) * 28, 24 + (r - 1) * 11
    if bx >= rx and bx <= rx + 25 and by >= ry and by <= ry + 8 then
      br[r][c] = false; vy = -vy; sc = sc + 10 * dif; rem = rem - 1
      for k = 1, 3 do table.insert(spk, {x = rx + 12, y = ry + 4, vx = (math.random() - 0.5) * 3, vy = (math.random() - 0.5) * 3, t = 8}) end
    end
  end end end
  if rem == 0 then win = true end
  if by > 134 then lv = lv - 1; if lv <= 0 then over = true else local s = BSP[dif]; bx, by, vx, vy, tr = px + pw / 2, 112, s, -s, {} end end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then
    if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end
    return
  end
  if (over or win) then
    if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Breakout [" .. DN[dif] .. "] " .. sc .. " L:" .. lv)
  for r = 1, 4 do for c = 1, 8 do if br[r][c] then cardos.fillRect(8 + (c - 1) * 28, 24 + (r - 1) * 11, 25, 8, 2, COLS[r]) end end end
  for _, s in ipairs(spk) do cardos.fillRect(math.floor(s.x), math.floor(s.y), 2, 2, 0, "warn") end
  for i, p in ipairs(tr) do cardos.circle(math.floor(p.x), math.floor(p.y), math.max(1, 3 - i), "dim", true) end
  cardos.fillRect(math.floor(px), 122, pw, 5, 2, "accent"); cardos.circle(math.floor(bx), math.floor(by), 3, "text", true)
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("BREAKOUT - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold A/D or Left/Right smoothly", 120, 88, "dim", 1, "C")
  elseif over or win then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, win and "ok" or "err")
    cardos.text(win and "CLEARED! " .. sc or "GAME OVER " .. sc, 120, 58, win and "ok" or "err", 1, "C")
    cardos.text("[Ent] Again [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
