local stack, cx, cw, dir, sc, over, lm = {}, 60, 76, 2.5, 0, false, 0
function onEnter() stack = {{x=82, w=76}}; cx, cw, dir, sc, over, lm = 40, 76, 2.5, 0, false, cardos.millis() end
function onTick()
  local now = cardos.millis(); if over or now - lm < 25 then return end; lm = now
  cx = cx + dir
  if cx < 10 then cx = 10; dir = math.abs(dir) elseif cx + cw > 230 then cx = 230 - cw; dir = -math.abs(dir) end
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or (c or ""):lower() == "r") then onEnter(); return end
  if not over and (c == " " or k == "enter") then
    local top = stack[#stack]
    local lx = math.max(top.x, math.floor(cx))
    local rx = math.min(top.x + top.w, math.floor(cx) + cw)
    local nw = rx - lx
    if nw <= 2 then over = true; return end
    table.insert(stack, {x=lx, w=nw}); cw = nw; sc = sc + 1
    dir = (dir > 0 and 1 or -1) * math.min(5.5, 2.4 + sc * 0.2)
    if #stack > 9 then table.remove(stack, 1) end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Stack Tower  H:" .. sc)
  for i, b in ipairs(stack) do
    local y = 126 - i * 10
    cardos.fillRect(b.x, y, b.w, 9, 1, (i % 2 == 0) and "accent" or "accent2")
  end
  if not over then
    local my = 126 - (#stack + 1) * 10
    cardos.fillRect(math.floor(cx), my, cw, 9, 1, "warn")
  else
    cardos.fillRect(42, 46, 156, 42, 5, "surface"); cardos.drawRect(42, 46, 156, 42, 5, "err")
    cardos.text("TOWER FELL! H:" .. sc, 120, 60, "err", 1, "C"); cardos.text("[Ent] Retry [ESC] Exit", 120, 74, "dim", 1, "C")
  end
end

