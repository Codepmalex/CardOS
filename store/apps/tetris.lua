local W, H, SZ = 10, 14, 8
local grid, px, py, pc, pr, npc, sc, over, pick, dif, lm, hm, fl = {}, 4, 1, 1, 1, 2, 0, false, true, 2, 0, 0, 0
local DROP = {480, 330, 200}
local DN = {"Easy", "Normal", "Hard"}
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
local function spawn() pc = npc; npc = math.random(#P); pr = 1; px = 4; py = 1; if not fits(px, py, pr) then over = true end end
local function lockPiece()
  local sh = P[pc][pr]
  for i = 1, 4 do local x, y = px + sh[i][1], py + sh[i][2]; if y >= 1 and y <= H then grid[y][x] = pc end end
  for r = H, 1, -1 do
    local full = true
    for c = 1, W do if grid[r][c] == 0 then full = false; break end end
    if full then
      sc = sc + 100 * dif; fl = 6
      for rr = r, 2, -1 do grid[rr] = grid[rr - 1] end
      grid[1] = {}; for c = 1, W do grid[1][c] = 0 end
    end
  end
  spawn()
end
local function start(d)
  dif = d or dif; pick = false; math.randomseed(cardos.millis())
  grid = {}; for r = 1, H do grid[r] = {}; for c = 1, W do grid[r][c] = 0 end end
  npc = math.random(#P); sc, over, fl, lm, hm = 0, false, 0, cardos.millis(), cardos.millis(); spawn()
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or over then return end
  if fl > 0 then fl = fl - 1 end
  if now - hm >= 115 then
    hm = now
    if cardos.isKeyDown("left") and fits(px - 1, py, pr) then px = px - 1 end
    if cardos.isKeyDown("right") and fits(px + 1, py, pr) then px = px + 1 end
    if cardos.isKeyDown("down") then if fits(px, py + 1, pr) then py = py + 1 else lockPiece(); lm = now end end
  end
  if now - lm >= math.max(90, DROP[dif] - math.floor(sc / 40)) then
    lm = now; if fits(px, py + 1, pr) then py = py + 1 else lockPiece() end
  end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then
    if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end
    return
  end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end; return end
  if k == "up" or c == ";" or cl == "w" or c == " " then local nr = (pr % #P[pc]) + 1; if fits(px, py, nr) then pr = nr end end
end
function onRender()
  cardos.clear(); cardos.statusBar("Block Drop [" .. DN[dif] .. "]")
  local ox, oy = 60, 19
  cardos.fillRect(ox - 2, oy - 1, W * SZ + 4, H * SZ + 2, 2, fl > 0 and "surface2" or "surface")
  cardos.drawRect(ox - 2, oy - 1, W * SZ + 4, H * SZ + 2, 2, fl > 0 and "warn" or "border")
  if not pick then
    for r = 1, H do for c = 1, W do if grid[r][c] ~= 0 then cardos.fillRect(ox + (c - 1) * SZ, oy + (r - 1) * SZ, SZ - 1, SZ - 1, 1, COLS[grid[r][c]]) end end end
    local sh = P[pc][pr]; for i = 1, 4 do local x, y = px + sh[i][1], py + sh[i][2]; if y >= 1 then cardos.fillRect(ox + (x - 1) * SZ, oy + (y - 1) * SZ, SZ - 1, SZ - 1, 1, COLS[pc]) end end
    cardos.text("Next", 156, 22, "dim", 1, "L")
    for i = 1, 4 do local b = P[npc][1][i]; cardos.fillRect(160 + b[1] * 6, 33 + b[2] * 6, 5, 5, 1, COLS[npc]) end
    cardos.text("Score", 156, 62, "dim", 1, "L"); cardos.text(tostring(sc), 156, 74, "accent", 1, "L")
    cardos.text("[W]Rot [A/D/S]", 156, 98, "dim", 1, "L")
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("BLOCK DROP - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("Hold A/D/S to slide & drop", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("GAME OVER S:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Ent] Again [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
