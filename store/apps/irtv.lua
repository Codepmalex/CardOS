local DB, SV = "/store/irtv_db.tsv", "/irtv_saved.tsv"
local dbN, md = 0, "SAVED"
local sv, sSel, sScr = {}, 1, 1
local br, bSel, bScr, idxB, idxT = {}, 1, 1, {}, {}
local cnd, bIdx, bAuto, bStep, bMute, lastB = {}, 1, true, false, false, 0
local bDev, cDev, act = nil, nil, 0
local rnTxt, rnIdx, flMs, cmdLbl = "", 0, 0, ""

local function tr(s) return (s:gsub("^%s*(.-)%s*$", "%1")) end
local function trunc(s, n) return (#s > n) and (s:sub(1, n - 1) .. "~") or s end

local function spl(ln)
  local p, a = {}, 1
  while true do
    local i = ln:find("|", a, true)
    if not i then table.insert(p, ln:sub(a)); break end
    table.insert(p, ln:sub(a, i - 1)); a = i + 1
  end
  return p
end

local function parseP(s)
  if not s or s == "" then return nil end
  local f = spl(s)
  return {
    b=f[1] or "", m=f[2] or "", t=f[3] or "TV", p=f[4] or "NEC", bits=tonumber(f[5]) or 32,
    pwr=f[6] or "", vu=f[7] or "", vd=f[8] or "", mt=f[9] or "",
    cu=f[10] or "", cd=f[11] or "", in_=f[12] or "", mn=f[13] or "",
    ok=f[14] or "", up=f[15] or "", dn=f[16] or "", lt=f[17] or "", rt=f[18] or "", bk=f[19] or ""
  }
end

local function brandType(s)
  if not s then return "", "TV" end
  local b, _, t = s:match("^([^|]+)|([^|]+)|([^|]+)|")
  return b or "", t or "TV"
end

local function loadSv()
  sv = {}
  local raw = cardos.readFile(SV)
  if not raw or raw == "" then return end
  for ln in raw:gmatch("[^\r\n]+") do
    local f = spl(ln)
    if #f >= 6 and f[1] ~= "" then
      table.insert(sv, {
        name=f[1], b=f[2] or "", m=f[3] or "", t=f[4] or "TV", p=f[5] or "NEC", bits=tonumber(f[6]) or 32,
        pwr=f[7] or "", vu=f[8] or "", vd=f[9] or "", mt=f[10] or "",
        cu=f[11] or "", cd=f[12] or "", in_=f[13] or "", mn=f[14] or "",
        ok=f[15] or "", up=f[16] or "", dn=f[17] or "", lt=f[18] or "", rt=f[19] or "", bk=f[20] or ""
      })
    end
  end
end

local function saveSv()
  local out = {}
  for _, d in ipairs(sv) do
    local nm = (d.name or "TV"):gsub("[|\r\n]", " ")
    table.insert(out, table.concat({
      nm, d.b, d.m, d.t, d.p, tostring(d.bits or 32),
      d.pwr, d.vu, d.vd, d.mt, d.cu, d.cd, d.in_, d.mn,
      d.ok, d.up, d.dn, d.lt, d.rt, d.bk
    }, "|"))
  end
  cardos.writeFile(SV, table.concat(out, "\n") .. "\n")
end

local function uniqName(base)
  local function used(n)
    local l = n:lower()
    for _, d in ipairs(sv) do if d.name:lower() == l then return true end end
    return false
  end
  if not used(base) then return base end
  for i = 2, 50 do
    local c = base .. " " .. i
    if not used(c) then return c end
  end
  return base
end

local function sendIr(d, code, lbl)
  if not d or not code or code == "" then cardos.toast("Key N/A", 800); return end
  cardos.irSend(d.p, code, d.bits)
  flMs, cmdLbl = cardos.millis(), lbl
end

local function fireBrute()
  if #cnd == 0 then return end
  bDev = parseP(cardos.readLine(DB, cnd[bIdx]))
  if not bDev then return end
  local um = bMute and bDev.mt ~= ""
  sendIr(bDev, um and bDev.mt or bDev.pwr, um and "MUTE" or "PWR")
  lastB = cardos.millis()
end

local function buildBr()
  br = {}
  if dbN == 0 then cardos.toast("Re-download IR TV in Store", 1600); return false end
  if #idxB ~= dbN then
    idxB, idxT = {}, {}
    for i = 1, dbN do
      local b, t = brandType(cardos.readLine(DB, i))
      idxB[i], idxT[i] = b, t
    end
  end
  local tvC, prC, seen, ord = 0, 0, {}, {}
  for i = 1, dbN do
    local b, t = idxB[i], idxT[i]
    if t == "Proj" then prC = prC + 1 else tvC = tvC + 1 end
    if not seen[b] then seen[b] = {l=b, k="B", m=b, c=1}; table.insert(ord, seen[b])
    else seen[b].c = seen[b].c + 1 end
  end
  table.insert(br, {l="* Brute-Force ALL", k="A", m="", c=dbN})
  if tvC > 0 then table.insert(br, {l="* All TVs", k="T", m="", c=tvC}) end
  if prC > 0 then table.insert(br, {l="* All Projectors", k="P", m="", c=prC}) end
  for _, v in ipairs(ord) do table.insert(br, v) end
  bSel, bScr = 1, 1
  return true
end

local function isUp(k, c) return k == "up" or (k == "char" and c == ";") end
local function isDn(k, c) return k == "down" or (k == "char" and c == ".") end
local function isLt(k, c) return k == "left" or (k == "char" and c == ",") end
local function isRt(k, c) return k == "right" or (k == "char" and c == "/") end
local function isBk(k, c) return k == "esc" or (k == "char" and c == "`") end

local function pulse(x, y)
  if cardos.millis() - flMs < 350 then
    cardos.circle(x, y, 4, "err", true); cardos.circle(x, y, 7, "accent", false); cardos.circle(x, y, 10, "dim", false)
  else
    cardos.circle(x, y, 4, "border", false)
  end
end

local function btn(x, y, w, h, lbl, bg, fg)
  cardos.fillRect(x, y, w, h, 4, bg); cardos.drawRect(x, y, w, h, 4, "border")
  cardos.text(lbl, x + math.floor(w / 2), y + math.floor(h / 2), fg, 1, "C")
end

local function kSaved(k, c, cl)
  if isBk(k, c) then cardos.exit(); return end
  if isUp(k, c) and sSel > 1 then sSel = sSel - 1; if sSel < sScr then sScr = sSel end
  elseif isDn(k, c) and sSel < #sv then sSel = sSel + 1; if sSel >= sScr + 3 then sScr = sSel - 2 end
  elseif cl == "a" then if buildBr() then md = "PICK" end
  elseif k == "enter" then if #sv == 0 then if buildBr() then md = "PICK" end else act = sSel; md = "REMOTE" end
  elseif cl == "r" and #sv > 0 then rnIdx, rnTxt, md = sSel, sv[sSel].name, "RENAME"
  elseif (k == "del" or cl == "d") and #sv > 0 then
    local rm = sv[sSel].name; table.remove(sv, sSel)
    if sSel > #sv then sSel = math.max(1, #sv) end
    if sScr > math.max(1, #sv - 2) then sScr = math.max(1, #sv - 2) end
    saveSv(); cardos.toast("Removed " .. rm, 1100)
  elseif cl == "p" and #sv > 0 then sendIr(sv[sSel], sv[sSel].pwr, "PWR") end
end

local function kPick(k, c)
  if isBk(k, c) then md = "SAVED"; return end
  if isUp(k, c) and bSel > 1 then bSel = bSel - 1; if bSel < bScr then bScr = bSel end
  elseif isDn(k, c) and bSel < #br then bSel = bSel + 1; if bSel >= bScr + 4 then bScr = bSel - 3 end
  elseif k == "enter" and #br > 0 then
    local bf = br[bSel]; cnd = {}
    for i = 1, dbN do
      local b, t = idxB[i], idxT[i]
      if not b then b, t = brandType(cardos.readLine(DB, i)) end
      if bf.k == "A" or (bf.k == "T" and t ~= "Proj") or (bf.k == "P" and t == "Proj") or (bf.k == "B" and b == bf.m) then
        table.insert(cnd, i)
      end
    end
    if #cnd > 0 then bIdx, bAuto, bStep, md = 1, true, false, "BRUTE"; fireBrute() end
  end
end

local function kBrute(k, c, cl)
  if isBk(k, c) then bAuto = false; md = "PICK"; return end
  if #cnd == 0 then return end
  if cl == "m" then bStep = not bStep; bAuto = not bStep; lastB = cardos.millis(); return end
  if cl == "t" then bMute = not bMute; fireBrute(); return end
  if cl == "f" then fireBrute(); return end
  if k == "enter" or cl == "y" then
    bAuto = false; cDev = parseP(cardos.readLine(DB, cnd[bIdx]))
    if cDev then cDev.name = uniqName(cDev.b .. " " .. cDev.t); md = "CONFIRM" end
    return
  end
  if bStep then
    if cl == "n" or (k == "char" and c == " ") or isRt(k, c) then bIdx = (bIdx % #cnd) + 1; fireBrute()
    elseif isLt(k, c) then bIdx = ((bIdx - 2) % #cnd) + 1; fireBrute() end
  else
    if k == "char" and c == " " then bAuto = not bAuto; lastB = cardos.millis()
    elseif isLt(k, c) then bAuto = false; bIdx = ((bIdx - 2) % #cnd) + 1; fireBrute()
    elseif isRt(k, c) then bAuto = false; bIdx = (bIdx % #cnd) + 1; fireBrute() end
  end
end

local function kConf(k, c, cl)
  if isBk(k, c) then bAuto = false; md = "BRUTE"; return end
  if cl == "p" then sendIr(cDev, cDev.pwr, "PWR")
  elseif cl == "m" then sendIr(cDev, cDev.mt, "MUTE")
  elseif isUp(k, c) then sendIr(cDev, cDev.vu, "VOL+")
  elseif isDn(k, c) then sendIr(cDev, cDev.vd, "VOL-")
  elseif cl == "r" or k == "enter" then
    table.insert(sv, cDev); saveSv(); act = #sv; sSel = act
    if cl == "r" then rnIdx, rnTxt, md = act, cDev.name, "RENAME"
    else md = "REMOTE"; cardos.toast(cDev.name .. " saved!", 1300) end
  end
end

local function kRen(k, c)
  local prev = (act >= 1 and act <= #sv) and "REMOTE" or "SAVED"
  if k == "esc" or (k == "char" and c == "`") then md = prev; return end
  if k == "back" then if #rnTxt > 0 then rnTxt = rnTxt:sub(1, #rnTxt - 1) end; return end
  if k == "enter" then
    local n = tr(rnTxt)
    if n ~= "" and sv[rnIdx] then sv[rnIdx].name = n; saveSv(); cardos.toast("Saved!", 900) end
    md = prev; return
  end
  if k == "char" and c ~= "" and #rnTxt < 24 then
    local b = c:byte(1); if b >= 32 and b <= 126 then rnTxt = rnTxt .. c end
  end
end

local RMAP = {p={"pwr","POWER"}, m={"mt","MUTE"}, ["+"]={"vu","VOL+"}, ["="]={"vu","VOL+"},
  ["-"]={"vd","VOL-"}, _={"vd","VOL-"}, ["]"]={"cu","CH+"}, ["["]={"cd","CH-"},
  i={"in_","INPUT"}, o={"mn","MENU"}, b={"bk","BACK"}, w={"up","UP"}, s={"dn","DOWN"}, a={"lt","LEFT"}, d={"rt","RIGHT"}}

local function kRem(k, c, cl)
  if isBk(k, c) then act = 0; md = "SAVED"; return end
  local d = sv[act]; if not d then md = "SAVED"; return end
  if k == "enter" then sendIr(d, d.ok, "OK")
  elseif k == "back" then sendIr(d, d.bk, "BACK")
  elseif isUp(k, c) then sendIr(d, d.vu, "VOL+")
  elseif isDn(k, c) then sendIr(d, d.vd, "VOL-")
  elseif isRt(k, c) then sendIr(d, d.cu, "CH+")
  elseif isLt(k, c) then sendIr(d, d.cd, "CH-")
  elseif cl == "r" then rnIdx, rnTxt, md = act, d.name, "RENAME"
  elseif RMAP[cl] then sendIr(d, d[RMAP[cl][1]], RMAP[cl][2]) end
end

local function rSaved()
  cardos.statusBar("IR TV Remote")
  cardos.text("Saved (" .. #sv .. ") | DB:" .. dbN, 6, 21, "dim", 1, "L")
  cardos.fillRect(178, 18, 56, 13, 3, "accent"); cardos.text("[A] Add", 206, 24, "bg", 1, "C")
  if dbN == 0 and #sv == 0 then
    cardos.fillRect(8, 36, 224, 78, 6, "surface"); cardos.drawRect(8, 36, 224, 78, 6, "warn")
    cardos.text("IR Database Missing", 18, 46, "warn", 1, "L")
    cardos.text("Store -> IR TV -> [X] -> Get.", 18, 64, "dim", 1, "L")
    cardos.text("[ESC] Home", 18, 92, "accent", 1, "L"); return
  end
  if #sv == 0 then
    cardos.fillRect(8, 36, 224, 78, 6, "surface"); cardos.drawRect(8, 36, 224, 78, 6, "border")
    cardos.text("No Saved TVs / Projectors", 18, 46, "accent", 1, "L")
    cardos.text("Point at TV & brute-force code!", 18, 64, "dim", 1, "L")
    cardos.text("Press [A] or [Enter] to Add", 18, 92, "ok", 1, "L")
    cardos.text("[A/Ent]Add New   [ESC]Home", 6, 124, "dim", 1, "L"); return
  end
  for r = 0, 2 do
    local d = sv[sScr + r]; if not d then break end
    local sel, y = (sScr + r == sSel), 35 + r * 28
    cardos.fillRect(6, y, 224, 25, 4, sel and "surface2" or "surface")
    cardos.drawRect(6, y, 224, 25, 4, sel and "accent" or "border")
    local pr = d.t == "Proj"
    cardos.fillRect(10, y + 4, 28, 17, 3, pr and "accent2" or "accent")
    cardos.text(pr and "PRJ" or "TV", 24, y + 12, "bg", 1, "C")
    cardos.text(trunc(d.name, 21), 43, y + 4, sel and "accent" or "text", 1, "L")
    cardos.text(trunc(d.b .. " - " .. d.p, 25), 43, y + 14, "dim", 1, "L")
  end
  cardos.scrollbar(234, 35, 80, #sv, 3, sScr - 1)
  cardos.text("[Ent]Open [A]Add [P]Pwr [R]Ren [D]Del", 6, 124, "dim", 1, "L")
end

local function rPick()
  cardos.statusBar("Add TV / Projector")
  cardos.text("Pick Brand (or Brute-Force All):", 8, 20, "dim", 1, "L")
  for i = 0, 3 do
    local bf = br[bScr + i]; if not bf then break end
    local sel, y = (bScr + i == bSel), 33 + i * 22
    cardos.fillRect(6, y, 224, 20, 4, sel and "surface2" or "surface")
    cardos.drawRect(6, y, 224, 20, 4, sel and "accent" or "border")
    cardos.text(bf.l, 12, y + 6, sel and "accent" or (bf.k ~= "B" and "ok" or "text"), 1, "L")
    cardos.text(bf.c .. (bf.c == 1 and " code" or " codes"), 224, y + 6, "dim", 1, "R")
  end
  cardos.scrollbar(234, 33, 86, #br, 4, bScr - 1)
  cardos.text("[Enter] Start Brute-Force   [ESC] Back", 6, 124, "dim", 1, "L")
end

local function rBrute()
  cardos.statusBar("IR Brute-Force")
  if #cnd == 0 or not bDev then return end
  local p = bDev
  cardos.fillRect(6, 20, 228, 74, 6, "surface"); cardos.drawRect(6, 20, 228, 74, 6, bAuto and "accent" or "warn")
  cardos.text("Code " .. bIdx .. " / " .. #cnd .. " (" .. p.t .. ")", 14, 27, "accent", 1, "L"); pulse(218, 34)
  cardos.text(trunc(p.b, 14), 14, 41, "text", 2, "L")
  cardos.text(p.m .. " (" .. p.p .. ")", 14, 61, "dim", 1, "L")
  local stTxt = bStep and "STEP [Y/N] CONFIRM" or (bAuto and "AUTO-BLAST" or "PAUSED")
  local stCol = bStep and "accent" or (bAuto and "ok" or "warn")
  cardos.text(stTxt, 14, 75, stCol, 1, "L")
  cardos.text("Sig: " .. (bMute and "MUTE" or "PWR"), 226, 75, "text", 1, "R")
  local fw = math.max(4, math.floor((220 * bIdx) / math.max(1, #cnd)))
  cardos.fillRect(10, 97, 220, 4, 2, "surface2"); cardos.fillRect(10, 97, fw, 4, 2, "accent")
  if bStep then
    cardos.fillRect(10, 104, 106, 16, 4, "ok"); cardos.text("[Y/Ent] WORKED", 63, 112, "bg", 1, "C")
    cardos.fillRect(124, 104, 106, 16, 4, "err"); cardos.text("[N/Spc] NO ->", 177, 112, "bg", 1, "C")
    cardos.text("[<]Prev [F]Fire [T]Pwr/Mt [M]AutoMode", 6, 124, "dim", 1, "L")
  else
    cardos.fillRect(14, 104, 212, 16, 4, "ok"); cardos.text("[Enter] IT WORKED! Save TV", 120, 112, "bg", 1, "C")
    cardos.text("[Spc]Pause [</>]Step [M]Y/N [T]Pwr/Mt", 6, 124, "dim", 1, "L")
  end
end

local function rConf()
  cardos.statusBar("Confirm TV Code")
  cardos.fillRect(8, 22, 224, 92, 6, "surface"); cardos.drawRect(8, 22, 224, 92, 6, "ok")
  cardos.text("Code Matched! Verify Extra Keys:", 16, 30, "ok", 1, "L"); pulse(218, 34)
  cardos.text(cDev.b .. " - " .. cDev.m, 16, 44, "text", 1, "L")
  cardos.text("Name: " .. cDev.name, 16, 56, "dim", 1, "L")
  btn(16, 70, 48, 17, "[P]wr", "surface2", "text"); btn(68, 70, 50, 17, "[M]ute", "surface2", "text")
  btn(122, 70, 48, 17, "[^]Vol+", "surface2", "text"); btn(174, 70, 48, 17, "[v]Vol-", "surface2", "text")
  if cmdLbl ~= "" then cardos.text("Sent: " .. cmdLbl, 16, 95, "accent", 1, "L") end
  cardos.text("[Enter] Save TV   [R] Rename   [ESC] Back", 6, 124, "dim", 1, "L")
end

local function rRen()
  cardos.statusBar("Name Device")
  cardos.fillRect(12, 28, 216, 74, 6, "surface"); cardos.drawRect(12, 28, 216, 74, 6, "accent")
  cardos.text("Device Name:", 22, 38, "accent", 1, "L")
  cardos.fillRect(22, 54, 196, 20, 3, "surface2"); cardos.drawRect(22, 54, 196, 20, 3, "accent")
  cardos.text(rnTxt .. "_", 27, 60, "text", 1, "L")
  cardos.text("Enter: Save   ESC: Cancel", 22, 84, "dim", 1, "L")
end

local function rRem()
  local d = sv[act]; if not d then return end
  cardos.statusBar(d.name)
  cardos.text(d.b .. " (" .. d.p .. ")", 8, 19, "dim", 1, "L")
  if cmdLbl ~= "" then
    cardos.text("TX: " .. cmdLbl, 212, 19, (cardos.millis() - flMs < 350) and "err" or "accent", 1, "R")
  end
  pulse(225, 23)
  btn(6, 32, 46, 22, "[P]PWR", "err", "bg"); btn(56, 32, 46, 22, "[M]MUTE", "surface", "warn"); btn(106, 32, 46, 22, "[I]INP", "surface", "text")
  btn(6, 58, 46, 26, "^ VOL+", "surface2", "accent"); btn(6, 88, 46, 26, "v VOL-", "surface2", "text")
  btn(56, 58, 46, 26, "> CH+", "surface2", "accent"); btn(56, 88, 46, 26, "< CH-", "surface2", "text")
  btn(106, 58, 46, 26, "[O]MENU", "surface", "text"); btn(106, 88, 46, 26, "[B]BACK", "surface", "text")
  cardos.fillRect(158, 32, 76, 82, 6, "surface"); cardos.drawRect(158, 32, 76, 82, 6, "border")
  btn(181, 36, 30, 20, "W ^", "surface2", "text"); btn(162, 61, 20, 22, "A", "surface2", "text")
  btn(184, 61, 24, 22, "OK", "accent", "bg"); btn(210, 61, 20, 22, "D", "surface2", "text")
  btn(181, 88, 30, 20, "S v", "surface2", "text")
  cardos.text("Arrows:Vol/Ch  Ent:OK  R:Rename  ESC:List", 6, 124, "dim", 1, "L")
end

local IN = {SAVED=kSaved, PICK=kPick, BRUTE=kBrute, CONFIRM=kConf, RENAME=kRen, REMOTE=kRem}
local RD = {SAVED=rSaved, PICK=rPick, BRUTE=rBrute, CONFIRM=rConf, RENAME=rRen, REMOTE=rRem}

function onEnter()
  dbN = cardos.countLines(DB); idxB, idxT = {}, {}; loadSv()
  if sSel > #sv then sSel = math.max(1, #sv) end
  if sScr > math.max(1, #sv - 2) then sScr = math.max(1, #sv - 2) end
  flMs, cmdLbl, md = 0, "", "SAVED"
end

function onTick()
  if md == "BRUTE" and not bStep and bAuto and #cnd > 0 and cardos.millis() - lastB >= 1100 then
    bIdx = (bIdx % #cnd) + 1; fireBrute()
  end
end

function onInput(k, c) local f = IN[md]; if f then f(k, c, (c or ""):lower()) end end
function onRender() cardos.clear(); local f = RD[md]; if f then f() end end
