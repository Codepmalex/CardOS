local WORDS = {"code","cardputer","wifi","screen","matrix","signal","pixel","flash","esp32","radio","laser","timer","speed","keyboard","battery","robot","cyber","github","kernel","script","arcade","rocket","planet","sensor"}
local target, inp, cnt, t0, wpm, done = "", "", 0, 0, 0, false
local function nextW() target = WORDS[math.random(#WORDS)]; inp = "" end
function onEnter() math.randomseed(cardos.millis()); cnt, t0, wpm, done = 0, 0, 0, false; nextW() end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if done and k == "enter" then onEnter(); return end
  if done then return end
  if k == "back" and #inp > 0 then inp = inp:sub(1, #inp - 1); return end
  if c and c:match("^[a-zA-Z0-9]$") then
    if t0 == 0 then t0 = cardos.millis() end
    inp = inp .. c:lower()
    if inp == target then
      cnt = cnt + 1
      if cnt >= 10 then
        local sec = math.max(1, (cardos.millis() - t0) / 1000)
        wpm = math.floor((cnt * 60) / sec); done = true
      else nextW() end
    end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Type Racer  " .. cnt .. "/10")
  cardos.fillRect(12, 24, 216, 86, 6, "surface"); cardos.drawRect(12, 24, 216, 86, 6, done and "ok" or "border")
  if done then
    cardos.text(wpm .. " WPM!", 120, 56, "ok", 2, "C"); cardos.text("[Enter] Race Again", 120, 82, "dim", 1, "C")
  else
    cardos.text(target, 120, 46, "accent", 2, "C")
    local ok = (target:sub(1, #inp) == inp)
    cardos.fillRect(42, 72, 156, 22, 4, "surface2"); cardos.drawRect(42, 72, 156, 22, 4, ok and "ok" or "err")
    cardos.text(inp .. "_", 120, 83, ok and "text" or "err", 1, "C")
  end
  cardos.text("Type the word above   [ESC] Exit", 120, 122, "dim", 1, "C")
end

