local W, H, SZ = 10, 14, 8
local grid, px, py, pc, pr, sc, over, lm = {}, 4, 1, 1, 1, 0, false, 0
local P = {
  {{{0,0},{1,0},{0,1},{1,1}}},
  {{{0,0},{1,0},{2,0},{3,0}}, {{1,-1},{1,0},{1,1},{1,2}}},
  {{{0,0},{1,0},{2,0},{1,1}}, {{1,-1},{1,0},{1,1},{2,0}}, {{0,1},{1,1},{2,1},{1,0}}, {{1,-1},{1,0},{1,1},{0,0}}},
  {{{0,0},{1,0},{1,1},{2,1}}, {{1,0},{1,1},{0,1},{0,2}}},
  {{{0,0},{1,0},{2,0},{0,1}}, {{0,0},{1,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,0}}, {{0,0},{0,1},{0,2},{1,2}}}
}
local COLS = {"warn","accent","accent2","ok","err"}
local function fits(nx, ny, nr)
  local sh = P[pc][nr]
  for i = 1, 4 do
    local x, y = nx + sh[i][1], ny + sh[i][2]
    if x < 1 or x > W or y > H then return false end
    if y >= 1 and grid[y][x] ~= 0 then return false end
  end
  return true
end
local function spawn() pc = math.random(#P); pr = 1; px = 4; py = 1; if not fits(px, py, pr) then over = true end end
local function lockPiece()
  local sh = P[pc][pr]
  for i = 1, 4 do local x, y = px + sh[i][1], py + sh[i][2]; if y >= 1 and y <= H then grid[y][x] = pc end end
  for r = H, 1, -1 do
    local full = true
    for c = 1, W do if grid[r][c] == 0 then full = false; break end end
    if full then
      sc = sc + 100
      for rr = r, 2, -1 do grid[rr] = grid[rr - 1] end
      grid[1] = {}; for c = 1, W do grid[1][c] = 0 end
    end
  end
  spawn()
end
function onEnter() math.randomseed(cardos.millis()); grid = {}; for r = 1, H do grid[r] = {}; for c = 1, W do grid[r][c] = 0 end end; sc, over, lm = 0, false, cardos.millis(); spawn() end
function onTick()
  local now = cardos.millis(); if over or now - lm < 380 then return end; lm = now
  if fits(px, py + 1, pr) then py = py + 1 else lockPiece() end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then if fits(px - 1, py, pr) then px = px - 1 end
  elseif k == "right" or c == "/" or cl == "d" then if fits(px + 1, py, pr) then px = px + 1 end
  elseif k == "down" or c == "." or cl == "s" then if fits(px, py + 1, pr) then py = py + 1 else lockPiece() end
  elseif k == "up" or c == ";" or cl == "w" or c == " " then local nr = (pr % #P[pc]) + 1; if fits(px, py, nr) then pr = nr end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Block Drop")
  local ox, oy = 60, 19
  cardos.fillRect(ox - 2, oy - 1, W * SZ + 4, H * SZ + 2, 2, "surface"); cardos.drawRect(ox - 2, oy - 1, W * SZ + 4, H * SZ + 2, 2, "border")
  for r = 1, H do for c = 1, W do if grid[r][c] ~= 0 then cardos.fillRect(ox + (c - 1) * SZ, oy + (r - 1) * SZ, SZ - 1, SZ - 1, 1, COLS[grid[r][c]]) end end end
  local sh = P[pc][pr]; for i = 1, 4 do local x, y = px + sh[i][1], py + sh[i][2]; if y >= 1 then cardos.fillRect(ox + (x - 1) * SZ, oy + (y - 1) * SZ, SZ - 1, SZ - 1, 1, COLS[pc]) end end
  cardos.text("Score", 156, 26, "dim", 1, "L"); cardos.text(tostring(sc), 156, 38, "accent", 2, "L")
  cardos.text("[W/Up]Rot", 156, 70, "dim", 1, "L"); cardos.text("[A/D]Move", 156, 82, "dim", 1, "L"); cardos.text("[S]Drop", 156, 94, "dim", 1, "L")
  if over then
    cardos.fillRect(36, 48, 168, 42, 5, "surface"); cardos.drawRect(36, 48, 168, 42, 5, "err")
    cardos.text("GAME OVER S:" .. sc, 120, 62, "err", 1, "C"); cardos.text("[Ent] Again  [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end

