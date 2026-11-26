local WORDS = {"CARDPUTER","ESP32","WIRELESS","INFRARED","BATTERY","MATRIX","PIXEL","GITHUB","PYTHON","ROBOT","SIGNAL","ROCKET","PLANET","CYBER","GALAXY","SCREEN","CODING","PUZZLE","KEYBOARD","AUDIO"}
local word, guessed, wrong, maxW, st = "", {}, 0, 6, "PLAY"
local function newGame() word = WORDS[math.random(#WORDS)]; guessed, wrong, st = {}, 0, "PLAY" end
function onEnter() math.randomseed(cardos.millis()); newGame() end
local function masked()
  local out, done = {}, true
  for i = 1, #word do
    local ch = word:sub(i, i)
    if guessed[ch] then table.insert(out, ch) else table.insert(out, "_"); done = false end
  end
  return table.concat(out, " "), done
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if st ~= "PLAY" and (k == "enter" or c == " ") then newGame(); return end
  if st ~= "PLAY" or not c then return end
  local ch = c:upper()
  if not ch:match("^[A-Z]$") or guessed[ch] then return end
  guessed[ch] = true
  if not word:find(ch, 1, true) then
    wrong = wrong + 1
    if wrong >= maxW then st = "LOST" end
  else
    local _, done = masked(); if done then st = "WIN" end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Hangman  Miss:" .. wrong .. "/" .. maxW)
  cardos.fillRect(10, 22, 220, 84, 5, "surface"); cardos.drawRect(10, 22, 220, 84, 5, st == "WIN" and "ok" or (st == "LOST" and "err" or "border"))
  local m = masked()
  cardos.text(m, 120, 48, "accent", 1, "C")
  local used = {}; for k, _ in pairs(guessed) do table.insert(used, k) end; table.sort(used)
  cardos.text("Used: " .. table.concat(used, ""), 120, 72, "dim", 1, "C")
  if st == "WIN" then cardos.text("YOU WON! [Enter] Next", 120, 92, "ok", 1, "C")
  elseif st == "LOST" then cardos.text("Word: " .. word .. " [Ent]", 120, 92, "err", 1, "C")
  else cardos.text("Type A-Z to guess", 120, 92, "text", 1, "C") end
  cardos.text("[ESC] Exit", 120, 122, "dim", 1, "C")
end

