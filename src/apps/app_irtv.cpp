#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include "../ir_tx.h"
#include <ArduinoJson.h>
#include <stdlib.h>
#include <vector>

extern void switchApp(AppId id);

namespace {

static constexpr const char* IRTV_DB_FILE    = "/store/irtv.json";
static constexpr const char* IRTV_SAVED_FILE = "/irtv_saved.json";

struct IrProfile {
    String name;
    String brand;
    String model;
    String type;   // "TV" or "Proj"
    String proto;  // "NEC", "SAMSUNG", "SONY12", etc.
    uint8_t bits = 32;
    uint64_t pwr = 0, vu = 0, vd = 0, mt = 0, cu = 0, cd = 0;
    uint64_t in = 0, mn = 0, ok = 0, up = 0, dn = 0, lt = 0, rt = 0, bk = 0;
};

struct BrandFilterItem {
    enum Kind : uint8_t { KIND_ALL = 0, KIND_TV, KIND_PROJ, KIND_BRAND };
    String label;
    String brandMatch; // empty unless KIND_BRAND
    Kind kind;
    int count;
};

static uint64_t parseHex64(const char* s) {
    if (!s || !s[0]) return 0;
    return (uint64_t)strtoull(s, nullptr, 16);
}

static String hex64Str(uint64_t v) {
    if (v == 0) return "";
    char buf[20];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)v);
    return String(buf);
}

static IrProfile profileFromJson(JsonObject o) {
    IrProfile p;
    p.name  = String(o["name"] | "");
    p.brand = String(o["b"] | "");
    p.model = String(o["m"] | "");
    p.type  = String(o["t"] | "TV");
    p.proto = String(o["p"] | "NEC");
    p.bits  = (uint8_t)(o["bits"] | 32);
    p.pwr   = parseHex64(o["pwr"] | "");
    p.vu    = parseHex64(o["vu"]  | "");
    p.vd    = parseHex64(o["vd"]  | "");
    p.mt    = parseHex64(o["mt"]  | "");
    p.cu    = parseHex64(o["cu"]  | "");
    p.cd    = parseHex64(o["cd"]  | "");
    p.in    = parseHex64(o["in"]  | "");
    p.mn    = parseHex64(o["mn"]  | "");
    p.ok    = parseHex64(o["ok"]  | "");
    p.up    = parseHex64(o["up"]  | "");
    p.dn    = parseHex64(o["dn"]  | "");
    p.lt    = parseHex64(o["lt"]  | "");
    p.rt    = parseHex64(o["rt"]  | "");
    p.bk    = parseHex64(o["bk"]  | "");
    return p;
}

static void profileToJson(const IrProfile& p, JsonObject o) {
    o["name"] = p.name;
    o["b"]    = p.brand;
    o["m"]    = p.model;
    o["t"]    = p.type;
    o["p"]    = p.proto;
    o["bits"] = p.bits;
    if (p.pwr) o["pwr"] = hex64Str(p.pwr);
    if (p.vu)  o["vu"]  = hex64Str(p.vu);
    if (p.vd)  o["vd"]  = hex64Str(p.vd);
    if (p.mt)  o["mt"]  = hex64Str(p.mt);
    if (p.cu)  o["cu"]  = hex64Str(p.cu);
    if (p.cd)  o["cd"]  = hex64Str(p.cd);
    if (p.in)  o["in"]  = hex64Str(p.in);
    if (p.mn)  o["mn"]  = hex64Str(p.mn);
    if (p.ok)  o["ok"]  = hex64Str(p.ok);
    if (p.up)  o["up"]  = hex64Str(p.up);
    if (p.dn)  o["dn"]  = hex64Str(p.dn);
    if (p.lt)  o["lt"]  = hex64Str(p.lt);
    if (p.rt)  o["rt"]  = hex64Str(p.rt);
    if (p.bk)  o["bk"]  = hex64Str(p.bk);
}

class IrTvApp : public App {
public:
    AppId id() const override { return APP_IRTV; }
    const char* name() const override { return "IR TV"; }

