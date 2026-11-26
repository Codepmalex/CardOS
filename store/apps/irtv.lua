-- IR TV & Projector Universal Remote (GitHub Store Lua App for CardOS)
local SAVED_FILE = "/irtv_saved.tsv"

-- Compact pipe-delimited database (brand|model|type|proto|bits|pwr|vu|vd|mt|cu|cd|in|mn|ok|up|dn|lt|rt|bk)
-- Keeps RAM tiny (~6KB vs ~57KB for Lua hash tables) on ESP32-S3!
local PROFILES = {
  "Samsung|TV #1 (Standard)|TV|SAMSUNG|32|E0E040BF|E0E0E01F|E0E0D02F|E0E0F00F|E0E048B7|E0E008F7|E0E0807F|E0E058A7|E0E016E9|E0E006F9|E0E08679|E0E0A659|E0E046B9|E0E01AE5",
  "Samsung|TV #2 (Smart/Frame)|TV|SAMSUNG|32|E0E06798|E0E0E01F|E0E0D02F|E0E0F00F|E0E048B7|E0E008F7|E0E0807F|E0E058A7|E0E016E9|E0E006F9|E0E08679|E0E0A659|E0E046B9|E0E01AE5",
  "LG|TV #1 (webOS/Std)|TV|NEC|32|20DF10EF|20DF40BF|20DFC03F|20DF906F|20DF00FF|20DF807F|20DFD02F|20DFC23D|20DF22DD|20DF02FD|20DF827D|20DFE01F|20DF609F|20DF14EB",
  "LG|TV #2 (Discrete Pwr)|TV|NEC|32|20DF23DC|20DF40BF|20DFC03F|20DF906F|20DF00FF|20DF807F|20DFD02F|20DFC23D|20DF22DD|20DF02FD|20DF827D|20DFE01F|20DF609F|20DF14EB",
  "Sony|Bravia #1 (SIRC-12)|TV|SONY12|12|A90|490|C90|290|090|890|A50|070|A70|2F0|AF0|2D0|CD0|C70",
  "Sony|Bravia #2 (SIRC-15)|TV|SONY15|15|540A|240A|640A|140A|040A|440A|528A|070A|538A|178A|578A|168A|668A|62E9",
  "Sony|Bravia #3 (SIRC-20)|TV|SONY20|20|1A900|0C900|8C900|2C900|0900|8900|5A500|0700|A700|2F00|AF00|2D00|CD00|62E9",
  "Vizio|SmartCast / LED TV|TV|NEC|32|20DF10EF|20DF40BF|20DFC03F|20DF906F|20DF00FF|20DF807F|20DFF40B|20DFC23D|20DF22DD|20DF02FD|20DF827D|20DFE01F|20DF609F|20DF14EB",
  "TCL / Roku|Roku TV (Standard)|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "TCL|Android / Google TV|TV|NEC|32|F000D52A|F000D22D|F000D32C|F000C03F|F000D02F|F000D12E|F000C53A|F000F50A|F0001EE1|F000A659|F000A758|F000A956|F000A857|F000E51A",
  "Hisense|Smart / VIDAA TV|TV|NEC|32|00FF0DF2|00FF44BB|00FFC43B|00FF0EF1|00FF52AD|00FFD22D|00FF14EB|00FF16E9|00FFD42B|00FF9669|00FF1AE5|00FF1EE1|00FF56A9|00FF5EA1",
  "Hisense|Roku TV|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "Panasonic|Viera TV|TV|PANASONIC|48|40040100BCBD|400401000405|400401008485|400401004C4D|400401002C2D|40040100ACAD|40040100A0A1|400401004A4B|400401009293|400401005253|40040100D2D3|400401007273|40040100F2F3|400401002B2A",
  "Philips|TV #1 (RC5)|TV|RC5|13|0C|10|11|0D|20|21|38|12|17|10|11|15|16|0A",
  "Philips|TV #2 (RC6)|TV|RC6|20|1000C|10010|10011|1000D|10020|10021|10038|10054|1005C|10058|10059|1005A|1005B|1000A",
  "Sharp|Aquos #1 (15-bit)|TV|SHARP|15|41A2|40A2|42A2|43A2|44A2|45A2|4CA2|47A2|4BA2|4D22|4E22|4C22|4A22|4F22",
  "Sharp|Roku / Smart (NEC)|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "Toshiba|Regza #1 (NEC)|TV|NEC|32|02FD48B7|02FD58A7|02FD7887|02FD08F7|02FDD827|02FDF807|02FDF00F|02FD847B|02FD7C83|02FD9867|02FDB847|02FD42BD|02FD02FD|02FDC23D",
  "Toshiba|Fire TV Edition|TV|NEC|32|61A0F00F|61A030CF|61A0B04F|61A0708F|61A050AF|61A0D02F|61A010EF|61A0906F|61A0A857|61A042BD|61A0C23D|61A06897|61A0E817|61A022DD",
  "Insignia|NS / Standard TV|TV|NEC|32|61A0F00F|61A030CF|61A0B04F|61A0708F|61A050AF|61A0D02F|61A010EF|61A0906F|61A0A857|61A042BD|61A0C23D|61A06897|61A0E817|61A022DD",
  "Insignia|Fire TV Edition|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "RCA|TV #1 (NEC)|TV|NEC|32|F708F50A|F708F20D|F708F30C|F708F00F|F708D22D|F708D32C|F708E11E|F708E817|F708A25D|F708A659|F708A758|F708A956|F708A857|F708E51A",
  "Sanyo|TV #1 (NEC)|TV|NEC|32|1CE348B7|1CE3708F|1CE3F00F|1CE318E7|1CE350AF|1CE3D02F|1CE3C837|1CE3847B|1CE32AD5|1CE3D02F|1CE3708F|1CE3F807|1CE37887|1CE3E01F",
  "JVC|TV #1 (16-bit)|TV|JVC|16|C5E8|C578|C5F8|C538|C598|C518|C5C8|C59C|C53C|C5D8|C558|C5B8|C538|C5E0",
  "JVC|Roku / Smart (NEC)|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "Hitachi|TV #1 (NEC)|TV|NEC|32|50AF17E8|50AF21DE|50AFA15E|50AF09F6|50AF817E|50AF41BE|50AF31CE|50AFB14E|50AF718E|50AF916E|50AF51AE|50AFD12E|50AF619E|50AFE11E",
  "Pioneer|Plasma / Fire TV|TV|NEC|32|A55A38C7|A55A50AF|A55AD02F|A55A48B7|A55A906F|A55A10EF|A55AF807|A55A7887|A55AE817|A55AA15E|A55A619E|A55AE11E|A55A11EE|A55AD827",
  "Element|LED / Roku TV|TV|NEC|32|08F71DE2|08F71FE0|08F71EE1|08F71AE5|08F71BE4|08F71CE3|08F70FF0|08F744BB|08F714EB|08F710EF|08F712ED|08F713EC|08F715EA|08F74AB5",
  "Westinghouse|LED TV|TV|NEC|32|689710EF|6897807F|689740BF|689720DF|6897C03F|6897A05F|6897609F|6897E01F|689750AF|6897906F|6897708F|6897B04F|689730CF|6897D02F",
  "Magnavox|TV (RC5)|TV|RC5|13|0C|10|11|0D|20|21|38|12|17|10|11|15|16|0A",
  "Onn / Roku|Walmart Onn TV|TV|NEC|32|57E3E817|57E3F00F|57E308F7|57E304FB|57E348B7|57E38877|57E3B04F|57E38679|57E354AB|57E39867|57E3CC33|57E37887|57E3B44B|57E36699",
  "Haier|LED TV|TV|NEC|32|4BB620DF|4BB640BF|4BB6C03F|4BB6A05F|4BB600FF|4BB6807F|4BB610EF|4BB6D02F|4BB650AF|4BB630CF|4BB6B04F|4BB6708F|4BB6F00F|4BB6906F",
  "Epson|PowerLite #1|Proj|NEC|32|C1AA09F6|C1AA31CE|C1AA718E|C1AA8976|C1AA0DF2|C1AA4DB2|C1AAE11E|C1AA59A6|C1AA39C6|C1AA19E6|C1AA9966|C1AA59A6|C1AAD926|C1AAA15E",
  "Epson|Home Cinema #2|Proj|NEC|32|8355906F|835531CE|8355718E|8355C936|83550DF2|83554DB2|835521DE|835559A6|8355A15E|8355D12E|835551AE|8355916E|835511EE|8355B14E",
  "BenQ|W/TH Series Proj|Proj|NEC|32|000CF20D|000C926D|000CC23D|000CA25D|000CB04F|000C708F|000CF00F|000CD02F|000C10EF|000C00FF|000C807F|000C40BF|000CC03F|000C30CF",
  "Optoma|HD/UHD Projector|Proj|NEC|32|4CB3817E|4CB3B24D|4CB332CD|4CB341BE|4CB3D12E|4CB351AE|4CB30DF2|4CB3619E|4CB3A15E|4CB3C13E|4CB3E11E|4CB3916E|4CB3718E|4CB321DE",
  "ViewSonic|PX / M Series Proj|Proj|NEC|32|83F40BF4|83F4629D|83F4E21D|83F44BB4|83F41AE5|83F49A65|83F42BD4|83F46B94|83F4AB54|83F41BE4|83F49B64|83F45BA4|83F4DB24|83F43BC4",
  "NEC|M/PA Projector|Proj|NEC|32|18E710EF|18E740BF|18E7C03F|18E720DF|18E7807F|18E700FF|18E750AF|18E7609F|18E7A05F|18E7906F|18E7E01F|18E730CF|18E7B04F|18E7708F",
  "Acer|H/X Series Proj|Proj|NEC|32|10C8E11E|10C8718E|10C8B14E|10C831CE|10C851AE|10C8D12E|10C8A15E|10C821DE|10C8916E|10C801FE|10C8817E|10C841BE|10C8C13E|10C8619E",
  "Sony|VPL Projector|Proj|SONY15|15|542A|242A|642A|142A|042A|442A|572A|1D2A|2D2A|172A|572A|162A|662A|62EA",
  "LG|CineBeam Proj|Proj|NEC|32|20DF10EF|20DF40BF|20DFC03F|20DF906F|20DF00FF|20DF807F|20DFD02F|20DFC23D|20DF22DD|20DF02FD|20DF827D|20DFE01F|20DF609F|20DF14EB",
  "Panasonic|PT Projector|Proj|PANASONIC|48|40040100BCBD|400401000405|400401008485|400401004C4D|400401002C2D|40040100ACAD|40040100A0A1|400401004A4B|400401009293|400401005253|40040100D2D3|400401007273|40040100F2F3|400401002B2A",
  "Anker Nebula|Capsule / Mars|Proj|NEC|32|00FF00FF|00FF807F|00FF40BF|00FFC03F|00FF20DF|00FFA05F|00FF609F|00FFE01F|00FF10EF|00FF906F|00FF50AF|00FFD02F|00FF30CF|00FFB04F",
  "XGIMI|Horizon / Halo|Proj|NEC|32|FE013EC1|FE017887|FE01F807|FE01B847|FE0158A7|FE01D827|FE0118E7|FE019867|FE0138C7|FE0108F7|FE018877|FE0148B7|FE01C837|FE016897",
  "Generic|Mini LED Projector|Proj|NEC|32|00FF0DF2|00FF44BB|00FFC43B|00FF0EF1|00FF52AD|00FFD22D|00FF14EB|00FF16E9|00FFD42B|00FF9669|00FF1AE5|00FF1EE1|00FF56A9|00FF5EA1"
}

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
  if not code or code == "" then
    cardos.toast("Key N/A for model", 900)
    return
  end
  cardos.irSend(dev.p, code, dev.bits)
  lastFlashMs = cardos.millis()
  lastCmdLabel = label
