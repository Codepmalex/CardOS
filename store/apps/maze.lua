local W, H = 19, 11
local g, px, py, win, t0 = {}, 2, 2, false, 0
local function carve(x, y)
  local dirs = {{0,-2},{2,0},{0,2},{-2,0}}
  for i = 4, 2, -1 do local j = math.random(i); dirs[i], dirs[j] = dirs[j], dirs[i] end
  for _, d in ipairs(dirs) do
    local nx, ny = x + d[1], y + d[2]
    if nx >= 2 and nx < W and ny >= 2 and ny < H and g[ny][nx] == 1 then
      g[y + math.floor(d[2] / 2)][x + math.floor(d[1] / 2)] = 0; g[ny][nx] = 0; carve(nx, ny)
    end
  end
end
function onEnter()
  math.randomseed(cardos.millis()); g = {}
  for y = 1, H do g[y] = {}; for x = 1, W do g[y][x] = 1 end end
  g[2][2] = 0; carve(2, 2); px, py, win, t0 = 2, 2, false, cardos.millis()
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (win and k == "enter") or cl == "r" then onEnter(); return end
  local nx, ny = px, py
  if k == "left" or c == "," or cl == "a" then nx = px - 1
  elseif k == "right" or c == "/" or cl == "d" then nx = px + 1
  elseif k == "up" or c == ";" or cl == "w" then ny = py - 1
  elseif k == "down" or c == "." or cl == "s" then ny = py + 1 end
  if g[ny] and g[ny][nx] == 0 then px, py = nx, ny; if px == W - 1 and py == H - 1 then win = true end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Maze Runner")
  for y = 1, H do for x = 1, W do
    if g[y][x] == 1 then cardos.fillRect(25 + (x - 1) * 10, 19 + (y - 1) * 10, 9, 9, 1, "surface2") end
  end end
  cardos.fillRect(25 + (W - 2) * 10, 19 + (H - 2) * 10, 9, 9, 2, "ok")
  cardos.fillRect(25 + (px - 1) * 10, 19 + (py - 1) * 10, 9, 9, 2, "accent")
  if win then
    cardos.fillRect(42, 46, 156, 40, 5, "surface"); cardos.drawRect(42, 46, 156, 40, 5, "ok")
    cardos.text("ESCAPED! [Ent] New", 120, 66, "ok", 1, "C")
  end
end