    void onEnter() override {
        initIrTx();
        m_lastIrFlashMs = 0;
        m_lastCmdLabel[0] = '\0';
        loadDatabase();
        loadSavedDevices();
        m_savedSel = constrain(m_savedSel, 0, max(0, (int)m_saved.size() - 1));
        m_savedScroll = constrain(m_savedScroll, 0, max(0, (int)m_saved.size() - 3));
        m_mode = MODE_SAVED;
    }

    void onTick() override {
        if (m_mode == MODE_BRUTE && m_bruteAuto && !m_candidates.empty()) {
            uint32_t now = millis();
            if (now - m_lastBruteSendMs >= 1100) {
                m_bruteIdx = (m_bruteIdx + 1) % (int)m_candidates.size();
                fireBruteCurrent();
            }
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_catalog.empty() && m_saved.empty()) {
            if (isNavBack(ev)) { switchApp(APP_HOME); return; }
            if (ev.code == CK_CHAR && (ev.ch == 's' || ev.ch == 'S')) {
                switchApp(APP_STORE);
            }
            return;
        }

        switch (m_mode) {
            case MODE_SAVED:      handleSavedInput(ev); break;
            case MODE_PICK_BRAND: handlePickBrandInput(ev); break;
            case MODE_BRUTE:      handleBruteInput(ev); break;
            case MODE_CONFIRM:    handleConfirmInput(ev); break;
            case MODE_RENAME:     handleRenameInput(ev); break;
            case MODE_REMOTE:     handleRemoteInput(ev); break;
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        if (m_catalog.empty() && m_saved.empty()) {
            ui::drawStatusBar(spr, "IR TV");
            spr.fillRoundRect(10, 26, SCREEN_W - 20, 88, 6, th.surface);
            spr.drawRoundRect(10, 26, SCREEN_W - 20, 88, 6, th.border);
            spr.setTextSize(1);
            spr.setTextColor(th.warn);
            spr.drawString("IR Code DB Not Found", 20, 38);
            spr.setTextColor(th.textDim);
            spr.drawString("Download IR TV in App Store", 20, 56);
            spr.drawString("to fetch TV & Proj codes.", 20, 68);
            spr.setTextColor(acc);
            spr.drawString("[S] Open App Store  [ESC] Home", 20, 92);
            ui::drawToastOverlay(spr);
            return;
        }

        switch (m_mode) {
            case MODE_SAVED:      renderSaved(spr, th, acc); break;
            case MODE_PICK_BRAND: renderPickBrand(spr, th, acc); break;
            case MODE_BRUTE:      renderBrute(spr, th, acc); break;
            case MODE_CONFIRM:    renderConfirm(spr, th, acc); break;
            case MODE_RENAME:     renderRename(spr, th, acc); break;
            case MODE_REMOTE:     renderRemote(spr, th, acc); break;
        }
        ui::drawToastOverlay(spr);
    }

private:
    enum Mode : uint8_t {
        MODE_SAVED = 0,
        MODE_PICK_BRAND,
        MODE_BRUTE,
        MODE_CONFIRM,
        MODE_RENAME,
        MODE_REMOTE
    } m_mode = MODE_SAVED;

    std::vector<IrProfile> m_catalog;
    std::vector<IrProfile> m_saved;
    std::vector<BrandFilterItem> m_brands;
    std::vector<int> m_candidates;

    int m_savedSel = 0;
    int m_savedScroll = 0;
    int m_brandSel = 0;
    int m_brandScroll = 0;

    int m_bruteIdx = 0;
    bool m_bruteAuto = true;
    bool m_bruteUseMute = false;
    uint32_t m_lastBruteSendMs = 0;

    IrProfile m_confirmDev;
    int m_activeSavedIdx = -1;
    String m_renameInput;
    int m_renameTargetIdx = -1;

    uint32_t m_lastIrFlashMs = 0;
    char m_lastCmdLabel[20] = "";

    void loadDatabase() {
        m_catalog.clear();
        if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
        if (!activeFS().exists(IRTV_DB_FILE)) return;
        String raw = readTextFile(IRTV_DB_FILE, 32768);
        if (raw.isEmpty()) return;
        JsonDocument doc;
        if (deserializeJson(doc, raw) != DeserializationError::Ok) return;
        JsonArray arr = doc["profiles"].as<JsonArray>();
        for (JsonObject o : arr) {
            IrProfile p = profileFromJson(o);
            if (!p.brand.isEmpty() && p.pwr != 0) {
                m_catalog.push_back(p);
            }
        }
    }

    void loadSavedDevices() {
        m_saved.clear();
        if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
        if (!activeFS().exists(IRTV_SAVED_FILE)) return;
        String raw = readTextFile(IRTV_SAVED_FILE, 16384);
        if (raw.isEmpty()) return;
        JsonDocument doc;
        if (deserializeJson(doc, raw) != DeserializationError::Ok) return;
        JsonArray arr = doc["devices"].as<JsonArray>();
        for (JsonObject o : arr) {
            IrProfile p = profileFromJson(o);
            if (p.name.isEmpty()) p.name = p.brand + " " + p.type;
            m_saved.push_back(p);
        }
    }

    void saveSavedDevices() {
        if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
        JsonDocument doc;
        JsonArray arr = doc["devices"].to<JsonArray>();
        for (const auto& p : m_saved) {
            JsonObject o = arr.add<JsonObject>();
            profileToJson(p, o);
        }
        String out;
        serializeJsonPretty(doc, out);
        writeTextFile(IRTV_SAVED_FILE, out);
    }

    void buildBrandList() {
        m_brands.clear();
        int tvCount = 0, projCount = 0;
        for (const auto& p : m_catalog) {
            if (p.type == "Proj") projCount++;
            else tvCount++;
        }
        m_brands.push_back({"* Brute-Force ALL", "", BrandFilterItem::KIND_ALL, (int)m_catalog.size()});
        if (tvCount > 0)   m_brands.push_back({"* All TVs", "", BrandFilterItem::KIND_TV, tvCount});
        if (projCount > 0) m_brands.push_back({"* All Projectors", "", BrandFilterItem::KIND_PROJ, projCount});

        for (const auto& p : m_catalog) {
            bool found = false;
            for (auto& b : m_brands) {
                if (b.kind == BrandFilterItem::KIND_BRAND && b.brandMatch == p.brand) {
                    b.count++;
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_brands.push_back({p.brand, p.brand, BrandFilterItem::KIND_BRAND, 1});
            }
        }
        m_brandSel = 0;
        m_brandScroll = 0;
    }

    void sendCode(const IrProfile& p, uint64_t code, const char* label) {
        if (code == 0) {
            showToast("Key N/A for model", 900);
            return;
        }
        irTxSend(p.proto.c_str(), code, p.bits);
        m_lastIrFlashMs = millis();
        strlcpy(m_lastCmdLabel, label, sizeof(m_lastCmdLabel));
    }

    void fireBruteCurrent() {
        if (m_candidates.empty()) return;
        const auto& p = m_catalog[m_candidates[m_bruteIdx]];
        uint64_t code = (m_bruteUseMute && p.mt != 0) ? p.mt : p.pwr;
        sendCode(p, code, (m_bruteUseMute && p.mt != 0) ? "MUTE" : "PWR");
        m_lastBruteSendMs = millis();
    }

    String makeUniqueDeviceName(const String& base) const {
        bool clash = false;
        for (const auto& d : m_saved) {
            if (d.name.equalsIgnoreCase(base)) { clash = true; break; }
        }
        if (!clash) return base;
        for (int n = 2; n < 50; ++n) {
            String cand = base + " " + String(n);
            bool used = false;
            for (const auto& d : m_saved) {
                if (d.name.equalsIgnoreCase(cand)) { used = true; break; }
            }
            if (!used) return cand;
        }
        return base;
    }

    void startAddWizard() {
        if (m_catalog.empty()) {
            showToast("Sync IR TV in App Store", 1500);
            return;
        }
        buildBrandList();
        m_mode = MODE_PICK_BRAND;
    }

    // ---- Input handlers ----
    void handleSavedInput(const InputEvent& ev) {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavUp(ev) && m_savedSel > 0) {
            m_savedSel--;
            if (m_savedSel < m_savedScroll) m_savedScroll = m_savedSel;
        } else if (isNavDown(ev) && m_savedSel < (int)m_saved.size() - 1) {
            m_savedSel++;
            if (m_savedSel >= m_savedScroll + 3) m_savedScroll = m_savedSel - 2;
        } else if (ev.code == CK_CHAR && (ev.ch == 'a' || ev.ch == 'A')) {
            startAddWizard();
        } else if (ev.code == CK_ENTER) {
            if (m_saved.empty()) {
                startAddWizard();
            } else {
                m_activeSavedIdx = m_savedSel;
                m_mode = MODE_REMOTE;
            }
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R') && !m_saved.empty()) {
            m_renameTargetIdx = m_savedSel;
            m_renameInput = m_saved[m_savedSel].name;
            m_mode = MODE_RENAME;
        } else if ((ev.code == CK_DEL || (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D'))) && !m_saved.empty()) {
            String rm = m_saved[m_savedSel].name;
            m_saved.erase(m_saved.begin() + m_savedSel);
            if (m_savedSel >= (int)m_saved.size()) m_savedSel = max(0, (int)m_saved.size() - 1);
            m_savedScroll = constrain(m_savedScroll, 0, max(0, (int)m_saved.size() - 3));
            saveSavedDevices();
            showToast(("Removed " + rm).c_str(), 1200);
        } else if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P') && !m_saved.empty()) {
            sendCode(m_saved[m_savedSel], m_saved[m_savedSel].pwr, "PWR");
        }
    }

    void handlePickBrandInput(const InputEvent& ev) {
        if (isNavBack(ev)) { m_mode = MODE_SAVED; return; }
        if (isNavUp(ev) && m_brandSel > 0) {
            m_brandSel--;
            if (m_brandSel < m_brandScroll) m_brandScroll = m_brandSel;
        } else if (isNavDown(ev) && m_brandSel < (int)m_brands.size() - 1) {
            m_brandSel++;
            if (m_brandSel >= m_brandScroll + 4) m_brandScroll = m_brandSel - 3;
        } else if (ev.code == CK_ENTER && !m_brands.empty()) {
            const auto& bf = m_brands[m_brandSel];
            m_candidates.clear();
            for (int i = 0; i < (int)m_catalog.size(); ++i) {
                const auto& p = m_catalog[i];
                bool ok = (bf.kind == BrandFilterItem::KIND_ALL) ||
                          (bf.kind == BrandFilterItem::KIND_TV && p.type != "Proj") ||
                          (bf.kind == BrandFilterItem::KIND_PROJ && p.type == "Proj") ||
                          (bf.kind == BrandFilterItem::KIND_BRAND && p.brand == bf.brandMatch);
                if (ok) m_candidates.push_back(i);
            }
            if (m_candidates.empty()) return;
            m_bruteIdx = 0;
            m_bruteAuto = true;
            m_mode = MODE_BRUTE;
            fireBruteCurrent();
        }
    }

    void handleBruteInput(const InputEvent& ev) {
        if (isNavBack(ev)) {
            m_bruteAuto = false;
            m_mode = MODE_PICK_BRAND;
            return;
        }
        if (m_candidates.empty()) return;

        if (ev.code == CK_ENTER) {
            m_bruteAuto = false;
            m_confirmDev = m_catalog[m_candidates[m_bruteIdx]];
            m_confirmDev.name = makeUniqueDeviceName(m_confirmDev.brand + " " + m_confirmDev.type);
            m_mode = MODE_CONFIRM;
        } else if (ev.code == CK_CHAR && ev.ch == ' ') {
            m_bruteAuto = !m_bruteAuto;
            m_lastBruteSendMs = millis();
        } else if (isNavLeft(ev)) {
            m_bruteAuto = false;
            m_bruteIdx = (m_bruteIdx - 1 + (int)m_candidates.size()) % (int)m_candidates.size();
            fireBruteCurrent();
        } else if (isNavRight(ev)) {
            m_bruteAuto = false;
            m_bruteIdx = (m_bruteIdx + 1) % (int)m_candidates.size();
            fireBruteCurrent();
        } else if (ev.code == CK_CHAR && (ev.ch == 'f' || ev.ch == 'F')) {
            fireBruteCurrent();
        } else if (ev.code == CK_CHAR && (ev.ch == 't' || ev.ch == 'T')) {
            m_bruteUseMute = !m_bruteUseMute;
            fireBruteCurrent();
        }
    }

    void handleConfirmInput(const InputEvent& ev) {
        if (isNavBack(ev)) {
            m_bruteAuto = false;
            m_mode = MODE_BRUTE;
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P')) {
            sendCode(m_confirmDev, m_confirmDev.pwr, "PWR");
        } else if (ev.code == CK_CHAR && (ev.ch == 'm' || ev.ch == 'M')) {
            sendCode(m_confirmDev, m_confirmDev.mt, "MUTE");
        } else if (isNavUp(ev)) {
            sendCode(m_confirmDev, m_confirmDev.vu, "VOL+");
        } else if (isNavDown(ev)) {
            sendCode(m_confirmDev, m_confirmDev.vd, "VOL-");
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            m_saved.push_back(m_confirmDev);
            saveSavedDevices();
            m_activeSavedIdx = (int)m_saved.size() - 1;
            m_savedSel = m_activeSavedIdx;
            m_renameTargetIdx = m_activeSavedIdx;
            m_renameInput = m_confirmDev.name;
            m_mode = MODE_RENAME;
        } else if (ev.code == CK_ENTER) {
            m_saved.push_back(m_confirmDev);
            saveSavedDevices();
            m_activeSavedIdx = (int)m_saved.size() - 1;
            m_savedSel = m_activeSavedIdx;
            m_mode = MODE_REMOTE;
            showToast((m_confirmDev.name + " saved!").c_str(), 1400);
        }
    }

    void handleRenameInput(const InputEvent& ev) {
        if (isNavBack(ev)) {
            m_mode = (m_activeSavedIdx >= 0 && m_activeSavedIdx < (int)m_saved.size()) ? MODE_REMOTE : MODE_SAVED;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_renameInput.isEmpty()) {
            m_renameInput.remove(m_renameInput.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            m_renameInput.trim();
            if (!m_renameInput.isEmpty() && m_renameTargetIdx >= 0 && m_renameTargetIdx < (int)m_saved.size()) {
                m_saved[m_renameTargetIdx].name = m_renameInput;
                saveSavedDevices();
                showToast("Saved name!", 1100);
            }
            m_mode = (m_activeSavedIdx >= 0 && m_activeSavedIdx < (int)m_saved.size()) ? MODE_REMOTE : MODE_SAVED;
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_renameInput.length() < 24) {
            m_renameInput += ev.ch;
        }
    }

    void handleRemoteInput(const InputEvent& ev) {
        if (isNavBack(ev)) {
            m_activeSavedIdx = -1;
            m_mode = MODE_SAVED;
            return;
        }
        if (m_activeSavedIdx < 0 || m_activeSavedIdx >= (int)m_saved.size()) {
            m_mode = MODE_SAVED;
            return;
        }
        const auto& d = m_saved[m_activeSavedIdx];

        if (ev.code == CK_ENTER) {
            sendCode(d, d.ok, "OK");
        } else if (ev.code == CK_BACKSPACE) {
            sendCode(d, d.bk, "BACK");
        } else if (isNavUp(ev)) {
            sendCode(d, d.vu, "VOL+");
        } else if (isNavDown(ev)) {
            sendCode(d, d.vd, "VOL-");
        } else if (isNavRight(ev)) {
            sendCode(d, d.cu, "CH+");
        } else if (isNavLeft(ev)) {
            sendCode(d, d.cd, "CH-");
        } else if (ev.code == CK_CHAR) {
            char c = ev.ch;
            if (c == 'p' || c == 'P') sendCode(d, d.pwr, "POWER");
            else if (c == 'm' || c == 'M') sendCode(d, d.mt, "MUTE");
            else if (c == '+' || c == '=') sendCode(d, d.vu, "VOL+");
            else if (c == '-' || c == '_') sendCode(d, d.vd, "VOL-");
            else if (c == ']') sendCode(d, d.cu, "CH+");
            else if (c == '[') sendCode(d, d.cd, "CH-");
            else if (c == 'i' || c == 'I') sendCode(d, d.in, "INPUT");
            else if (c == 'o' || c == 'O') sendCode(d, d.mn, "MENU");
            else if (c == 'b' || c == 'B') sendCode(d, d.bk, "BACK");
            else if (c == 'w' || c == 'W') sendCode(d, d.up, "UP");
            else if (c == 's' || c == 'S') sendCode(d, d.dn, "DOWN");
            else if (c == 'a' || c == 'A') sendCode(d, d.lt, "LEFT");
            else if (c == 'd' || c == 'D') sendCode(d, d.rt, "RIGHT");
            else if (c == 'r' || c == 'R') {
                m_renameTargetIdx = m_activeSavedIdx;
                m_renameInput = d.name;
                m_mode = MODE_RENAME;
            }
        }
    }

    // ---- Renderers ----
    void renderIrPulseBadge(M5Canvas& spr, int cx, int cy, const ThemePalette& th, uint16_t acc) {
        bool flashing = (millis() - m_lastIrFlashMs) < 350;
        if (flashing) {
            spr.fillCircle(cx, cy, 4, th.err);
            spr.drawCircle(cx, cy, 7, acc);
            spr.drawCircle(cx, cy, 10, th.textDim);
        } else {
            spr.drawCircle(cx, cy, 4, th.border);
        }
    }

    void renderSaved(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "IR TV Remote");

        // Header bar with [A] Add button
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "Saved (%d) | DB:%d", (int)m_saved.size(), (int)m_catalog.size());
        spr.drawString(hdr, 6, 21);

        spr.fillRoundRect(SCREEN_W - 62, 18, 56, 13, 3, acc);
        spr.setTextColor(th.bg);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("[A] Add", SCREEN_W - 34, 24);
        spr.setTextDatum(TL_DATUM);

        if (m_saved.empty()) {
            spr.fillRoundRect(8, 36, SCREEN_W - 16, 78, 6, th.surface);
            spr.drawRoundRect(8, 36, SCREEN_W - 16, 78, 6, th.border);
            spr.setTextColor(acc);
            spr.drawString("No Saved TVs / Projectors", 18, 46);
            spr.setTextColor(th.textDim);
            spr.drawString("Point Cardputer at TV/Projector", 18, 62);
            spr.drawString("and run Brute-Force IR setup!", 18, 74);
            spr.setTextColor(th.ok);
            spr.drawString("Press [A] or [Enter] to Add TV", 18, 94);
        } else {
            for (int row = 0; row < 3; ++row) {
                int idx = m_savedScroll + row;
                if (idx >= (int)m_saved.size()) break;
                const auto& d = m_saved[idx];
                bool sel = (idx == m_savedSel);
                int y = 35 + row * 28;

                spr.fillRoundRect(6, y, SCREEN_W - 16, 25, 4, sel ? th.surface2 : th.surface);
                spr.drawRoundRect(6, y, SCREEN_W - 16, 25, 4, sel ? acc : th.border);

                // Type badge
                bool isProj = (d.type == "Proj");
                spr.fillRoundRect(10, y + 4, 28, 17, 3, isProj ? th.accent2 : acc);
                spr.setTextColor(th.bg);
                spr.setTextDatum(MC_DATUM);
                spr.drawString(isProj ? "PRJ" : "TV", 24, y + 12);
                spr.setTextDatum(TL_DATUM);

                spr.setTextColor(sel ? acc : th.text);
                String nm = d.name;
                if ((int)nm.length() > 21) nm = nm.substring(0, 20) + "~";
                spr.drawString(nm, 43, y + 4);

                spr.setTextColor(th.textDim);
                String sub = d.brand + " - " + d.proto;
                if ((int)sub.length() > 25) sub = sub.substring(0, 24) + "~";
                spr.drawString(sub, 43, y + 14);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, 35, 80, (int)m_saved.size(), 3, m_savedScroll);
        }

        spr.setTextColor(th.textDim);
        spr.drawString(m_saved.empty() ? "[A/Ent]Add New   [ESC]Home" : "[Ent]Open [A]Add [P]Pwr [R]Ren [D]Del", 6, 124);
    }

    void renderPickBrand(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "Add TV / Projector");
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("Pick Brand (or Brute-Force All):", 8, 20);

        for (int i = 0; i < 4; ++i) {
            int idx = m_brandScroll + i;
            if (idx >= (int)m_brands.size()) break;
            const auto& bf = m_brands[idx];
            bool sel = (idx == m_brandSel);
            int y = 33 + i * 22;

            spr.fillRoundRect(6, y, SCREEN_W - 16, 20, 4, sel ? th.surface2 : th.surface);
            spr.drawRoundRect(6, y, SCREEN_W - 16, 20, 4, sel ? acc : th.border);

            spr.setTextColor(sel ? acc : (bf.kind != BrandFilterItem::KIND_BRAND ? th.ok : th.text));
            spr.drawString(bf.label, 12, y + 6);

            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString(String(bf.count) + (bf.count == 1 ? " code" : " codes"), SCREEN_W - 16, y + 6);
            spr.setTextDatum(TL_DATUM);
        }
        ui::drawScrollbar(spr, SCREEN_W - 6, 33, 86, (int)m_brands.size(), 4, m_brandScroll);

        spr.setTextColor(th.textDim);
        spr.drawString("[Enter] Start Brute-Force   [ESC] Back", 6, 124);
    }

