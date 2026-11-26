local st, goAt, res, best = "WAIT", 0, 0, 9999
local function startRound() st = "WAIT"; goAt = cardos.millis() + math.random(1400, 3800); res = 0 end
function onEnter() math.randomseed(cardos.millis()); best = 9999; startRound() end
function onTick()
  if st == "WAIT" and cardos.millis() >= goAt then st = "GO" end
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if k == "enter" or c == " " then
    if st == "WAIT" then st = "EARLY"
    elseif st == "GO" then res = cardos.millis() - goAt; if res < best then best = res end; st = "DONE"
    else startRound() end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Reflex Test" .. (best < 9999 and ("  Best:" .. best .. "ms") or ""))
  local col = (st == "GO") and "ok" or ((st == "EARLY") and "err" or "surface")
  cardos.fillRect(16, 26, 208, 84, 7, col); cardos.drawRect(16, 26, 208, 84, 7, st == "GO" and "ok" or "border")
  if st == "WAIT" then cardos.text("WAIT FOR GREEN...", 120, 68, "warn", 1, "C")
  elseif st == "GO" then cardos.text("PRESS NOW!", 120, 68, "bg", 2, "C")
  elseif st == "EARLY" then cardos.text("TOO EARLY!", 120, 60, "bg", 1, "C"); cardos.text("[Space/Ent] Retry", 120, 78, "bg", 1, "C")
  else cardos.text(res .. " ms", 120, 58, "accent", 2, "C"); cardos.text("[Space/Ent] Again", 120, 82, "dim", 1, "C") end
  cardos.text("[Space/Enter] Hit   [ESC] Exit", 120, 122, "dim", 1, "C")
end

