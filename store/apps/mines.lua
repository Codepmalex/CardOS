local W, H, MINES = 8, 5, 6
local bd, st, cx, cy, over, win = {}, {}, 1, 1, false, false
local function reveal(x, y)
  if x < 1 or x > W or y < 1 or y > H or st[y][x] ~= 0 then return end
  st[y][x] = 1
  if bd[y][x] == 9 then over = true; return end
  if bd[y][x] == 0 then for dy = -1, 1 do for dx = -1, 1 do reveal(x + dx, y + dy) end end end
end
local function checkWin()
  for y = 1, H do for x = 1, W do if bd[y][x] ~= 9 and st[y][x] ~= 1 then return false end end end
  return true
end
function onEnter()
  math.randomseed(cardos.millis()); bd, st, cx, cy, over, win = {}, {}, 1, 1, false, false
  for y = 1, H do bd[y], st[y] = {}, {}; for x = 1, W do bd[y][x], st[y][x] = 0, 0 end end
  local m = 0; while m < MINES do local rx, ry = math.random(W), math.random(H); if bd[ry][rx] == 0 then bd[ry][rx] = 9; m = m + 1 end end
  for y = 1, H do for x = 1, W do if bd[y][x] == 0 then
    local n = 0; for dy = -1, 1 do for dx = -1, 1 do local nx, ny = x + dx, y + dy; if nx >= 1 and nx <= W and ny >= 1 and ny <= H and bd[ny][nx] == 9 then n = n + 1 end end end
    bd[y][x] = n
  end end end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if ((over or win) and k == "enter") or cl == "r" then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then cx = math.max(1, cx - 1)
  elseif k == "right" or c == "/" or cl == "d" then cx = math.min(W, cx + 1)
  elseif k == "up" or c == ";" or cl == "w" then cy = math.max(1, cy - 1)
  elseif k == "down" or c == "." or cl == "s" then cy = math.min(H, cy + 1)
  elseif cl == "f" then if st[cy][cx] == 0 then st[cy][cx] = 2 elseif st[cy][cx] == 2 then st[cy][cx] = 0 end
  elseif k == "enter" or c == " " then reveal(cx, cy); if not over then win = checkWin() end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Minesweeper (" .. MINES .. " Mines)")
  for y = 1, H do for x = 1, W do
    local px, py, s, v = 24 + (x - 1) * 24, 22 + (y - 1) * 19, st[y][x], bd[y][x]
    local sel = (x == cx and y == cy)
    local bg = (s == 1) and ((v == 9) and "err" or "surface2") or ((s == 2) and "warn" or "surface")
    cardos.fillRect(px, py, 22, 17, 2, bg); cardos.drawRect(px, py, 22, 17, 2, sel and "accent" or "border")
    if s == 2 then cardos.text("F", px + 11, py + 8, "bg", 1, "C")
    elseif s == 1 and v == 9 then cardos.text("*", px + 11, py + 8, "bg", 1, "C")
    elseif s == 1 and v > 0 then cardos.text(tostring(v), px + 11, py + 8, "ok", 1, "C") end
  end end
  local msg = win and "CLEARED! [Ent]New" or (over and "BOOM! [Ent]New" or "[Ent]Dig [F]Flag [R]New")
  cardos.text(msg, 120, 124, win and "ok" or (over and "err" or "dim"), 1, "C")
end

