-- IR TV & Projector Universal Remote (GitHub Store Lua App for CardOS)
-- Database stays on SD (/store/irtv_db.tsv) and is streamed 1 line at a time.
local DB_FILE = "/store/irtv_db.tsv"
local SAVED_FILE = "/irtv_saved.tsv"

local dbCount = 0
local mode = "SAVED"
local saved = {}
local savedSel, savedScroll = 1, 1
local brands = {}
local brandSel, brandScroll = 1, 1
local candidates = {}
local bruteIdx = 1
local bruteAuto = true
local bruteUseMute = false
local lastBruteMs = 0
local bruteDev = nil
local confirmDev = nil
local activeIdx = 0
local renameInput = ""
local renameTarget = 0
local lastFlashMs = 0
local lastCmdLabel = ""

local function trim(s)
  return (s:gsub("^%s*(.-)%s*$", "%1"))
end

local function trunc(s, n)
  if #s > n then return string.sub(s, 1, n - 1) .. "~" end
  return s
end

local function splitFields(line)
  local parts = {}
  local start = 1
  while true do
    local p = string.find(line, "|", start, true)
    if not p then
      table.insert(parts, string.sub(line, start))
      break
    end
    table.insert(parts, string.sub(line, start, p - 1))
    start = p + 1
  end
  return parts
end

local function parseProfile(str)
  if not str or str == "" then return nil end
  local f = splitFields(str)
  return {
    b = f[1] or "", m = f[2] or "", t = f[3] or "TV",
    p = f[4] or "NEC", bits = tonumber(f[5]) or 32,
    pwr = f[6] or "", vu = f[7] or "", vd = f[8] or "", mt = f[9] or "",
    cu = f[10] or "", cd = f[11] or "", in_ = f[12] or "", mn = f[13] or "",
    ok = f[14] or "", up = f[15] or "", dn = f[16] or "", lt = f[17] or "",
    rt = f[18] or "", bk = f[19] or ""
  }
end

local function getBrandType(str)
  if not str then return "", "TV" end
  local b, _, t = string.match(str, "^([^|]+)|([^|]+)|([^|]+)|")
  return b or "", t or "TV"
end

local function loadSaved()
  saved = {}
  local raw = cardos.readFile(SAVED_FILE)
  if not raw or raw == "" then return end
  for line in string.gmatch(raw, "[^\r\n]+") do
    local f = splitFields(line)
    if #f >= 6 and f[1] ~= "" then
      table.insert(saved, {
        name = f[1], b = f[2] or "", m = f[3] or "", t = f[4] or "TV",
        p = f[5] or "NEC", bits = tonumber(f[6]) or 32,
        pwr = f[7] or "", vu = f[8] or "", vd = f[9] or "", mt = f[10] or "",
        cu = f[11] or "", cd = f[12] or "", in_ = f[13] or "", mn = f[14] or "",
        ok = f[15] or "", up = f[16] or "", dn = f[17] or "", lt = f[18] or "",
        rt = f[19] or "", bk = f[20] or ""
      })
    end
  end
end

local function saveSaved()
  local lines = {}
  for _, d in ipairs(saved) do
    local nm = (d.name or "TV"):gsub("[|\r\n]", " ")
    local row = table.concat({
      nm, d.b or "", d.m or "", d.t or "TV", d.p or "NEC", tostring(d.bits or 32),
      d.pwr or "", d.vu or "", d.vd or "", d.mt or "",
      d.cu or "", d.cd or "", d.in_ or "", d.mn or "",
      d.ok or "", d.up or "", d.dn or "", d.lt or "", d.rt or "", d.bk or ""
    }, "|")
    table.insert(lines, row)
  end
  cardos.writeFile(SAVED_FILE, table.concat(lines, "\n") .. "\n")
end

local function makeUniqueName(base)
  local function used(nm)
    local low = string.lower(nm)
    for _, d in ipairs(saved) do
      if string.lower(d.name) == low then return true end
    end
    return false
  end
  if not used(base) then return base end
  for n = 2, 50 do
    local cand = base .. " " .. tostring(n)
    if not used(cand) then return cand end
  end
  return base
