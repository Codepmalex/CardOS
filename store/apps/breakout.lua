local px, bx, by, vx, vy, sc, lv, over, win, lm = 100, 120, 110, 2, -2, 0, 3, false, false, 0
local br = {}
local COLS = {"err","warn","ok","accent"}
local function mkBricks() br = {}; for r = 1, 4 do br[r] = {}; for c = 1, 8 do br[r][c] = true end end end
function onEnter() math.randomseed(cardos.millis()); px, sc, lv, over, win, lm = 100, 0, 3, false, false, cardos.millis(); mkBricks(); bx, by, vx, vy = 120, 108, 2, -2 end
function onTick()
  local now = cardos.millis(); if over or win or now - lm < 25 then return end; lm = now
  bx, by = bx + vx, by + vy
  if bx < 6 then bx = 6; vx = -vx elseif bx > 234 then bx = 234; vx = -vx end
  if by < 20 then by = 20; vy = -vy end
  if by >= 120 and by <= 125 and bx >= px and bx <= px + 40 and vy > 0 then vy = -math.abs(vy); vx = (bx - (px + 20)) * 0.16 end
  local rem = 0
  for r = 1, 4 do for c = 1, 8 do if br[r][c] then
    rem = rem + 1
    local rx, ry = 8 + (c - 1) * 28, 24 + (r - 1) * 11
    if bx >= rx and bx <= rx + 25 and by >= ry and by <= ry + 8 then br[r][c] = false; vy = -vy; sc = sc + 10; rem = rem - 1 end
  end end end
  if rem == 0 then win = true end
  if by > 134 then lv = lv - 1; if lv <= 0 then over = true else bx, by, vx, vy = px + 20, 112, 2, -2 end end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (over or win) and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then px = math.max(4, px - 14)
  elseif k == "right" or c == "/" or cl == "d" then px = math.min(196, px + 14) end
end
function onRender()
  cardos.clear(); cardos.statusBar("Breakout  S:" .. sc .. " L:" .. lv)
  for r = 1, 4 do for c = 1, 8 do if br[r][c] then cardos.fillRect(8 + (c - 1) * 28, 24 + (r - 1) * 11, 25, 8, 2, COLS[r]) end end end
  cardos.fillRect(px, 122, 40, 5, 2, "accent"); cardos.circle(math.floor(bx), math.floor(by), 3, "text", true)
  if over or win then
    cardos.fillRect(36, 48, 168, 44, 5, "surface"); cardos.drawRect(36, 48, 168, 44, 5, win and "ok" or "err")
    cardos.text(win and "CLEARED! S:" .. sc or "GAME OVER S:" .. sc, 120, 62, win and "ok" or "err", 1, "C")
    cardos.text("[Ent] Restart  [ESC] Exit", 120, 78, "dim", 1, "C")
  end
end

