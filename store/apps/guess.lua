local target, inp, tries, hint, win = 50, "", 0, "Pick 1 to 100", false
function onEnter() math.randomseed(cardos.millis()); target = math.random(100); inp, tries, hint, win = "", 0, "Pick 1 to 100", false end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (win and k == "enter") or cl == "r" then onEnter(); return end
  if k == "back" and #inp > 0 then inp = inp:sub(1, #inp - 1); return end
  if k == "enter" and #inp > 0 then
    local v = tonumber(inp) or 0; inp = ""; tries = tries + 1
    if v == target then hint = "NAILED IT in " .. tries .. "!"; win = true
    elseif v < target then hint = v .. " is TOO LOW"
    else hint = v .. " is TOO HIGH" end
    return
  end
  if c and c:match("^%d$") and #inp < 3 then inp = inp .. c end
end
function onRender()
  cardos.clear(); cardos.statusBar("Number Guess  Tries:" .. tries)
  cardos.fillRect(14, 24, 212, 86, 6, "surface"); cardos.drawRect(14, 24, 212, 86, 6, win and "ok" or "border")
  cardos.text(hint, 120, 44, win and "ok" or "accent", 1, "C")
  cardos.fillRect(72, 62, 96, 24, 4, "surface2"); cardos.drawRect(72, 62, 96, 24, 4, "accent")
  cardos.text((inp ~= "" and inp or "?") .. "_", 120, 74, "text", 2, "C")
  cardos.text(win and "[Ent] New Game" or "Type number + [Enter]", 120, 98, "dim", 1, "C")
  cardos.text("[R] Reset   [ESC] Exit", 120, 122, "dim", 1, "C")
end

