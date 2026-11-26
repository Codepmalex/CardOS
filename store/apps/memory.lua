local SYM = {"A","B","C","D","E","F","G","H"}
local cards, rev, done, cur, f1, f2, hideAt, mv, win = {}, {}, {}, 1, 0, 0, 0, 0, false
function onEnter()
  math.randomseed(cardos.millis())
  cards, rev, done, cur, f1, f2, hideAt, mv, win = {}, {}, {}, 1, 0, 0, 0, 0, false
  for i = 1, 8 do table.insert(cards, SYM[i]); table.insert(cards, SYM[i]) end
  for i = 16, 2, -1 do local j = math.random(i); cards[i], cards[j] = cards[j], cards[i] end
  for i = 1, 16 do rev[i], done[i] = false, false end
end
function onTick()
  if hideAt > 0 and cardos.millis() >= hideAt then rev[f1], rev[f2], f1, f2, hideAt = false, false, 0, 0, 0 end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if (win and k == "enter") or cl == "r" then onEnter(); return end
  if k == "left" or c == "," or cl == "a" then if (cur - 1) % 4 > 0 then cur = cur - 1 end
  elseif k == "right" or c == "/" or cl == "d" then if (cur - 1) % 4 < 3 then cur = cur + 1 end
  elseif k == "up" or c == ";" or cl == "w" then if cur > 4 then cur = cur - 4 end
  elseif k == "down" or c == "." or cl == "s" then if cur < 13 then cur = cur + 4 end
  elseif (k == "enter" or c == " ") and hideAt == 0 and not rev[cur] and not done[cur] then
    rev[cur] = true
    if f1 == 0 then f1 = cur
    else
      f2 = cur; mv = mv + 1
      if cards[f1] == cards[f2] then
        done[f1], done[f2], f1, f2 = true, true, 0, 0
        local all = true; for i = 1, 16 do if not done[i] then all = false; break end end
        win = all
      else hideAt = cardos.millis() + 650 end
    end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Memory  Moves:" .. mv)
  for i = 1, 16 do
    local r, col = math.floor((i - 1) / 4), (i - 1) % 4
    local x, y = 48 + col * 36, 21 + r * 24
    local open = rev[i] or done[i]
    cardos.fillRect(x, y, 32, 21, 3, done[i] and "ok" or (open and "accent" or "surface"))
    cardos.drawRect(x, y, 32, 21, 3, i == cur and "warn" or "border")
    cardos.text(open and cards[i] or "?", x + 16, y + 10, open and "bg" or "dim", 1, "C")
  end
  cardos.text(win and ("MATCHED ALL in " .. mv .. "! [Ent]") or "[WASD]Move [Ent]Flip [R]New", 120, 123, win and "ok" or "dim", 1, "C")
end

