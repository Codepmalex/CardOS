local qStr, ans, inp, sc, startMs, leftMs, over = "", 0, "", 0, 0, 30000, false
local function nextQ()
  local op = math.random(3); local a, b = math.random(2, 19), math.random(2, 15)
  if op == 1 then qStr = a .. " + " .. b .. " = ?"; ans = a + b
  elseif op == 2 then if a < b then a, b = b, a end; qStr = a .. " - " .. b .. " = ?"; ans = a - b
  else a = math.random(2, 12); b = math.random(2, 12); qStr = a .. " x " .. b .. " = ?"; ans = a * b end
  inp = ""
end
function onEnter() math.randomseed(cardos.millis()); sc, over = 0, false; startMs = cardos.millis(); leftMs = 30000; nextQ() end
function onTick()
  if over then return end
  leftMs = math.max(0, 30000 - (cardos.millis() - startMs))
  if leftMs == 0 then over = true end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if over and (k == "enter" or cl == "r") then onEnter(); return end
  if over then return end
  if k == "back" and #inp > 0 then inp = inp:sub(1, #inp - 1); return end
  if k == "enter" and #inp > 0 then if tonumber(inp) == ans then sc = sc + 1 end; nextQ(); return end
  if c and c:match("^%d$") and #inp < 4 then inp = inp .. c end
end
function onRender()
  cardos.clear(); cardos.statusBar("Math Dash  S:" .. sc .. "  " .. math.ceil(leftMs / 1000) .. "s")
  cardos.fillRect(14, 24, 212, 86, 6, "surface"); cardos.drawRect(14, 24, 212, 86, 6, over and "ok" or "border")
  if over then
    cardos.text("TIME UP! Solved: " .. sc, 120, 56, "ok", 1, "C"); cardos.text("[Ent] Play Again", 120, 78, "dim", 1, "C")
  else
    cardos.text(qStr, 120, 44, "accent", 2, "C")
    cardos.fillRect(72, 68, 96, 24, 4, "surface2"); cardos.drawRect(72, 68, 96, 24, 4, "accent")
    cardos.text((inp ~= "" and inp or "?") .. "_", 120, 80, "text", 2, "C")
  end
  cardos.text("Type answer + [Enter]   [ESC] Exit", 120, 122, "dim", 1, "C")
end

