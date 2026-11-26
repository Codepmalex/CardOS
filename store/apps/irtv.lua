-- IR TV & Projector Universal Remote (GitHub Store Lua App for CardOS)
local SAVED_FILE = "/irtv_saved.tsv"

local PROFILES = {
  {b="Samsung", m="TV #1 (Standard)", t="TV", p="SAMSUNG", bits=32, pwr="E0E040BF", vu="E0E0E01F", vd="E0E0D02F", mt="E0E0F00F", cu="E0E048B7", cd="E0E008F7", in_="E0E0807F", mn="E0E058A7", ok="E0E016E9", up="E0E006F9", dn="E0E08679", lt="E0E0A659", rt="E0E046B9", bk="E0E01AE5"},
  {b="Samsung", m="TV #2 (Smart/Frame)", t="TV", p="SAMSUNG", bits=32, pwr="E0E06798", vu="E0E0E01F", vd="E0E0D02F", mt="E0E0F00F", cu="E0E048B7", cd="E0E008F7", in_="E0E0807F", mn="E0E058A7", ok="E0E016E9", up="E0E006F9", dn="E0E08679", lt="E0E0A659", rt="E0E046B9", bk="E0E01AE5"},
  {b="LG", m="TV #1 (webOS/Std)", t="TV", p="NEC", bits=32, pwr="20DF10EF", vu="20DF40BF", vd="20DFC03F", mt="20DF906F", cu="20DF00FF", cd="20DF807F", in_="20DFD02F", mn="20DFC23D", ok="20DF22DD", up="20DF02FD", dn="20DF827D", lt="20DFE01F", rt="20DF609F", bk="20DF14EB"},
  {b="LG", m="TV #2 (Discrete Pwr)", t="TV", p="NEC", bits=32, pwr="20DF23DC", vu="20DF40BF", vd="20DFC03F", mt="20DF906F", cu="20DF00FF", cd="20DF807F", in_="20DFD02F", mn="20DFC23D", ok="20DF22DD", up="20DF02FD", dn="20DF827D", lt="20DFE01F", rt="20DF609F", bk="20DF14EB"},
  {b="Sony", m="Bravia #1 (SIRC-12)", t="TV", p="SONY12", bits=12, pwr="A90", vu="490", vd="C90", mt="290", cu="090", cd="890", in_="A50", mn="070", ok="A70", up="2F0", dn="AF0", lt="2D0", rt="CD0", bk="C70"},
  {b="Sony", m="Bravia #2 (SIRC-15)", t="TV", p="SONY15", bits=15, pwr="540A", vu="240A", vd="640A", mt="140A", cu="040A", cd="440A", in_="528A", mn="070A", ok="538A", up="178A", dn="578A", lt="168A", rt="668A", bk="62E9"},
  {b="Sony", m="Bravia #3 (SIRC-20)", t="TV", p="SONY20", bits=20, pwr="1A900", vu="0C900", vd="8C900", mt="2C900", cu="0900", cd="8900", in_="5A500", mn="0700", ok="A700", up="2F00", dn="AF00", lt="2D00", rt="CD00", bk="62E9"},
  {b="Vizio", m="SmartCast / LED TV", t="TV", p="NEC", bits=32, pwr="20DF10EF", vu="20DF40BF", vd="20DFC03F", mt="20DF906F", cu="20DF00FF", cd="20DF807F", in_="20DFF40B", mn="20DFC23D", ok="20DF22DD", up="20DF02FD", dn="20DF827D", lt="20DFE01F", rt="20DF609F", bk="20DF14EB"},
  {b="TCL / Roku", m="Roku TV (Standard)", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="TCL", m="Android / Google TV", t="TV", p="NEC", bits=32, pwr="F000D52A", vu="F000D22D", vd="F000D32C", mt="F000C03F", cu="F000D02F", cd="F000D12E", in_="F000C53A", mn="F000F50A", ok="F0001EE1", up="F000A659", dn="F000A758", lt="F000A956", rt="F000A857", bk="F000E51A"},
  {b="Hisense", m="Smart / VIDAA TV", t="TV", p="NEC", bits=32, pwr="00FF0DF2", vu="00FF44BB", vd="00FFC43B", mt="00FF0EF1", cu="00FF52AD", cd="00FFD22D", in_="00FF14EB", mn="00FF16E9", ok="00FFD42B", up="00FF9669", dn="00FF1AE5", lt="00FF1EE1", rt="00FF56A9", bk="00FF5EA1"},
  {b="Hisense", m="Roku TV", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="Panasonic", m="Viera TV", t="TV", p="PANASONIC", bits=48, pwr="40040100BCBD", vu="400401000405", vd="400401008485", mt="400401004C4D", cu="400401002C2D", cd="40040100ACAD", in_="40040100A0A1", mn="400401004A4B", ok="400401009293", up="400401005253", dn="40040100D2D3", lt="400401007273", rt="40040100F2F3", bk="400401002B2A"},
  {b="Philips", m="TV #1 (RC5)", t="TV", p="RC5", bits=13, pwr="0C", vu="10", vd="11", mt="0D", cu="20", cd="21", in_="38", mn="12", ok="17", up="10", dn="11", lt="15", rt="16", bk="0A"},
  {b="Philips", m="TV #2 (RC6)", t="TV", p="RC6", bits=20, pwr="1000C", vu="10010", vd="10011", mt="1000D", cu="10020", cd="10021", in_="10038", mn="10054", ok="1005C", up="10058", dn="10059", lt="1005A", rt="1005B", bk="1000A"},
  {b="Sharp", m="Aquos #1 (15-bit)", t="TV", p="SHARP", bits=15, pwr="41A2", vu="40A2", vd="42A2", mt="43A2", cu="44A2", cd="45A2", in_="4CA2", mn="47A2", ok="4BA2", up="4D22", dn="4E22", lt="4C22", rt="4A22", bk="4F22"},
  {b="Sharp", m="Roku / Smart (NEC)", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="Toshiba", m="Regza #1 (NEC)", t="TV", p="NEC", bits=32, pwr="02FD48B7", vu="02FD58A7", vd="02FD7887", mt="02FD08F7", cu="02FDD827", cd="02FDF807", in_="02FDF00F", mn="02FD847B", ok="02FD7C83", up="02FD9867", dn="02FDB847", lt="02FD42BD", rt="02FD02FD", bk="02FDC23D"},
  {b="Toshiba", m="Fire TV Edition", t="TV", p="NEC", bits=32, pwr="61A0F00F", vu="61A030CF", vd="61A0B04F", mt="61A0708F", cu="61A050AF", cd="61A0D02F", in_="61A010EF", mn="61A0906F", ok="61A0A857", up="61A042BD", dn="61A0C23D", lt="61A06897", rt="61A0E817", bk="61A022DD"},
  {b="Insignia", m="NS / Standard TV", t="TV", p="NEC", bits=32, pwr="61A0F00F", vu="61A030CF", vd="61A0B04F", mt="61A0708F", cu="61A050AF", cd="61A0D02F", in_="61A010EF", mn="61A0906F", ok="61A0A857", up="61A042BD", dn="61A0C23D", lt="61A06897", rt="61A0E817", bk="61A022DD"},
  {b="Insignia", m="Fire TV Edition", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="RCA", m="TV #1 (NEC)", t="TV", p="NEC", bits=32, pwr="F708F50A", vu="F708F20D", vd="F708F30C", mt="F708F00F", cu="F708D22D", cd="F708D32C", in_="F708E11E", mn="F708E817", ok="F708A25D", up="F708A659", dn="F708A758", lt="F708A956", rt="F708A857", bk="F708E51A"},
  {b="Sanyo", m="TV #1 (NEC)", t="TV", p="NEC", bits=32, pwr="1CE348B7", vu="1CE3708F", vd="1CE3F00F", mt="1CE318E7", cu="1CE350AF", cd="1CE3D02F", in_="1CE3C837", mn="1CE3847B", ok="1CE32AD5", up="1CE3D02F", dn="1CE3708F", lt="1CE3F807", rt="1CE37887", bk="1CE3E01F"},
  {b="JVC", m="TV #1 (16-bit)", t="TV", p="JVC", bits=16, pwr="C5E8", vu="C578", vd="C5F8", mt="C538", cu="C598", cd="C518", in_="C5C8", mn="C59C", ok="C53C", up="C5D8", dn="C558", lt="C5B8", rt="C538", bk="C5E0"},
  {b="JVC", m="Roku / Smart (NEC)", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="Hitachi", m="TV #1 (NEC)", t="TV", p="NEC", bits=32, pwr="50AF17E8", vu="50AF21DE", vd="50AFA15E", mt="50AF09F6", cu="50AF817E", cd="50AF41BE", in_="50AF31CE", mn="50AFB14E", ok="50AF718E", up="50AF916E", dn="50AF51AE", lt="50AFD12E", rt="50AF619E", bk="50AFE11E"},
  {b="Pioneer", m="Plasma / Fire TV", t="TV", p="NEC", bits=32, pwr="A55A38C7", vu="A55A50AF", vd="A55AD02F", mt="A55A48B7", cu="A55A906F", cd="A55A10EF", in_="A55AF807", mn="A55A7887", ok="A55AE817", up="A55AA15E", dn="A55A619E", lt="A55AE11E", rt="A55A11EE", bk="A55AD827"},
  {b="Element", m="LED / Roku TV", t="TV", p="NEC", bits=32, pwr="08F71DE2", vu="08F71FE0", vd="08F71EE1", mt="08F71AE5", cu="08F71BE4", cd="08F71CE3", in_="08F70FF0", mn="08F744BB", ok="08F714EB", up="08F710EF", dn="08F712ED", lt="08F713EC", rt="08F715EA", bk="08F74AB5"},
  {b="Westinghouse", m="LED TV", t="TV", p="NEC", bits=32, pwr="689710EF", vu="6897807F", vd="689740BF", mt="689720DF", cu="6897C03F", cd="6897A05F", in_="6897609F", mn="6897E01F", ok="689750AF", up="6897906F", dn="6897708F", lt="6897B04F", rt="689730CF", bk="6897D02F"},
  {b="Magnavox", m="TV (RC5)", t="TV", p="RC5", bits=13, pwr="0C", vu="10", vd="11", mt="0D", cu="20", cd="21", in_="38", mn="12", ok="17", up="10", dn="11", lt="15", rt="16", bk="0A"},
  {b="Onn / Roku", m="Walmart Onn TV", t="TV", p="NEC", bits=32, pwr="57E3E817", vu="57E3F00F", vd="57E308F7", mt="57E304FB", cu="57E348B7", cd="57E38877", in_="57E3B04F", mn="57E38679", ok="57E354AB", up="57E39867", dn="57E3CC33", lt="57E37887", rt="57E3B44B", bk="57E36699"},
  {b="Haier", m="LED TV", t="TV", p="NEC", bits=32, pwr="4BB620DF", vu="4BB640BF", vd="4BB6C03F", mt="4BB6A05F", cu="4BB600FF", cd="4BB6807F", in_="4BB610EF", mn="4BB6D02F", ok="4BB650AF", up="4BB630CF", dn="4BB6B04F", lt="4BB6708F", rt="4BB6F00F", bk="4BB6906F"},
  {b="Epson", m="PowerLite #1", t="Proj", p="NEC", bits=32, pwr="C1AA09F6", vu="C1AA31CE", vd="C1AA718E", mt="C1AA8976", cu="C1AA0DF2", cd="C1AA4DB2", in_="C1AAE11E", mn="C1AA59A6", ok="C1AA39C6", up="C1AA19E6", dn="C1AA9966", lt="C1AA59A6", rt="C1AAD926", bk="C1AAA15E"},
  {b="Epson", m="Home Cinema #2", t="Proj", p="NEC", bits=32, pwr="8355906F", vu="835531CE", vd="8355718E", mt="8355C936", cu="83550DF2", cd="83554DB2", in_="835521DE", mn="835559A6", ok="8355A15E", up="8355D12E", dn="835551AE", lt="8355916E", rt="835511EE", bk="8355B14E"},
  {b="BenQ", m="W/TH Series Proj", t="Proj", p="NEC", bits=32, pwr="000CF20D", vu="000C926D", vd="000CC23D", mt="000CA25D", cu="000CB04F", cd="000C708F", in_="000CF00F", mn="000CD02F", ok="000C10EF", up="000C00FF", dn="000C807F", lt="000C40BF", rt="000CC03F", bk="000C30CF"},
  {b="Optoma", m="HD/UHD Projector", t="Proj", p="NEC", bits=32, pwr="4CB3817E", vu="4CB3B24D", vd="4CB332CD", mt="4CB341BE", cu="4CB3D12E", cd="4CB351AE", in_="4CB30DF2", mn="4CB3619E", ok="4CB3A15E", up="4CB3C13E", dn="4CB3E11E", lt="4CB3916E", rt="4CB3718E", bk="4CB321DE"},
  {b="ViewSonic", m="PX / M Series Proj", t="Proj", p="NEC", bits=32, pwr="83F40BF4", vu="83F4629D", vd="83F4E21D", mt="83F44BB4", cu="83F41AE5", cd="83F49A65", in_="83F42BD4", mn="83F46B94", ok="83F4AB54", up="83F41BE4", dn="83F49B64", lt="83F45BA4", rt="83F4DB24", bk="83F43BC4"},
  {b="NEC", m="M/PA Projector", t="Proj", p="NEC", bits=32, pwr="18E710EF", vu="18E740BF", vd="18E7C03F", mt="18E720DF", cu="18E7807F", cd="18E700FF", in_="18E750AF", mn="18E7609F", ok="18E7A05F", up="18E7906F", dn="18E7E01F", lt="18E730CF", rt="18E7B04F", bk="18E7708F"},
  {b="Acer", m="H/X Series Proj", t="Proj", p="NEC", bits=32, pwr="10C8E11E", vu="10C8718E", vd="10C8B14E", mt="10C831CE", cu="10C851AE", cd="10C8D12E", in_="10C8A15E", mn="10C821DE", ok="10C8916E", up="10C801FE", dn="10C8817E", lt="10C841BE", rt="10C8C13E", bk="10C8619E"},
  {b="Sony", m="VPL Projector", t="Proj", p="SONY15", bits=15, pwr="542A", vu="242A", vd="642A", mt="142A", cu="042A", cd="442A", in_="572A", mn="1D2A", ok="2D2A", up="172A", dn="572A", lt="162A", rt="662A", bk="62EA"},
  {b="LG", m="CineBeam Proj", t="Proj", p="NEC", bits=32, pwr="20DF10EF", vu="20DF40BF", vd="20DFC03F", mt="20DF906F", cu="20DF00FF", cd="20DF807F", in_="20DFD02F", mn="20DFC23D", ok="20DF22DD", up="20DF02FD", dn="20DF827D", lt="20DFE01F", rt="20DF609F", bk="20DF14EB"},
  {b="Panasonic", m="PT Projector", t="Proj", p="PANASONIC", bits=48, pwr="40040100BCBD", vu="400401000405", vd="400401008485", mt="400401004C4D", cu="400401002C2D", cd="40040100ACAD", in_="40040100A0A1", mn="400401004A4B", ok="400401009293", up="400401005253", dn="40040100D2D3", lt="400401007273", rt="40040100F2F3", bk="400401002B2A"},
  {b="Anker Nebula", m="Capsule / Mars", t="Proj", p="NEC", bits=32, pwr="00FF00FF", vu="00FF807F", vd="00FF40BF", mt="00FFC03F", cu="00FF20DF", cd="00FFA05F", in_="00FF609F", mn="00FFE01F", ok="00FF10EF", up="00FF906F", dn="00FF50AF", lt="00FFD02F", rt="00FF30CF", bk="00FFB04F"},
  {b="XGIMI", m="Horizon / Halo", t="Proj", p="NEC", bits=32, pwr="FE013EC1", vu="FE017887", vd="FE01F807", mt="FE01B847", cu="FE0158A7", cd="FE01D827", in_="FE0118E7", mn="FE019867", ok="FE0138C7", up="FE0108F7", dn="FE018877", lt="FE0148B7", rt="FE01C837", bk="FE016897"},
  {b="Generic", m="Mini LED Projector", t="Proj", p="NEC", bits=32, pwr="00FF0DF2", vu="00FF44BB", vd="00FFC43B", mt="00FF0EF1", cu="00FF52AD", cd="00FFD22D", in_="00FF14EB", mn="00FF16E9", ok="00FFD42B", up="00FF9669", dn="00FF1AE5", lt="00FF1EE1", rt="00FF56A9", bk="00FF5EA1"}
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

local function cloneProfile(src)
  local c = {}
  for k, v in pairs(src) do c[k] = v end
  return c
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
  local dev = PROFILES[candidates[bruteIdx]]
  local useMute = bruteUseMute and (dev.mt ~= nil and dev.mt ~= "")
  local code = useMute and dev.mt or dev.pwr
  sendIr(dev, code, useMute and "MUTE" or "PWR")
  lastBruteMs = cardos.millis()
end

local function buildBrands()
  brands = {}
  local tvCnt, projCnt = 0, 0
  for _, p in ipairs(PROFILES) do
    if p.t == "Proj" then projCnt = projCnt + 1 else tvCnt = tvCnt + 1 end
  end
  table.insert(brands, {label="* Brute-Force ALL", kind="ALL", match="", count=#PROFILES})
  if tvCnt > 0 then table.insert(brands, {label="* All TVs", kind="TV", match="", count=tvCnt}) end
  if projCnt > 0 then table.insert(brands, {label="* All Projectors", kind="PROJ", match="", count=projCnt}) end

  local seen = {}
  for _, p in ipairs(PROFILES) do
    if not seen[p.b] then
      seen[p.b] = {label=p.b, kind="BRAND", match=p.b, count=1}
      table.insert(brands, seen[p.b])
    else
      seen[p.b].count = seen[p.b].count + 1
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
      for i, p in ipairs(PROFILES) do
        local ok = (bf.kind == "ALL") or
                   (bf.kind == "TV" and p.t ~= "Proj") or
                   (bf.kind == "PROJ" and p.t == "Proj") or
                   (bf.kind == "BRAND" and p.b == bf.match)
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
      confirmDev = cloneProfile(PROFILES[candidates[bruteIdx]])
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
    local p = PROFILES[candidates[bruteIdx]]
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

