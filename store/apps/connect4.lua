local W, H = 7, 5
local bd, colSel, st = {}, 4, "Your Turn (You=Cyan)"
local function drop(c, p)
  for r = H, 1, -1 do if bd[r][c] == 0 then bd[r][c] = p; return r end end; return 0
end
local function checkW(p)
  for r = 1, H do for c = 1, W do if bd[r][c] == p then
    for _, d in ipairs({{1,0},{0,1},{1,1},{1,-1}}) do
      local ok = true
      for k = 1, 3 do local nr, nc = r + d[2]*k, c + d[1]*k; if nr < 1 or nr > H or nc < 1 or nc > W or bd[nr][nc] ~= p then ok = false; break end end
      if ok then return true end
    end
  end end end
  return false
end
local function cpuMove()
  local opts = {}; for c = 1, W do if bd[1][c] == 0 then table.insert(opts, c) end end
  if #opts == 0 then return false end
  for _, p in ipairs({2, 1}) do
    for _, c in ipairs(opts) do local r = drop(c, p); if checkW(p) then bd[r][c] = 2; return true end; bd[r][c] = 0 end
  end
  drop(opts[math.random(#opts)], 2); return true
end
function onEnter() math.randomseed(cardos.millis()); bd = {}; for r = 1, H do bd[r] = {0,0,0,0,0,0,0} end; colSel, st = 4, "Your Turn" end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (st ~= "Your Turn" and k == "enter") or cl == "r" then onEnter(); return end
  if st ~= "Your Turn" then return end
  if k == "left" or c == "," or cl == "a" then colSel = math.max(1, colSel - 1)
  elseif k == "right" or c == "/" or cl == "d" then colSel = math.min(W, colSel + 1)
  elseif k == "enter" or c == " " then
    if drop(colSel, 1) > 0 then
      if checkW(1) then st = "YOU WIN! [Ent]"
      elseif not cpuMove() then st = "DRAW! [Ent]"
      elseif checkW(2) then st = "CPU WINS! [Ent]" end
    end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Connect 4")
  cardos.text("v", 38 + (colSel - 1) * 24 + 10, 22, "accent", 1, "C")
  cardos.fillRect(34, 30, 172, 86, 4, "surface"); cardos.drawRect(34, 30, 172, 86, 4, "border")
  for r = 1, H do for c = 1, W do
    local cx, cy, v = 38 + (c - 1) * 24 + 10, 34 + (r - 1) * 16 + 7, bd[r][c]
    cardos.circle(cx, cy, 6, v == 1 and "accent" or (v == 2 and "err" or "bg"), true)
  end end
  cardos.text(st, 120, 123, st:find("YOU") and "ok" or (st:find("CPU") and "err" or "dim"), 1, "C")
end