    void renderBrute(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "IR Brute-Force");
        if (m_candidates.empty()) return;

        const auto& p = m_catalog[m_candidates[m_bruteIdx]];
        int total = (int)m_candidates.size();

        // Card container
        spr.fillRoundRect(6, 20, SCREEN_W - 12, 74, 6, th.surface);
        spr.drawRoundRect(6, 20, SCREEN_W - 12, 74, 6, m_bruteAuto ? acc : th.warn);

        // Top row inside card: progress + status + IR LED pulse
        spr.setTextSize(1);
        spr.setTextColor(acc);
        char prog[32];
        snprintf(prog, sizeof(prog), "Code %d / %d  (%s)", m_bruteIdx + 1, total, p.type.c_str());
        spr.drawString(prog, 14, 27);

        renderIrPulseBadge(spr, SCREEN_W - 22, 34, th, acc);

        // Brand & model
        spr.setTextColor(th.text);
        spr.setTextSize(2);
        String br = p.brand;
        if ((int)br.length() > 14) br = br.substring(0, 13) + "~";
        spr.drawString(br, 14, 41);

        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(p.model + " (" + p.proto + ")", 14, 61);

        // State & testing button badge
        spr.setTextColor(m_bruteAuto ? th.ok : th.warn);
        spr.drawString(m_bruteAuto ? "AUTO-BLAST" : "PAUSED", 14, 75);
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(th.text);
        spr.drawString(String("Signal: ") + (m_bruteUseMute ? "MUTE" : "POWER"), SCREEN_W - 14, 75);
        spr.setTextDatum(TL_DATUM);

