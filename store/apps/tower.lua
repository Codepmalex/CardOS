local stack, cx, cw, dir, sc, over, pick, dif, lm = {}, 60, 76, 2.5, 0, false, true, 2, 0
local drop = nil
local CW = {88, 72, 54}
local SP = {2.0, 2.7, 3.6}
local DN = {"Easy", "Normal", "Hard"}
local function start(d)
  dif = d or dif; pick = false; cw = CW[dif]; dir = SP[dif]
  stack = {{x = 120 - math.floor(cw / 2), w = cw}}; cx, sc, over, lm, drop = 40, 0, false, cardos.millis(), nil
end
function onEnter() pick = true; over = false; sc = 0 end
function onTick()
  local now = cardos.millis(); if pick or now - lm < 25 then return end; lm = now
  if drop then
    drop.y = drop.y + drop.vy; drop.vy = drop.vy + 0.6
    if drop.y > 140 then drop = nil end
  end
  if over then return end
  cx = cx + dir
  if cx < 10 then cx = 10; dir = math.abs(dir) elseif cx + cw > 230 then cx = 230 - cw; dir = -math.abs(dir) end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if pick then if c == "1" then start(1) elseif c == "2" or k == "enter" then start(2) elseif c == "3" then start(3) end; return end
  if over then if k == "enter" or cl == "r" then start(dif) elseif cl == "d" then pick = true end; return end
  if c == " " or k == "enter" then
    local top = stack[#stack]
    local ix = math.floor(cx)
    local lx = math.max(top.x, ix)
    local rx = math.min(top.x + top.w, ix + cw)
    local nw = rx - lx
    local my = 126 - (#stack + 1) * 10
    if nw <= 2 then drop = {x = ix, y = my, w = cw, vy = 1}; over = true; return end
    if nw < cw then
      local dx = (ix < top.x) and ix or rx
      drop = {x = dx, y = my, w = cw - nw, vy = 1}
    end
    table.insert(stack, {x = lx, w = nw}); cw = nw; sc = sc + 1
    dir = (dir > 0 and 1 or -1) * math.min(6.4, SP[dif] + sc * (0.14 + dif * 0.04))
    if #stack > 9 then table.remove(stack, 1) end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Stack Tower [" .. DN[dif] .. "] H:" .. sc)
  if not pick then
    for i, b in ipairs(stack) do
      local y = 126 - i * 10
      cardos.fillRect(b.x, y, b.w, 9, 1, (i % 2 == 0) and "accent" or "accent2")
    end
    if drop then cardos.fillRect(math.floor(drop.x), math.floor(drop.y), drop.w, 9, 1, "err") end
    if not over then
      local my = 126 - (#stack + 1) * 10
      cardos.fillRect(math.floor(cx), my, cw, 9, 1, "warn")
    end
  end
  if pick then
    cardos.fillRect(30, 36, 180, 68, 5, "surface"); cardos.drawRect(30, 36, 180, 68, 5, "accent")
    cardos.text("STACK TOWER - DIFFICULTY", 120, 48, "accent", 1, "C")
    cardos.text("[1] Easy  [2] Normal  [3] Hard", 120, 68, "text", 1, "C")
    cardos.text("[Space / Enter] Drop Block", 120, 88, "dim", 1, "C")
  elseif over then
    cardos.fillRect(32, 46, 176, 46, 5, "surface"); cardos.drawRect(32, 46, 176, 46, 5, "err")
    cardos.text("TOWER FELL! H:" .. sc, 120, 58, "err", 1, "C"); cardos.text("[Ent] Retry [D] Diff [ESC] Exit", 120, 76, "dim", 1, "C")
  end
end
