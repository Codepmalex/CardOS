local seq, pos, st, lit, litAt, best = {}, 1, "SHOW", 0, 0, 0
local PAD = {
  {k="w", lbl="W", x=96,  y=24, col="ok"},
  {k="d", lbl="D", x=142, y=54, col="err"},
  {k="s", lbl="S", x=96,  y=84, col="warn"},
  {k="a", lbl="A", x=50,  y=54, col="accent"}
}
local function nextRound() table.insert(seq, math.random(4)); pos = 1; st = "SHOW"; lit = 0; litAt = cardos.millis() end
function onEnter() math.randomseed(cardos.millis()); seq, best = {}, 0; nextRound() end
function onTick()
  local now = cardos.millis()
  if st == "SHOW" then
    local el = now - litAt
    if lit == 0 and el > 260 then
      if pos > #seq then st = "PLAY"; pos = 1; lit = 0
      else lit = seq[pos]; litAt = now end
    elseif lit > 0 and el > 360 then lit = 0; pos = pos + 1; litAt = now end
  elseif st == "PLAY" and lit > 0 and now - litAt > 180 then lit = 0 end
end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if st == "OVER" and (k == "enter" or cl == "r") then onEnter(); return end
  if st ~= "PLAY" then return end
  local pick = 0
  if k == "up" or c == ";" or cl == "w" then pick = 1
  elseif k == "right" or c == "/" or cl == "d" then pick = 2
  elseif k == "down" or c == "." or cl == "s" then pick = 3
  elseif k == "left" or c == "," or cl == "a" then pick = 4 end
  if pick == 0 then return end
  lit = pick; litAt = cardos.millis()
  if pick == seq[pos] then
    pos = pos + 1
    if pos > #seq then if #seq > best then best = #seq end; nextRound() end
  else st = "OVER" end
end
function onRender()
  cardos.clear(); cardos.statusBar("Simon Says  Lv:" .. #seq .. " Best:" .. best)
  for i, p in ipairs(PAD) do
    local on = (lit == i)
    cardos.fillRect(p.x, p.y, 44, 28, 5, on and p.col or "surface"); cardos.drawRect(p.x, p.y, 44, 28, 5, p.col)
    cardos.text(p.lbl, p.x + 22, p.y + 14, on and "bg" or p.col, 1, "C")
  end
  local msg = (st == "SHOW") and "Watch..." or ((st == "PLAY") and "Repeat [W/A/S/D]!" or "WRONG! [Ent] Retry")
  cardos.text(msg, 120, 122, st == "OVER" and "err" or "text", 1, "C")
end