        // Progress bar
        int bw = SCREEN_W - 20;
        int fw = (bw * (m_bruteIdx + 1)) / max(1, total);
        spr.fillRoundRect(10, 97, bw, 4, 2, th.surface2);
        spr.fillRoundRect(10, 97, max(4, fw), 4, 2, acc);

        // Prominent "IT WORKED!" CTA
        spr.fillRoundRect(14, 104, SCREEN_W - 28, 16, 4, th.ok);
        spr.setTextColor(th.bg);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("[Enter] IT WORKED! Save TV", SCREEN_W / 2, 112);
        spr.setTextDatum(TL_DATUM);

        spr.setTextColor(th.textDim);
        spr.drawString("[Spc]Pause [< / >]Step [T]Pwr/Mute [F]Fire", 6, 124);
    }

    void renderConfirm(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "Confirm TV Code");
        spr.fillRoundRect(8, 22, SCREEN_W - 16, 92, 6, th.surface);
        spr.drawRoundRect(8, 22, SCREEN_W - 16, 92, 6, th.ok);

        spr.setTextSize(1);
        spr.setTextColor(th.ok);
        spr.drawString("Code Matched! Verify Extra Keys:", 16, 30);

        spr.setTextColor(th.text);
        spr.drawString(m_confirmDev.brand + " - " + m_confirmDev.model, 16, 44);

        spr.setTextColor(th.textDim);
        spr.drawString("Name: " + m_confirmDev.name, 16, 56);

        // Quick verify pills
        spr.fillRoundRect(16, 70, 48, 17, 3, th.surface2);
        spr.fillRoundRect(68, 70, 50, 17, 3, th.surface2);
        spr.fillRoundRect(122, 70, 48, 17, 3, th.surface2);
        spr.fillRoundRect(174, 70, 48, 17, 3, th.surface2);
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(th.text);
        spr.drawString("[P]wr", 40, 78);
        spr.drawString("[M]ute", 93, 78);
        spr.drawString("[^]Vol+", 146, 78);
        spr.drawString("[v]Vol-", 198, 78);
        spr.setTextDatum(TL_DATUM);

        if (m_lastCmdLabel[0]) {
            spr.setTextColor(acc);
            spr.drawString(String("Sent: ") + m_lastCmdLabel, 16, 95);
        }
        renderIrPulseBadge(spr, SCREEN_W - 22, 34, th, acc);

        spr.setTextColor(th.textDim);
        spr.drawString("[Enter] Save TV   [R] Rename   [ESC] Back", 6, 124);
    }

    void renderRename(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "Name Device");
        spr.fillRoundRect(12, 28, SCREEN_W - 24, 74, 6, th.surface);
        spr.drawRoundRect(12, 28, SCREEN_W - 24, 74, 6, acc);
        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("Device Name:", 22, 38);
        spr.fillRoundRect(22, 54, SCREEN_W - 44, 20, 3, th.surface2);
        spr.drawRoundRect(22, 54, SCREEN_W - 44, 20, 3, acc);
        spr.setTextColor(th.text);
        spr.drawString(m_renameInput + "_", 27, 60);
        spr.setTextColor(th.textDim);
        spr.drawString("Enter: Save   ESC: Cancel", 22, 84);
    }

    void renderRemote(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        if (m_activeSavedIdx < 0 || m_activeSavedIdx >= (int)m_saved.size()) return;
        const auto& d = m_saved[m_activeSavedIdx];
        ui::drawStatusBar(spr, d.name.c_str());

        // Top info strip
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(d.brand + " (" + d.proto + ")", 8, 19);
        if (m_lastCmdLabel[0]) {
            spr.setTextDatum(TR_DATUM);
            spr.setTextColor((millis() - m_lastIrFlashMs < 350) ? th.err : acc);
            spr.drawString(String("TX: ") + m_lastCmdLabel, SCREEN_W - 28, 19);
            spr.setTextDatum(TL_DATUM);
        }
        renderIrPulseBadge(spr, SCREEN_W - 15, 23, th, acc);

        // Left & Center Remote Buttons
        auto drawBtn = [&](int x, int y, int w, int h, const char* lbl, uint16_t bgCol, uint16_t txtCol) {
            spr.fillRoundRect(x, y, w, h, 4, bgCol);
            spr.drawRoundRect(x, y, w, h, 4, th.border);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(txtCol);
            spr.drawString(lbl, x + w / 2, y + h / 2);
            spr.setTextDatum(TL_DATUM);
        };

        // Row 1: Power, Mute, Input
        drawBtn(6,  32, 46, 22, "[P]PWR",  th.err,     th.bg);
        drawBtn(56, 32, 46, 22, "[M]MUTE", th.surface,  th.warn);
        drawBtn(106,32, 46, 22, "[I]INP",  th.surface,  th.text);

        // Vol & Ch rockers
        drawBtn(6,  58, 46, 26, "^ VOL+", th.surface2, acc);
        drawBtn(6,  88, 46, 26, "v VOL-", th.surface2, th.text);

        drawBtn(56, 58, 46, 26, "> CH+",  th.surface2, acc);
        drawBtn(56, 88, 46, 26, "< CH-",  th.surface2, th.text);

        drawBtn(106,58, 46, 26, "[O]MENU", th.surface, th.text);
        drawBtn(106,88, 46, 26, "[B]BACK", th.surface, th.text);

        // Right panel: D-Pad + OK (W/A/S/D + Enter)
        spr.fillRoundRect(158, 32, 76, 82, 6, th.surface);
        spr.drawRoundRect(158, 32, 76, 82, 6, th.border);
        drawBtn(181, 36, 30, 20, "W ^", th.surface2, th.text);
        drawBtn(162, 61, 20, 22, "A",   th.surface2, th.text);
        drawBtn(184, 61, 24, 22, "OK",  acc,         th.bg);
        drawBtn(210, 61, 20, 22, "D",   th.surface2, th.text);
        drawBtn(181, 88, 30, 20, "S v", th.surface2, th.text);

        spr.setTextColor(th.textDim);
        spr.drawString("Arrows:Vol/Ch  Ent:OK  R:Rename  ESC:List", 6, 124);
    }
};

} // namespace

App* createIrTvApp() { return new IrTvApp(); }
