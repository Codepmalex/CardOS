local SAVE = "/byteclicker.txt"
local pts, perClick, perSec, cost1, cost2, lastSec = 0, 1, 0, 15, 50, 0
local function save() cardos.writeFile(SAVE, pts .. "|" .. perClick .. "|" .. perSec .. "|" .. cost1 .. "|" .. cost2) end
function onEnter()
  lastSec = cardos.millis()
  local raw = cardos.readFile(SAVE)
  if raw then
    local a, b, c, d, e = raw:match("(%d+)|(%d+)|(%d+)|(%d+)|(%d+)")
    if a then pts, perClick, perSec, cost1, cost2 = tonumber(a), tonumber(b), tonumber(c), tonumber(d), tonumber(e) end
  end
end
function onTick()
  local now = cardos.millis()
  if perSec > 0 and now - lastSec >= 1000 then lastSec = now; pts = pts + perSec end
end
function onInput(k, c)
  if k == "esc" or c == "`" then save(); cardos.exit(); return end
  if c == " " or k == "enter" then pts = pts + perClick
  elseif c == "1" and pts >= cost1 then pts = pts - cost1; perClick = perClick + 1; cost1 = math.floor(cost1 * 1.6); save()
  elseif c == "2" and pts >= cost2 then pts = pts - cost2; perSec = perSec + 2; cost2 = math.floor(cost2 * 1.7); save()
  elseif (c or ""):lower() == "r" then pts, perClick, perSec, cost1, cost2 = 0, 1, 0, 15, 50; save() end
end
function onRender()
  cardos.clear(); cardos.statusBar("Byte Clicker")
  cardos.fillRect(10, 22, 220, 44, 5, "surface"); cardos.drawRect(10, 22, 220, 44, 5, "accent")
  cardos.text(pts .. " Bytes", 120, 38, "accent", 2, "C")
  cardos.text("+" .. perClick .. "/click  |  +" .. perSec .. "/sec", 120, 56, "dim", 1, "C")
  cardos.fillRect(10, 72, 106, 36, 4, pts >= cost1 and "surface2" or "surface"); cardos.drawRect(10, 72, 106, 36, 4, pts >= cost1 and "ok" or "border")
  cardos.text("[1] +1 Click", 63, 83, "text", 1, "C"); cardos.text("Cost: " .. cost1, 63, 97, "warn", 1, "C")
  cardos.fillRect(124, 72, 106, 36, 4, pts >= cost2 and "surface2" or "surface"); cardos.drawRect(124, 72, 106, 36, 4, pts >= cost2 and "ok" or "border")
  cardos.text("[2] +2 / Sec", 177, 83, "text", 1, "C"); cardos.text("Cost: " .. cost2, 177, 97, "warn", 1, "C")
  cardos.text("[Space/Ent] Mine Bytes   [ESC] Save&Exit", 120, 122, "dim", 1, "C")
end

