local pl, dl, chips, st = {}, {}, 100, ""
local LABELS = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"}
local function drawCard() return math.random(13) end
local function scoreHand(h)
  local s, aces = 0, 0
  for _, v in ipairs(h) do if v == 1 then aces = aces + 1; s = s + 11 elseif v >= 10 then s = s + 10 else s = s + v end end
  while s > 21 and aces > 0 do s = s - 10; aces = aces - 1 end
  return s
end
local function handStr(h, hide)
  local o = {}; for i, v in ipairs(h) do table.insert(o, (hide and i == 2) and "?" or LABELS[v]) end
  return table.concat(o, " ")
end
local function deal()
  pl, dl = {drawCard(), drawCard()}, {drawCard(), drawCard()}
  if scoreHand(pl) == 21 then chips = chips + 15; st = "BLACKJACK! +15" else st = "PLAY" end
end
function onEnter() math.randomseed(cardos.millis()); chips = 100; deal() end
function onInput(k, c)
  local cl = (c or ""):lower()
  if k == "esc" or c == "`" then cardos.exit(); return end
  if st ~= "PLAY" and (k == "enter" or cl == "d" or c == " ") then deal(); return end
  if st ~= "PLAY" then return end
  if cl == "h" then
    table.insert(pl, drawCard())
    if scoreHand(pl) > 21 then chips = chips - 10; st = "BUST! -10" end
  elseif cl == "s" then
    while scoreHand(dl) < 17 do table.insert(dl, drawCard()) end
    local ps, ds = scoreHand(pl), scoreHand(dl)
    if ds > 21 or ps > ds then chips = chips + 10; st = "YOU WIN! +10"
    elseif ps == ds then st = "PUSH (Tie)"
    else chips = chips - 10; st = "DEALER WINS -10" end
  end
end
function onRender()
  cardos.clear(); cardos.statusBar("Blackjack  $" .. chips)
  local hide = (st == "PLAY")
  cardos.fillRect(10, 22, 220, 36, 4, "surface"); cardos.text("Dealer (" .. (hide and "?" or scoreHand(dl)) .. "):", 16, 26, "dim", 1, "L")
  cardos.text(handStr(dl, hide), 16, 40, "warn", 1, "L")
  cardos.fillRect(10, 62, 220, 36, 4, "surface"); cardos.text("You (" .. scoreHand(pl) .. "):", 16, 66, "dim", 1, "L")
  cardos.text(handStr(pl, false), 16, 80, "accent", 1, "L")
  if st == "PLAY" then cardos.text("[H] Hit   [S] Stand", 120, 114, "text", 1, "C")
  else cardos.text(st .. "  [Ent/D] Deal", 120, 114, (st:find("WIN") or st:find("BLACK")) and "ok" or "err", 1, "C") end
end
