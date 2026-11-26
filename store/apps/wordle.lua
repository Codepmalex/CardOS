local W5 = {"APPLE","BRAIN","CHAIR","DANCE","EAGLE","FLAME","GRAPE","HOUSE","INPUT","JUICE","KNIFE","LASER","MAGIC","NOBLE","OCEAN","PIANO","QUEST","RADIO","SMART","TIGER","ULTRA","VIDEO","WATCH","XENON","YOUTH","ZEBRA"}
local target, rows, cur, st = "", {}, "", "PLAY"
local function newGame() target = W5[math.random(#W5)]; rows, cur, st = {}, "", "PLAY" end
function onEnter() math.randomseed(cardos.millis()); newGame() end
local function evalGuess(g)
  local res = {}
  for i = 1, 5 do
    local ch = g:sub(i, i)
    if ch == target:sub(i, i) then res[i] = "ok"
    elseif target:find(ch, 1, true) then res[i] = "warn"
    else res[i] = "surface2" end
  end
  return res
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit(); return end
  if st ~= "PLAY" and k == "enter" then newGame(); return end
  if st ~= "PLAY" then return end
  if k == "back" and #cur > 0 then cur = cur:sub(1, #cur - 1); return end
  if k == "enter" and #cur == 5 then
    table.insert(rows, {w=cur, r=evalGuess(cur)})
    if cur == target then st = "WIN" elseif #rows >= 5 then st = "LOST" end
    cur = ""; return
  end
  if c and c:match("^[a-zA-Z]$") and #cur < 5 then cur = cur .. c:upper() end
end
function onRender()
  cardos.clear(); cardos.statusBar("Word5")
  for row = 1, 5 do
    local g = rows[row]
    for col = 1, 5 do
      local x, y = 65 + (col - 1) * 22, 19 + (row - 1) * 20
      local ch, bg = "", "surface"
      if g then ch = g.w:sub(col, col); bg = g.r[col]
      elseif row == #rows + 1 and col <= #cur then ch = cur:sub(col, col); bg = "surface2" end
      cardos.fillRect(x, y, 20, 18, 2, bg); cardos.drawRect(x, y, 20, 18, 2, "border")
      if ch ~= "" then cardos.text(ch, x + 10, y + 9, (bg == "ok" or bg == "warn") and "bg" or "text", 1, "C") end
    end
  end
  local msg = (st == "WIN") and "SOLVED! [Ent]" or ((st == "LOST") and ("Word:" .. target .. " [Ent]") or "Type 5 letters + [Ent]")
  cardos.text(msg, 120, 124, st == "WIN" and "ok" or (st == "LOST" and "err" or "dim"), 1, "C")
end

