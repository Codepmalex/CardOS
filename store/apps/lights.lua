local W, H = 5, 4
local g, cx, cy, mv, win = {}, 3, 2, 0, false
local function press(x, y)
  local dirs = {{0,0},{1,0},{-1,0},{0,1},{0,-1}}
  for _, d in ipairs(dirs) do
    local nx, ny = x + d[1], y + d[2]
    if nx >= 1 and nx <= W and ny >= 1 and ny <= H then g[ny][nx] = not g[ny][nx] end
  end
end
function onEnter()
  math.randomseed(cardos.millis()); g, cx, cy, mv, win = {}, 3, 2, 0, false
  for y = 1, H do g[y] = {}; for x = 1, W do g[y][x] = false end end
  for _ = 1, 7 do press(math.random(W), math.random(H)) end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (win and k == "enter") or cl == "r" then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then cx = math.max(1, cx - 1)
  elseif k == "right" or c == "/" or cl == "d" then cx = math.min(W, cx + 1)
  elseif k == "up" or c == ";" or cl == "w" then cy = math.max(1, cy - 1)
  elseif k == "down" or c == "." or cl == "s" then cy = math.min(H, cy + 1)
  elseif k == "enter" or c == " " then
    press(cx, cy); mv = mv + 1; win = true
    for y = 1, H do for x = 1, W do if g[y][x] then win = false end end end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Lights Out  Moves:" .. mv)
  for y = 1, H do for x = 1, W do
    local px, py = 46 + (x - 1) * 30, 22 + (y - 1) * 23
    cardos.fillRect(px, py, 26, 20, 3, g[y][x] and "warn" or "surface")
    cardos.drawRect(px, py, 26, 20, 3, (x == cx and y == cy) and "accent" or "border")
  end end
  cardos.text(win and ("SOLVED in " .. mv .. "! [Ent]") or "[WASD]Move [Ent]Toggle [R]New", 120, 123, win and "ok" or "dim", 1, "C")
end

