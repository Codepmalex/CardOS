local pd1, pd2, cd1, cd2, ps, cs, msg = 1, 1, 1, 1, 0, 0, "Press [Space/Ent] Roll!"
function onEnter() math.randomseed(cardos.millis()); pd1, pd2, cd1, cd2, ps, cs, msg = 1, 1, 1, 1, 0, 0, "Press [Space/Ent] Roll!" end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (c or ""):lower() == "r" then onEnter(); return end
  if c == " " or k == "enter" then
    pd1, pd2 = math.random(6), math.random(6)
    cd1, cd2 = math.random(6), math.random(6)
    local pt, ct = pd1 + pd2, cd1 + cd2
    if pt > ct then ps = ps + 1; msg = "YOU WIN (" .. pt .. " vs " .. ct .. ")"
    elseif ct > pt then cs = cs + 1; msg = "CPU WINS (" .. pt .. " vs " .. ct .. ")"
    else msg = "TIE (" .. pt .. " - " .. ct .. ")" end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Dice Duel  " .. ps .. " - " .. cs)
  cardos.text("YOU (" .. (pd1 + pd2) .. ")", 64, 24, "accent", 1, "C")
  cardos.fillRect(26, 36, 34, 34, 4, "surface"); cardos.drawRect(26, 36, 34, 34, 4, "accent"); cardos.text(tostring(pd1), 43, 53, "text", 2, "C")
  cardos.fillRect(68, 36, 34, 34, 4, "surface"); cardos.drawRect(68, 36, 34, 34, 4, "accent"); cardos.text(tostring(pd2), 85, 53, "text", 2, "C")
  cardos.text("CPU (" .. (cd1 + cd2) .. ")", 176, 24, "err", 1, "C")
  cardos.fillRect(138, 36, 34, 34, 4, "surface"); cardos.drawRect(138, 36, 34, 34, 4, "err"); cardos.text(tostring(cd1), 155, 53, "text", 2, "C")
  cardos.fillRect(180, 36, 34, 34, 4, "surface"); cardos.drawRect(180, 36, 34, 34, 4, "err"); cardos.text(tostring(cd2), 197, 53, "text", 2, "C")
  cardos.text(msg, 120, 92, "warn", 1, "C"); cardos.text("[Space/Ent] Roll   [R] Reset", 120, 122, "dim", 1, "C")
end