end

local function fireBruteCurrent()
  if #candidates == 0 then return end
  bruteDev = parseProfile(PROFILES[candidates[bruteIdx]])
  local useMute = bruteUseMute and (bruteDev.mt ~= nil and bruteDev.mt ~= "")
  local code = useMute and bruteDev.mt or bruteDev.pwr
  sendIr(bruteDev, code, useMute and "MUTE" or "PWR")
  lastBruteMs = cardos.millis()
end

local function buildBrands()
  brands = {}
  local tvCnt, projCnt = 0, 0
  for _, s in ipairs(PROFILES) do
    local _, t = getBrandType(s)
    if t == "Proj" then projCnt = projCnt + 1 else tvCnt = tvCnt + 1 end
  end
  table.insert(brands, {label="* Brute-Force ALL", kind="ALL", match="", count=#PROFILES})
  if tvCnt > 0 then table.insert(brands, {label="* All TVs", kind="TV", match="", count=tvCnt}) end
  if projCnt > 0 then table.insert(brands, {label="* All Projectors", kind="PROJ", match="", count=projCnt}) end

  local seen = {}
  for _, s in ipairs(PROFILES) do
    local b = getBrandType(s)
    if not seen[b] then
      seen[b] = {label=b, kind="BRAND", match=b, count=1}
      table.insert(brands, seen[b])
    else
      seen[b].count = seen[b].count + 1
    end
  end
  brandSel, brandScroll = 1, 1
end

local function isUp(k, c)    return k == "up" or (k == "char" and c == ";") end
local function isDown(k, c)  return k == "down" or (k == "char" and c == ".") end
local function isLeft(k, c)  return k == "left" or (k == "char" and c == ",") end
local function isRight(k, c) return k == "right" or (k == "char" and c == "/") end
local function isBack(k, c)  return k == "esc" or (k == "char" and c == "`") end

function onEnter()
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
      buildBrands()
      mode = "PICK_BRAND"
    elseif key == "enter" then
      if #saved == 0 then
        buildBrands()
        mode = "PICK_BRAND"
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
      for i, s in ipairs(PROFILES) do
        local b, t = getBrandType(s)
        local ok = (bf.kind == "ALL") or
                   (bf.kind == "TV" and t ~= "Proj") or
                   (bf.kind == "PROJ" and t == "Proj") or
                   (bf.kind == "BRAND" and b == bf.match)
        if ok then table.insert(candidates, i) end
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
      confirmDev = parseProfile(PROFILES[candidates[bruteIdx]])
      confirmDev.name = makeUniqueName(confirmDev.b .. " " .. confirmDev.t)
      mode = "CONFIRM"
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
    cardos.text("Saved (" .. #saved .. ") | DB:" .. #PROFILES, 6, 21, "dim", 1, "L")
    cardos.fillRect(178, 18, 56, 13, 3, "accent")
    cardos.text("[A] Add", 206, 24, "bg", 1, "C")

    if #saved == 0 then
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
    if #candidates == 0 then return end
    local p = bruteDev or parseProfile(PROFILES[candidates[bruteIdx]])
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
