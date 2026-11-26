local g, sc, over = {}, 0, false
local function spawn()
  local e = {}; for r = 1, 4 do for c = 1, 4 do if g[r][c] == 0 then table.insert(e, {r, c}) end end end
  if #e > 0 then local p = e[math.random(#e)]; g[p[1]][p[2]] = (math.random(10) == 1) and 4 or 2 end
end
local function slideLine(a)
  local b, moved = {}, false
  for i = 1, 4 do if a[i] ~= 0 then table.insert(b, a[i]) end end
  local res, i = {}, 1
  while i <= #b do
    if i < #b and b[i] == b[i + 1] then table.insert(res, b[i] * 2); sc = sc + b[i] * 2; i = i + 2
    else table.insert(res, b[i]); i = i + 1 end
  end
  while #res < 4 do table.insert(res, 0) end
  for j = 1, 4 do if res[j] ~= a[j] then moved = true end end
  return res, moved
end
local function checkOver()
  for r = 1, 4 do for c = 1, 4 do
    if g[r][c] == 0 then return false end
    if c < 4 and g[r][c] == g[r][c + 1] then return false end
    if r < 4 and g[r][c] == g[r + 1][c] then return false end
  end end
  return true
end
function onEnter() math.randomseed(cardos.millis()); g = {}; for r = 1, 4 do g[r] = {0, 0, 0, 0} end; sc, over = 0, false; spawn(); spawn() end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (over and k == "enter") or cl == "r" then onEnter(); return end
  local mv = false
  if k == "left" or c == "," or cl == "a" then
    for r = 1, 4 do local res, m = slideLine(g[r]); g[r] = res; mv = mv or m end
  elseif k == "right" or c == "/" or cl == "d" then
    for r = 1, 4 do local res, m = slideLine({g[r][4], g[r][3], g[r][2], g[r][1]}); g[r] = {res[4], res[3], res[2], res[1]}; mv = mv or m end
  elseif k == "up" or c == ";" or cl == "w" then
    for c2 = 1, 4 do local res, m = slideLine({g[1][c2], g[2][c2], g[3][c2], g[4][c2]}); for r = 1, 4 do g[r][c2] = res[r] end; mv = mv or m end
  elseif k == "down" or c == "." or cl == "s" then
    for c2 = 1, 4 do local res, m = slideLine({g[4][c2], g[3][c2], g[2][c2], g[1][c2]}); for r = 1, 4 do g[5 - r][c2] = res[r] end; mv = mv or m end
  end
  if mv then spawn(); over = checkOver() end
end
function onRender()
  cardos.clear(); cardos.statusBar("2048  Score:" .. sc)
  for r = 1, 4 do for c = 1, 4 do
    local x, y, v = 64 + (c - 1) * 28, 21 + (r - 1) * 26, g[r][c]
    local col = (v == 0) and "surface" or (v <= 4 and "surface2" or (v <= 32 and "accent" or (v <= 128 and "warn" or "ok")))
    cardos.fillRect(x, y, 26, 24, 3, col); cardos.drawRect(x, y, 26, 24, 3, "border")
    if v > 0 then cardos.text(tostring(v), x + 13, y + 12, (v >= 16) and "bg" or "text", 1, "C") end
  end end
  cardos.text("[WASD]Slide [R]New", 6, 124, "dim", 1, "L")
  if over then cardos.text("NO MOVES! [Ent]", 176, 124, "err", 1, "C") end
end

