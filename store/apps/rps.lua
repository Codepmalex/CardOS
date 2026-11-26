local CH = {"ROCK","PAPER","SCISSORS"}
local p, cp, ps, cs, msg = 0, 0, 0, 0, "Pick [1]Rock [2]Paper [3]Scis"
function onEnter() math.randomseed(cardos.millis()); p, cp, ps, cs, msg = 0, 0, 0, 0, "Pick [1]Rock [2]Paper [3]Scis" end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if cl == "n" then onEnter(); return end
  local pick = (c == "1" or cl == "r") and 1 or ((c == "2" or cl == "p") and 2 or ((c == "3" or cl == "s") and 3 or 0))
  if pick == 0 then return end
  p, cp = pick, math.random(3)
  if p == cp then msg = "DRAW!"
  elseif (p == 1 and cp == 3) or (p == 2 and cp == 1) or (p == 3 and cp == 2) then ps = ps + 1; msg = "YOU WIN!"
  else cs = cs + 1; msg = "CPU WINS!" end
end
function onRender()
  cardos.clear(); cardos.statusBar("Rock Paper  " .. ps .. " - " .. cs)
  cardos.fillRect(14, 24, 98, 58, 5, "surface"); cardos.drawRect(14, 24, 98, 58, 5, "accent")
  cardos.text("YOU", 63, 35, "dim", 1, "C"); cardos.text(p > 0 and CH[p] or "?", 63, 56, "accent", 1, "C")
  cardos.fillRect(128, 24, 98, 58, 5, "surface"); cardos.drawRect(128, 24, 98, 58, 5, "err")
  cardos.text("CPU", 177, 35, "dim", 1, "C"); cardos.text(cp > 0 and CH[cp] or "?", 177, 56, "err", 1, "C")
  cardos.text(msg, 120, 96, msg:find("WIN!") and (msg:find("YOU") and "ok" or "err") or "warn", 1, "C")
  cardos.text("[1/R]Rock [2/P]Paper [3/S]Scis [N]Reset", 120, 122, "dim", 1, "C")
end

