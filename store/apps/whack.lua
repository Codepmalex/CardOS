local mole, sc, leftMs, startMs, lastPop, lm, over = 0, 0, 30000, 0, 0, 0, false
function onEnter() math.randomseed(cardos.millis()); sc, over = 0, false; startMs = cardos.millis(); lastPop = startMs; lm = startMs; mole = math.random(9) end
function onTick()
  if over then return end
  local now = cardos.millis(); local el = now - startMs
  leftMs = math.max(0, 30000 - el)
  if leftMs == 0 then over = true; mole = 0; return end
  if now - lastPop > math.max(420, 950 - sc * 15) then mole = math.random(9); lastPop = now end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if over then return end
  local n = tonumber(c)
  if n and n >= 1 and n <= 9 then
    if n == mole then sc = sc + 1; mole = math.random(9); lastPop = cardos.millis()
    else sc = math.max(0, sc - 1) end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Whack Mole  S:" .. sc .. "  " .. math.ceil(leftMs / 1000) .. "s")
  for i = 1, 9 do
    local r, c2 = math.floor((i - 1) / 3), (i - 1) % 3
    local x, y = 42 + c2 * 54, 24 + r * 31
    local on = (i == mole)
    cardos.fillRect(x, y, 48, 27, 4, on and "warn" or "surface"); cardos.drawRect(x, y, 48, 27, 4, on and "accent" or "border")
    cardos.text(on and "MOLE!" or tostring(i), x + 24, y + 13, on and "bg" or "dim", 1, "C")
  end
  cardos.text(over and ("TIME UP! Score:" .. sc .. " [Ent]") or "Press keys [1-9] to whack!", 120, 124, over and "ok" or "dim", 1, "C")
end

