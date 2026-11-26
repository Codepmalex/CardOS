local SYM = {"7","BAR","$","*","@","#"}
local r1, r2, r3, cr, spinEnd, msg = 1, 1, 1, 100, 0, "Press [Space] Spin!"
function onEnter() math.randomseed(cardos.millis()); r1, r2, r3, cr, spinEnd, msg = 1, 2, 3, 100, 0, "Press [Space] Spin!" end
function onTick()
  if spinEnd == 0 then return end
  local now = cardos.millis()
  if now < spinEnd then r1, r2, r3 = math.random(6), math.random(6), math.random(6)
  else
    spinEnd = 0; r1, r2, r3 = math.random(6), math.random(6), math.random(6)
    if r1 == r2 and r2 == r3 then local win = (r1 == 1) and 100 or 40; cr = cr + win; msg = "JACKPOT +" .. win .. "!"
    elseif r1 == r2 or r2 == r3 or r1 == r3 then cr = cr + 10; msg = "PAIR! +10"
    else msg = "No luck - try again!" end
  end
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (c == " " or k == "enter") and spinEnd == 0 then
    if cr < 5 then cr = 100 end
    cr = cr - 5; spinEnd = cardos.millis() + 750; msg = "Spinning..."
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Lucky Slots  $" .. cr)
  local vals = {r1, r2, r3}
  for i = 1, 3 do
    local x = 28 + (i - 1) * 64
    cardos.fillRect(x, 28, 56, 54, 5, "surface"); cardos.drawRect(x, 28, 56, 54, 5, spinEnd > 0 and "warn" or "accent")
    cardos.text(SYM[vals[i]], x + 28, 55, vals[i] == 1 and "err" or "warn", 2, "C")
  end
  cardos.text(msg, 120, 96, msg:find("JACK") and "ok" or "text", 1, "C")
  cardos.text("[Space/Ent] Spin ($5)   [ESC] Exit", 120, 120, "dim", 1, "C")
end