end

local function sendIr(dev, code, label)
  if not dev or not code or code == "" then
    cardos.toast("Key N/A for model", 900)
    return
  end
  cardos.irSend(dev.p, code, dev.bits)
  lastFlashMs = cardos.millis()
  lastCmdLabel = label
end

local function fireBruteCurrent()
  if #candidates == 0 then return end
  bruteDev = parseProfile(cardos.readLine(DB_FILE, candidates[bruteIdx]))
  if not bruteDev then return end
  local useMute = bruteUseMute and (bruteDev.mt ~= nil and bruteDev.mt ~= "")
  local code = useMute and bruteDev.mt or bruteDev.pwr
  sendIr(bruteDev, code, useMute and "MUTE" or "PWR")
  lastBruteMs = cardos.millis()
end

local function buildBrands()
  brands = {}
  if dbCount == 0 then
    cardos.toast("Re-download IR TV in Store", 1800)
    return false
  end
  local tvCnt, projCnt = 0, 0
  local seen = {}
  local order = {}
  for i = 1, dbCount do
    local s = cardos.readLine(DB_FILE, i)
    if s then
      local b, t = getBrandType(s)
      if t == "Proj" then projCnt = projCnt + 1 else tvCnt = tvCnt + 1 end
      if not seen[b] then
        seen[b] = {label=b, kind="BRAND", match=b, count=1}
        table.insert(order, seen[b])
      else
        seen[b].count = seen[b].count + 1
      end
    end
  end
  table.insert(brands, {label="* Brute-Force ALL", kind="ALL", match="", count=dbCount})
  if tvCnt > 0 then table.insert(brands, {label="* All TVs", kind="TV", match="", count=tvCnt}) end
  if projCnt > 0 then table.insert(brands, {label="* All Projectors", kind="PROJ", match="", count=projCnt}) end
  for _, item in ipairs(order) do table.insert(brands, item) end
  brandSel, brandScroll = 1, 1
  return true
end

local function isUp(k, c)    return k == "up" or (k == "char" and c == ";") end
local function isDown(k, c)  return k == "down" or (k == "char" and c == ".") end
local function isLeft(k, c)  return k == "left" or (k == "char" and c == ",") end
local function isRight(k, c) return k == "right" or (k == "char" and c == "/") end
local function isBack(k, c)  return k == "esc" or (k == "char" and c == "`") end

