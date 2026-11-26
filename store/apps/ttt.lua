local b, cur, ps, cs, st = {}, 5, 0, 0, ""
local W = {{1,2,3},{4,5,6},{7,8,9},{1,4,7},{2,5,8},{3,6,9},{1,5,9},{3,5,7}}
local function winner()
  for _, l in ipairs(W) do local v = b[l[1]]; if v ~= 0 and v == b[l[2]] and v == b[l[3]] then return v end end
  for i = 1, 9 do if b[i] == 0 then return 0 end end; return 3
end
local function cpuMove()
  for _, p in ipairs({2, 1}) do
    for i = 1, 9 do if b[i] == 0 then b[i] = p; local w = winner(); if w == p then b[i] = 2; return end; b[i] = 0 end end
  end
  if b[5] == 0 then b[5] = 2; return end
  local e = {}; for i = 1, 9 do if b[i] == 0 then table.insert(e, i) end end
  if #e > 0 then b[e[math.random(#e)]] = 2 end
end
local function resetB() b = {0,0,0,0,0,0,0,0,0}; st = "Your turn (X)" end
function onEnter() math.randomseed(cardos.millis()); ps, cs, cur = 0, 0, 5; resetB() end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if cl == "r" then resetB(); return end
  if st ~= "Your turn (X)" and (k == "enter" or c == " ") then resetB(); return end
  if k == "left" or c == "," or cl == "a" then if (cur - 1) % 3 > 0 then cur = cur - 1 end
  elseif k == "right" or c == "/" or cl == "d" then if (cur - 1) % 3 < 2 then cur = cur + 1 end
  elseif k == "up" or c == ";" or cl == "w" then if cur > 3 then cur = cur - 3 end
  elseif k == "down" or c == "." or cl == "s" then if cur < 7 then cur = cur + 3 end
  elseif (k == "enter" or c == " ") and st == "Your turn (X)" and b[cur] == 0 then
    b[cur] = 1; local w = winner()
    if w == 0 then cpuMove(); w = winner() end
    if w == 1 then ps = ps + 1; st = "YOU WIN! [Ent]" elseif w == 2 then cs = cs + 1; st = "CPU WINS! [Ent]" elseif w == 3 then st = "DRAW! [Ent]" end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Tic Tac Toe  " .. ps .. "-" .. cs)
  for i = 1, 9 do
    local r, c = math.floor((i - 1) / 3), (i - 1) % 3
    local x, y = 78 + c * 28, 22 + r * 28
    cardos.fillRect(x, y, 26, 26, 3, "surface"); cardos.drawRect(x, y, 26, 26, 3, i == cur and "accent" or "border")
    if b[i] == 1 then cardos.text("X", x + 13, y + 13, "accent", 2, "C")
    elseif b[i] == 2 then cardos.text("O", x + 13, y + 13, "err", 2, "C") end
  end
  cardos.text(st, 120, 114, "ok", 1, "C"); cardos.text("[WASD]Move [Ent]Place [R]Reset", 120, 125, "dim", 1, "C")
end