function onEnter()
  dbCount = cardos.countLines(DB_FILE)
  loadSaved()
  if savedSel > #saved then savedSel = math.max(1, #saved) end
  if savedScroll > math.max(1, #saved - 2) then savedScroll = math.max(1, #saved - 2) end
  lastFlashMs = 0
  lastCmdLabel = ""
  mode = "SAVED"
end

function onTick()
  if mode == "BRUTE" and bruteAuto and #candidates > 0 then
    if cardos.millis() - lastBruteMs >= 1100 then
      bruteIdx = (bruteIdx % #candidates) + 1
      fireBruteCurrent()
    end
  end
end

function onInput(key, ch)
  local cl = string.lower(ch or "")

  if mode == "SAVED" then
    if isBack(key, ch) then cardos.exit(); return end
    if isUp(key, ch) and savedSel > 1 then
      savedSel = savedSel - 1
      if savedSel < savedScroll then savedScroll = savedSel end
    elseif isDown(key, ch) and savedSel < #saved then
      savedSel = savedSel + 1
      if savedSel >= savedScroll + 3 then savedScroll = savedSel - 2 end
    elseif cl == "a" then
      if buildBrands() then mode = "PICK_BRAND" end
    elseif key == "enter" then
      if #saved == 0 then
        if buildBrands() then mode = "PICK_BRAND" end
      else
        activeIdx = savedSel
        mode = "REMOTE"
      end
    elseif cl == "r" and #saved > 0 then
      renameTarget = savedSel
      renameInput = saved[savedSel].name
      mode = "RENAME"
    elseif (key == "del" or cl == "d") and #saved > 0 then
      local rm = saved[savedSel].name
      table.remove(saved, savedSel)
      if savedSel > #saved then savedSel = math.max(1, #saved) end
      if savedScroll > math.max(1, #saved - 2) then savedScroll = math.max(1, #saved - 2) end
      saveSaved()
      cardos.toast("Removed " .. rm, 1200)
    elseif cl == "p" and #saved > 0 then
      sendIr(saved[savedSel], saved[savedSel].pwr, "PWR")
    end

  elseif mode == "PICK_BRAND" then
    if isBack(key, ch) then mode = "SAVED"; return end
    if isUp(key, ch) and brandSel > 1 then
      brandSel = brandSel - 1
      if brandSel < brandScroll then brandScroll = brandSel end
    elseif isDown(key, ch) and brandSel < #brands then
      brandSel = brandSel + 1
      if brandSel >= brandScroll + 4 then brandScroll = brandSel - 3 end
    elseif key == "enter" and #brands > 0 then
      local bf = brands[brandSel]
      candidates = {}
      for i = 1, dbCount do
        local s = cardos.readLine(DB_FILE, i)
        if s then
          local b, t = getBrandType(s)
          local ok = (bf.kind == "ALL") or
                     (bf.kind == "TV" and t ~= "Proj") or
                     (bf.kind == "PROJ" and t == "Proj") or
                     (bf.kind == "BRAND" and b == bf.match)
          if ok then table.insert(candidates, i) end
        end
      end
      if #candidates > 0 then
        bruteIdx = 1
        bruteAuto = true
        mode = "BRUTE"
        fireBruteCurrent()
      end
    end

  elseif mode == "BRUTE" then
    if isBack(key, ch) then bruteAuto = false; mode = "PICK_BRAND"; return end
    if #candidates == 0 then return end
    if key == "enter" then
      bruteAuto = false
      confirmDev = parseProfile(cardos.readLine(DB_FILE, candidates[bruteIdx]))
      if confirmDev then
        confirmDev.name = makeUniqueName(confirmDev.b .. " " .. confirmDev.t)
        mode = "CONFIRM"
      end
    elseif key == "char" and ch == " " then
      bruteAuto = not bruteAuto
      lastBruteMs = cardos.millis()
    elseif isLeft(key, ch) then
      bruteAuto = false
      bruteIdx = ((bruteIdx - 2) % #candidates) + 1
      fireBruteCurrent()
    elseif isRight(key, ch) then
      bruteAuto = false
      bruteIdx = (bruteIdx % #candidates) + 1
      fireBruteCurrent()
    elseif cl == "f" then
      fireBruteCurrent()
    elseif cl == "t" then
      bruteUseMute = not bruteUseMute
      fireBruteCurrent()
    end

  elseif mode == "CONFIRM" then
    if isBack(key, ch) then bruteAuto = false; mode = "BRUTE"; return end
    if cl == "p" then
      sendIr(confirmDev, confirmDev.pwr, "PWR")
    elseif cl == "m" then
      sendIr(confirmDev, confirmDev.mt, "MUTE")
    elseif isUp(key, ch) then
      sendIr(confirmDev, confirmDev.vu, "VOL+")
    elseif isDown(key, ch) then
      sendIr(confirmDev, confirmDev.vd, "VOL-")
    elseif cl == "r" then
      table.insert(saved, confirmDev)
      saveSaved()
      activeIdx = #saved
      savedSel = activeIdx
      renameTarget = activeIdx
      renameInput = confirmDev.name
      mode = "RENAME"
    elseif key == "enter" then
      table.insert(saved, confirmDev)
      saveSaved()
      activeIdx = #saved
      savedSel = activeIdx
      mode = "REMOTE"
      cardos.toast(confirmDev.name .. " saved!", 1400)
    end

  elseif mode == "RENAME" then
    if key == "esc" or (key == "char" and ch == "`") then
      mode = (activeIdx >= 1 and activeIdx <= #saved) and "REMOTE" or "SAVED"
      return
    end
    if key == "back" then
      if #renameInput > 0 then renameInput = string.sub(renameInput, 1, #renameInput - 1) end
      return
    end
    if key == "enter" then
      local nm = trim(renameInput)
      if nm ~= "" and renameTarget >= 1 and renameTarget <= #saved then
        saved[renameTarget].name = nm
        saveSaved()
        cardos.toast("Saved name!", 1100)
      end
      mode = (activeIdx >= 1 and activeIdx <= #saved) and "REMOTE" or "SAVED"
      return
    end
    if key == "char" and ch ~= "" and #renameInput < 24 then
      local b = string.byte(ch, 1)
      if b >= 32 and b <= 126 then renameInput = renameInput .. ch end
    end

  elseif mode == "REMOTE" then
    if isBack(key, ch) then activeIdx = 0; mode = "SAVED"; return end
    local d = saved[activeIdx]
    if not d then mode = "SAVED"; return end
    if key == "enter" then
      sendIr(d, d.ok, "OK")
    elseif key == "back" then
      sendIr(d, d.bk, "BACK")
    elseif isUp(key, ch) then
      sendIr(d, d.vu, "VOL+")
    elseif isDown(key, ch) then
      sendIr(d, d.vd, "VOL-")
    elseif isRight(key, ch) then
      sendIr(d, d.cu, "CH+")
    elseif isLeft(key, ch) then
      sendIr(d, d.cd, "CH-")
    elseif cl == "p" then sendIr(d, d.pwr, "POWER")
    elseif cl == "m" then sendIr(d, d.mt, "MUTE")
    elseif ch == "+" or ch == "=" then sendIr(d, d.vu, "VOL+")
    elseif ch == "-" or ch == "_" then sendIr(d, d.vd, "VOL-")
    elseif ch == "]" then sendIr(d, d.cu, "CH+")
    elseif ch == "[" then sendIr(d, d.cd, "CH-")
    elseif cl == "i" then sendIr(d, d.in_, "INPUT")
    elseif cl == "o" then sendIr(d, d.mn, "MENU")
    elseif cl == "b" then sendIr(d, d.bk, "BACK")
    elseif cl == "w" then sendIr(d, d.up, "UP")
    elseif cl == "s" then sendIr(d, d.dn, "DOWN")
    elseif cl == "a" then sendIr(d, d.lt, "LEFT")
    elseif cl == "d" then sendIr(d, d.rt, "RIGHT")
    elseif cl == "r" then
      renameTarget = activeIdx
      renameInput = d.name
      mode = "RENAME"
    end
  end
end

local function drawPulseBadge(cx, cy)
  if cardos.millis() - lastFlashMs < 350 then
    cardos.circle(cx, cy, 4, "err", true)
    cardos.circle(cx, cy, 7, "accent", false)
    cardos.circle(cx, cy, 10, "dim", false)
  else
    cardos.circle(cx, cy, 4, "border", false)
  end
end

function onRender()
  cardos.clear()

  if mode == "SAVED" then
    cardos.statusBar("IR TV Remote")
    cardos.text("Saved (" .. #saved .. ") | DB:" .. dbCount, 6, 21, "dim", 1, "L")
    cardos.fillRect(178, 18, 56, 13, 3, "accent")
    cardos.text("[A] Add", 206, 24, "bg", 1, "C")

    if dbCount == 0 and #saved == 0 then
      cardos.fillRect(8, 36, 224, 78, 6, "surface")
      cardos.drawRect(8, 36, 224, 78, 6, "warn")
      cardos.text("IR Database Missing", 18, 46, "warn", 1, "L")
      cardos.text("Open App Store -> IR TV,", 18, 62, "dim", 1, "L")
      cardos.text("[X] Remove -> [Ent] Download.", 18, 74, "dim", 1, "L")
      cardos.text("[ESC] Home", 18, 94, "accent", 1, "L")
    elseif #saved == 0 then
      cardos.fillRect(8, 36, 224, 78, 6, "surface")
      cardos.drawRect(8, 36, 224, 78, 6, "border")
      cardos.text("No Saved TVs / Projectors", 18, 46, "accent", 1, "L")
      cardos.text("Point Cardputer at TV/Projector", 18, 62, "dim", 1, "L")
      cardos.text("and run Brute-Force IR setup!", 18, 74, "dim", 1, "L")
      cardos.text("Press [A] or [Enter] to Add TV", 18, 94, "ok", 1, "L")
      cardos.text("[A/Ent]Add New   [ESC]Home", 6, 124, "dim", 1, "L")
    else
      for row = 0, 2 do
        local idx = savedScroll + row
        if idx > #saved then break end
        local d = saved[idx]
        local sel = (idx == savedSel)
        local y = 35 + row * 28
        cardos.fillRect(6, y, 224, 25, 4, sel and "surface2" or "surface")
        cardos.drawRect(6, y, 224, 25, 4, sel and "accent" or "border")
        local isProj = (d.t == "Proj")
        cardos.fillRect(10, y + 4, 28, 17, 3, isProj and "accent2" or "accent")
        cardos.text(isProj and "PRJ" or "TV", 24, y + 12, "bg", 1, "C")
        cardos.text(trunc(d.name, 21), 43, y + 4, sel and "accent" or "text", 1, "L")
        cardos.text(trunc(d.b .. " - " .. d.p, 25), 43, y + 14, "dim", 1, "L")
      end
      cardos.scrollbar(234, 35, 80, #saved, 3, savedScroll - 1)
      cardos.text("[Ent]Open [A]Add [P]Pwr [R]Ren [D]Del", 6, 124, "dim", 1, "L")
    end

  elseif mode == "PICK_BRAND" then
    cardos.statusBar("Add TV / Projector")
    cardos.text("Pick Brand (or Brute-Force All):", 8, 20, "dim", 1, "L")
    for i = 0, 3 do
      local idx = brandScroll + i
      if idx > #brands then break end
      local bf = brands[idx]
      local sel = (idx == brandSel)
      local y = 33 + i * 22
      cardos.fillRect(6, y, 224, 20, 4, sel and "surface2" or "surface")
      cardos.drawRect(6, y, 224, 20, 4, sel and "accent" or "border")
      local col = sel and "accent" or (bf.kind ~= "BRAND" and "ok" or "text")
      cardos.text(bf.label, 12, y + 6, col, 1, "L")
      cardos.text(tostring(bf.count) .. (bf.count == 1 and " code" or " codes"), 224, y + 6, "dim", 1, "R")
    end
    cardos.scrollbar(234, 33, 86, #brands, 4, brandScroll - 1)
    cardos.text("[Enter] Start Brute-Force   [ESC] Back", 6, 124, "dim", 1, "L")

  elseif mode == "BRUTE" then
    cardos.statusBar("IR Brute-Force")
    if #candidates == 0 or not bruteDev then return end
    local p = bruteDev
    cardos.fillRect(6, 20, 228, 74, 6, "surface")
    cardos.drawRect(6, 20, 228, 74, 6, bruteAuto and "accent" or "warn")
    cardos.text("Code " .. bruteIdx .. " / " .. #candidates .. " (" .. p.t .. ")", 14, 27, "accent", 1, "L")
    drawPulseBadge(218, 34)
    cardos.text(trunc(p.b, 14), 14, 41, "text", 2, "L")
    cardos.text(p.m .. " (" .. p.p .. ")", 14, 61, "dim", 1, "L")
    cardos.text(bruteAuto and "AUTO-BLAST" or "PAUSED", 14, 75, bruteAuto and "ok" or "warn", 1, "L")
    cardos.text("Signal: " .. (bruteUseMute and "MUTE" or "POWER"), 226, 75, "text", 1, "R")

    local fw = math.max(4, math.floor((220 * bruteIdx) / math.max(1, #candidates)))
    cardos.fillRect(10, 97, 220, 4, 2, "surface2")
    cardos.fillRect(10, 97, fw, 4, 2, "accent")

    cardos.fillRect(14, 104, 212, 16, 4, "ok")
    cardos.text("[Enter] IT WORKED! Save TV", 120, 112, "bg", 1, "C")
    cardos.text("[Spc]Pause [< / >]Step [T]Pwr/Mute [F]Fire", 6, 124, "dim", 1, "L")

  elseif mode == "CONFIRM" then
    cardos.statusBar("Confirm TV Code")
    cardos.fillRect(8, 22, 224, 92, 6, "surface")
    cardos.drawRect(8, 22, 224, 92, 6, "ok")
    cardos.text("Code Matched! Verify Extra Keys:", 16, 30, "ok", 1, "L")
    cardos.text(confirmDev.b .. " - " .. confirmDev.m, 16, 44, "text", 1, "L")
    cardos.text("Name: " .. confirmDev.name, 16, 56, "dim", 1, "L")
    drawPulseBadge(218, 34)

    cardos.fillRect(16, 70, 48, 17, 3, "surface2");  cardos.text("[P]wr", 40, 78, "text", 1, "C")
    cardos.fillRect(68, 70, 50, 17, 3, "surface2");  cardos.text("[M]ute", 93, 78, "text", 1, "C")
    cardos.fillRect(122, 70, 48, 17, 3, "surface2"); cardos.text("[^]Vol+", 146, 78, "text", 1, "C")
    cardos.fillRect(174, 70, 48, 17, 3, "surface2"); cardos.text("[v]Vol-", 198, 78, "text", 1, "C")

    if lastCmdLabel ~= "" then
      cardos.text("Sent: " .. lastCmdLabel, 16, 95, "accent", 1, "L")
    end
    cardos.text("[Enter] Save TV   [R] Rename   [ESC] Back", 6, 124, "dim", 1, "L")

  elseif mode == "RENAME" then
    cardos.statusBar("Name Device")
    cardos.fillRect(12, 28, 216, 74, 6, "surface")
    cardos.drawRect(12, 28, 216, 74, 6, "accent")
    cardos.text("Device Name:", 22, 38, "accent", 1, "L")
    cardos.fillRect(22, 54, 196, 20, 3, "surface2")
    cardos.drawRect(22, 54, 196, 20, 3, "accent")
    cardos.text(renameInput .. "_", 27, 60, "text", 1, "L")
    cardos.text("Enter: Save   ESC: Cancel", 22, 84, "dim", 1, "L")

  elseif mode == "REMOTE" then
    local d = saved[activeIdx]
    if not d then return end
    cardos.statusBar(d.name)
    cardos.text(d.b .. " (" .. d.p .. ")", 8, 19, "dim", 1, "L")
    if lastCmdLabel ~= "" then
      local col = (cardos.millis() - lastFlashMs < 350) and "err" or "accent"
      cardos.text("TX: " .. lastCmdLabel, 212, 19, col, 1, "R")
    end
    drawPulseBadge(225, 23)

    local function btn(x, y, w, h, lbl, bg, fg)
      cardos.fillRect(x, y, w, h, 4, bg)
      cardos.drawRect(x, y, w, h, 4, "border")
      cardos.text(lbl, x + math.floor(w / 2), y + math.floor(h / 2), fg, 1, "C")
    end

    btn(6,   32, 46, 22, "[P]PWR",  "err",      "bg")
    btn(56,  32, 46, 22, "[M]MUTE", "surface",  "warn")
    btn(106, 32, 46, 22, "[I]INP",  "surface",  "text")

    btn(6,   58, 46, 26, "^ VOL+",  "surface2", "accent")
    btn(6,   88, 46, 26, "v VOL-",  "surface2", "text")
    btn(56,  58, 46, 26, "> CH+",   "surface2", "accent")
    btn(56,  88, 46, 26, "< CH-",   "surface2", "text")
    btn(106, 58, 46, 26, "[O]MENU", "surface",  "text")
    btn(106, 88, 46, 26, "[B]BACK", "surface",  "text")

    cardos.fillRect(158, 32, 76, 82, 6, "surface")
    cardos.drawRect(158, 32, 76, 82, 6, "border")
    btn(181, 36, 30, 20, "W ^", "surface2", "text")
    btn(162, 61, 20, 22, "A",   "surface2", "text")
    btn(184, 61, 24, 22, "OK",  "accent",   "bg")
    btn(210, 61, 20, 22, "D",   "surface2", "text")
    btn(181, 88, 30, 20, "S v", "surface2", "text")

    cardos.text("Arrows:Vol/Ch  Ent:OK  R:Rename  ESC:List", 6, 124, "dim", 1, "L")
  end
end
