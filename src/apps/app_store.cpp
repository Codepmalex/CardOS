#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include "../app_installer.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <vector>

extern void switchApp(AppId id);
extern M5Canvas* getScreenCanvas();

namespace {

struct StoreItem {
    String id;
    String name;
    String category;
    String desc;
    String version;
    String pkgUrl;
};

static const char* CAT_FILTERS[4] = {"All", "Utilities", "Crypto", "Games"};

class StoreApp : public App {
public:
    AppId id() const override { return APP_STORE; }
    const char* name() const override { return "App Store"; }

    void onEnter() override {
        m_tab = 0;
        m_editingUrl = false;
        m_catIdx = 0;
        m_storeSel = 0;
        m_storeScroll = 0;
        m_bins = listSdBinApps();
        m_binSel = 0;
        m_binScroll = 0;

        loadCachedCatalog();
        if (g_sys.wifiConnected) {
            fetchCatalog();
        } else if (m_catalog.empty()) {
            loadDefaultCatalogMetadata();
            strlcpy(m_statusMsg, "Offline (connect Wi-Fi to download)", sizeof(m_statusMsg));
        }
        rebuildFiltered();
    }

    void onExit() override {
        std::vector<StoreItem>().swap(m_catalog);
        std::vector<int>().swap(m_filtered);
        std::vector<FileEntry>().swap(m_bins);
        m_urlInput = String();
    }

    void onInput(const InputEvent& ev) override {
        if (m_editingUrl) {
            handleUrlEditInput(ev);
            return;
        }
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavLeft(ev)) {
            m_tab = 0;
            return;
        }
        if (isNavRight(ev)) {
            m_tab = 1;
            return;
        }

        if (m_tab == 0) {
            handleStoreInput(ev);
        } else {
            handleBinsInput(ev);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        if (m_editingUrl) {
            renderUrlEdit(spr, th, acc);
            ui::drawToastOverlay(spr);
            return;
        }

        ui::drawStatusBar(spr, "App Store");

        // Top tabs (y: 18..31)
        spr.fillRoundRect(6, 18, 64, 13, 3, m_tab == 0 ? acc : th.surface);
        spr.setTextSize(1);
        spr.setTextColor(m_tab == 0 ? th.bg : th.textDim);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("Store", 38, 24);

        spr.fillRoundRect(74, 18, 76, 13, 3, m_tab == 1 ? acc : th.surface);
        spr.setTextColor(m_tab == 1 ? th.bg : th.textDim);
        spr.drawString("Other Apps", 112, 24);

        if (m_tab == 0) {
            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(acc);
            spr.drawString(String("[C]") + CAT_FILTERS[m_catIdx], SCREEN_W - 6, 21);
        }
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            renderStoreTab(spr, th, acc);
        } else {
            renderBinsTab(spr, th, acc);
        }

        ui::drawToastOverlay(spr);
    }

private:
    int m_tab = 0; // 0 = GitHub Store, 1 = Other Apps (.bin)
    int m_catIdx = 0;
    std::vector<StoreItem> m_catalog;
    std::vector<int> m_filtered;
    int m_storeSel = 0;
    int m_storeScroll = 0;
    char m_statusMsg[48] = "";

    bool m_editingUrl = false;
    String m_urlInput;

    std::vector<FileEntry> m_bins;
    int m_binSel = 0;
    int m_binScroll = 0;

    uint32_t m_animPhase = 0;

    void drawStoreProgress(const char* title, const char* stage, int pct) {
        M5Canvas* spr = getScreenCanvas();
        if (!spr) return;
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        pct = constrain(pct, 0, 100);
        m_animPhase++;

        // Draw centered modal overlay card
        int bx = 18, by = 28, bw = SCREEN_W - 36, bh = 78;
        spr->fillRoundRect(bx, by, bw, bh, 7, th.surface);
        spr->drawRoundRect(bx, by, bw, bh, 7, acc);

        // Animated rotating spinner + download arrow icon
        int cx = bx + 20, cy = by + 23;
        spr->drawCircle(cx, cy, 10, th.surface2);
        int startDeg = (int)((m_animPhase * 32) % 360);
        spr->drawArc(cx, cy, 10, 8, startDeg, startDeg + 115, acc);
        spr->drawFastVLine(cx, cy - 4, 7, th.text);
        spr->drawLine(cx - 3, cy + 1, cx, cy + 4, th.text);
        spr->drawLine(cx + 3, cy + 1, cx, cy + 4, th.text);

        spr->setTextSize(1);
        spr->setTextColor(acc);
        spr->drawString(title, bx + 36, by + 13);
        spr->setTextColor(th.textDim);
        String st = stage;
        if ((int)st.length() > 26) st = st.substring(0, 25) + "~";
        spr->drawString(st, bx + 36, by + 26);

        // Smooth progress bar
        int barX = bx + 12, barY = by + 48, barW = bw - 24, barH = 10;
        spr->fillRoundRect(barX, barY, barW, barH, 4, th.surface2);
        int fillW = max(6, (barW * pct) / 100);
        spr->fillRoundRect(barX, barY, fillW, barH, 4, acc);

        char pctBuf[12];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
        spr->setTextDatum(TC_DATUM);
        spr->setTextColor(th.text);
        spr->drawString(pctBuf, bx + bw / 2, barY + 14);
        spr->setTextDatum(TL_DATUM);

        spr->pushSprite(0, 0);
    }

    bool httpGetText(const String& url, String& outBody, int& httpCodeOut,
                     const char* progTitle = nullptr, const char* progStage = nullptr,
                     int startPct = 0, int endPct = 100) {
        httpCodeOut = 0;
        outBody = "";
        if (progTitle) drawStoreProgress(progTitle, progStage ? progStage : "Connecting...", startPct);

        HTTPClient http;
        http.setConnectTimeout(4500);
        http.setTimeout(10000);

        WiFiClientSecure sec;
        WiFiClient cli;
        bool begun = false;
        if (url.startsWith("https://")) {
            sec.setInsecure();
            begun = http.begin(sec, url);
        } else {
            begun = http.begin(cli, url);
        }
        if (!begun) return false;

        httpCodeOut = http.GET();
        if (httpCodeOut == 200) {
            int totalLen = http.getSize();
            WiFiClient* stream = http.getStreamPtr();
            if (totalLen > 0) outBody.reserve((size_t)totalLen + 16);

            uint8_t buf[512];
            int got = 0;
            uint32_t lastDrawMs = millis();
            uint32_t lastDataMs = millis();
            int curPct = startPct + 5;
            if (progTitle) drawStoreProgress(progTitle, progStage ? progStage : "Downloading...", curPct);

            while (http.connected() || (stream && stream->available() > 0)) {
                int avail = stream ? stream->available() : 0;
                if (avail > 0) {
                    int toRead = min(avail, (int)sizeof(buf));
                    int n = stream->readBytes(buf, toRead);
                    if (n > 0) {
                        outBody.concat((const char*)buf, n);
                        got += n;
                        lastDataMs = millis();
                    }
                    uint32_t now = millis();
                    if (progTitle && (now - lastDrawMs >= 90 || (totalLen > 0 && got >= totalLen))) {
                        lastDrawMs = now;
                        if (totalLen > 0) {
                            curPct = startPct + ((endPct - startPct) * got) / totalLen;
                        } else {
                            curPct = min(endPct - 2, curPct + 4);
                        }
                        char st[32];
                        if (totalLen > 0) {
                            snprintf(st, sizeof(st), "%d / %d KB", (got + 512) / 1024, (totalLen + 512) / 1024);
                        } else {
                            snprintf(st, sizeof(st), "Downloaded %d bytes", got);
                        }
                        drawStoreProgress(progTitle, st, curPct);
                    }
                    if (totalLen > 0 && got >= totalLen) break;
                } else {
                    if (millis() - lastDataMs > 5000) break;
                    delay(2);
                }
            }
            if (progTitle) drawStoreProgress(progTitle, "Done!", endPct);
        }
        http.end();
        return httpCodeOut == 200;
    }

    String resolvePkgUrl(const String& rawPkg) const {
        if (rawPkg.startsWith("http://") || rawPkg.startsWith("https://")) return rawPkg;
        String base = g_settings.storeUrl;
        int sl = base.lastIndexOf('/');
        String dir = (sl > 8) ? base.substring(0, sl + 1) : base;
        if (rawPkg.startsWith("/")) return dir + rawPkg.substring(1);
        return dir + rawPkg;
    }

    void loadDefaultCatalogMetadata() {
        m_catalog.clear();
        m_catalog.push_back({"weather", "Weather", "Utilities", "Live 3-tab forecast & rain alerts", "1.2", "apps/weather.json"});
        m_catalog.push_back({"miner",   "Miner",   "Crypto",    "Kryptex pool stats & alerts",       "1.1", "apps/miner.json"});
        m_catalog.push_back({"flappy",  "Flappy",  "Games",     "Classic arcade Flappy Bird",        "1.0", "apps/flappy.json"});
    }

    bool parseCatalogJson(const String& jsonTxt) {
        JsonDocument doc;
        if (deserializeJson(doc, jsonTxt) != DeserializationError::Ok) return false;
        JsonArray arr = doc["apps"];
        if (arr.isNull()) return false;
        m_catalog.clear();
        for (JsonObject a : arr) {
            if ((int)m_catalog.size() >= MAX_STORE_CATALOG) break;
            StoreItem it;
            it.id       = String(a["id"] | "");
            it.name     = String(a["name"] | it.id.c_str());
            it.category = String(a["category"] | "Other");
            it.desc     = String(a["desc"] | "");
            it.version  = String(a["version"] | "1.0");
            it.pkgUrl   = String(a["pkg"] | ("apps/" + it.id + ".json").c_str());
            if (!it.id.isEmpty()) m_catalog.push_back(it);
        }
        return !m_catalog.empty();
    }

    void loadCachedCatalog() {
        if ((g_sys.sdMounted || g_sys.spiffsMounted) && activeFS().exists("/store/catalog.json")) {
            String raw = readTextFile("/store/catalog.json", 8192);
            if (parseCatalogJson(raw)) {
                strlcpy(m_statusMsg, "Cached GitHub catalog", sizeof(m_statusMsg));
                return;
            }
        }
        m_catalog.clear();
    }

    void fetchCatalog() {
        if (!g_sys.wifiConnected) {
            showToast("Connect Wi-Fi first!", 1500);
            return;
        }
        String body;
        int code = 0;
        if (httpGetText(g_settings.storeUrl, body, code, "Syncing GitHub", "Fetching catalog.json...", 15, 95) &&
            parseCatalogJson(body)) {
            drawStoreProgress("Syncing GitHub", "Saving catalog...", 100);
            ensureDir("/store");
            writeTextFile("/store/catalog.json", body);
            strlcpy(m_statusMsg, "Synced from GitHub", sizeof(m_statusMsg));
            rebuildFiltered();
        } else {
            if (m_catalog.empty()) loadDefaultCatalogMetadata();
            snprintf(m_statusMsg, sizeof(m_statusMsg), "GitHub HTTP %d (using list)", code);
            rebuildFiltered();
        }
    }

    void rebuildFiltered() {
        m_filtered.clear();
        const char* want = CAT_FILTERS[m_catIdx];
        for (int i = 0; i < (int)m_catalog.size(); ++i) {
            if (m_catIdx == 0 || m_catalog[i].category.equalsIgnoreCase(want)) {
                m_filtered.push_back(i);
            }
        }
        if (m_storeSel >= (int)m_filtered.size()) m_storeSel = max(0, (int)m_filtered.size() - 1);
        m_storeScroll = constrain(m_storeScroll, 0, max(0, (int)m_filtered.size() - 3));
    }

    void downloadAndInstall(const StoreItem& it) {
        if (!g_sys.wifiConnected) {
            showToast("Wi-Fi required to download", 1600);
            return;
        }
        String pkgUrl = resolvePkgUrl(it.pkgUrl);
        String body;
        int code = 0;
        String title = "Installing " + it.name;
        if (!httpGetText(pkgUrl, body, code, title.c_str(), "Fetching manifest...", 10, 40)) {
            snprintf(m_statusMsg, sizeof(m_statusMsg), "DL HTTP %d: push GitHub?", code);
            showToast(("Download err HTTP " + String(code)).c_str(), 2000);
            return;
        }

        // If manifest specifies a downloadable Lua script (and optional companion data file),
        // fetch them genuinely from GitHub
        JsonDocument doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            String scriptRel = String(doc["script"] | "");
            String dataRel   = String(doc["data"] | "");
            if (!scriptRel.isEmpty()) {
                String scriptUrl = resolvePkgUrl(scriptRel);
                String scriptBody;
                int sCode = 0;
                int scriptEndPct = dataRel.isEmpty() ? 92 : 70;
                if (!httpGetText(scriptUrl, scriptBody, sCode, title.c_str(), "Downloading app script...", 35, scriptEndPct) ||
                    scriptBody.isEmpty()) {
                    snprintf(m_statusMsg, sizeof(m_statusMsg), "Script HTTP %d", sCode);
                    showToast(("Script err HTTP " + String(sCode)).c_str(), 2000);
                    return;
                }
                drawStoreProgress(title.c_str(), "Writing script to SD...", scriptEndPct + 2);
                ensureDir("/store");
                if (!writeTextFile(("/store/" + it.id + ".lua").c_str(), scriptBody)) {
                    showToast("SD write failed (.lua)", 1800);
                    return;
                }
            }
            if (!dataRel.isEmpty()) {
                String dataUrl = resolvePkgUrl(dataRel);
                int sl = dataRel.lastIndexOf('/');
                String baseName = (sl >= 0) ? dataRel.substring(sl + 1) : dataRel;
                String dataBody;
                int dCode = 0;
                if (!httpGetText(dataUrl, dataBody, dCode, title.c_str(), "Downloading app data...", 72, 94) ||
                    dataBody.isEmpty()) {
                    snprintf(m_statusMsg, sizeof(m_statusMsg), "Data HTTP %d", dCode);
                    showToast(("Data err HTTP " + String(dCode)).c_str(), 2000);
                    return;
                }
                drawStoreProgress(title.c_str(), "Writing data to SD...", 96);
                ensureDir("/store");
                if (!writeTextFile(("/store/" + baseName).c_str(), dataBody)) {
                    showToast("SD write failed (data)", 1800);
                    return;
                }
            }
        }

        drawStoreProgress(title.c_str(), "Registering on Home...", 98);
        ensureDir("/store");
        writeTextFile(("/store/" + it.id + ".json").c_str(), body);
        installStoreAppKey(it.id);
        drawStoreProgress(title.c_str(), "Installed!", 100);
        delay(200);
        showToast((it.name + " added to Home!").c_str(), 1800);
    }

    void handleStoreInput(const InputEvent& ev) {
        if (isNavUp(ev) && m_storeSel > 0) {
            m_storeSel--;
            if (m_storeSel < m_storeScroll) m_storeScroll = m_storeSel;
        } else if (isNavDown(ev) && m_storeSel < (int)m_filtered.size() - 1) {
            m_storeSel++;
            if (m_storeSel >= m_storeScroll + 3) m_storeScroll = m_storeSel - 2;
        } else if (ev.code == CK_CHAR && (ev.ch == 'c' || ev.ch == 'C')) {
            m_catIdx = (m_catIdx + 1) % 4;
            rebuildFiltered();
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            fetchCatalog();
            showToast("Store refreshed", 1000);
        } else if (ev.code == CK_CHAR && (ev.ch == 'u' || ev.ch == 'U')) {
            m_urlInput = g_settings.storeUrl;
            m_editingUrl = true;
        } else if (ev.code == CK_ENTER && !m_filtered.empty()) {
            const auto& it = m_catalog[m_filtered[m_storeSel]];
            if (isStoreAppInstalled(it.id)) {
                if (isLuaStoreAppInstalled(it.id)) {
                    launchLuaApp(it.id);
                } else {
                    AppId aid = appIdFromKey(it.id);
                    if (aid != APP_COUNT) switchApp(aid);
                }
            } else {
                downloadAndInstall(it);
            }
        } else if (ev.code == CK_CHAR && (ev.ch == 'x' || ev.ch == 'X') && !m_filtered.empty()) {
            const auto& it = m_catalog[m_filtered[m_storeSel]];
            if (isStoreAppInstalled(it.id)) {
                uninstallStoreAppKey(it.id);
                showToast((it.name + " removed").c_str(), 1400);
            }
        }
    }

    void handleBinsInput(const InputEvent& ev) {
        if (isNavUp(ev) && m_binSel > 0) {
            m_binSel--;
            if (m_binSel < m_binScroll) m_binScroll = m_binSel;
        } else if (isNavDown(ev) && m_binSel < (int)m_bins.size() - 1) {
            m_binSel++;
            if (m_binSel >= m_binScroll + 4) m_binScroll = m_binSel - 3;
        } else if (ev.code == CK_ENTER && !m_bins.empty()) {
            const auto& b = m_bins[m_binSel];
            installAndBootBinInteractive(b.path, b.name);
        } else if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P') && !m_bins.empty()) {
            bool pinned = toggleBinPin(m_bins[m_binSel].path);
            showToast(pinned ? "Pinned to Home!" : "Unpinned", 1400);
        } else if (ev.code == CK_CHAR && (ev.ch == 'f' || ev.ch == 'F') && !m_bins.empty()) {
            g_settings.installedBinName[0] = '\0';
            g_settings.installedBinSize = 0;
            saveSettings();
            installAndBootBinInteractive(m_bins[m_binSel].path, m_bins[m_binSel].name);
        } else if (ev.code == CK_CHAR && (ev.ch == 'u' || ev.ch == 'U')) {
            switchApp(APP_USB_MSC);
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            m_bins = listSdBinApps();
            showToast("Rescanned SD", 1000);
        }
    }

    void handleUrlEditInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_editingUrl = false;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_urlInput.isEmpty()) {
            m_urlInput.remove(m_urlInput.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            m_urlInput.trim();
            if (!m_urlInput.isEmpty()) {
                strlcpy(g_settings.storeUrl, m_urlInput.c_str(), sizeof(g_settings.storeUrl));
                saveSettings();
                m_editingUrl = false;
                fetchCatalog();
                showToast("URL saved!", 1200);
            }
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 33 && ev.ch <= 126 && m_urlInput.length() < 124) {
            m_urlInput += ev.ch;
        }
    }

    void renderStoreTab(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(m_statusMsg[0] ? m_statusMsg : "GitHub Store", 8, 34);

        if (m_filtered.empty()) {
            spr.setTextColor(th.textDim);
            spr.drawString("No apps in category. Press [C]/[R].", 10, 62);
        } else {
            for (int row = 0; row < 3; ++row) {
                int fp = m_storeScroll + row;
                if (fp >= (int)m_filtered.size()) break;
                const auto& it = m_catalog[m_filtered[fp]];
                bool sel = (fp == m_storeSel);
                bool inst = isStoreAppInstalled(it.id);

                int y = 45 + row * 25;
                spr.fillRoundRect(6, y, SCREEN_W - 16, 23, 4, sel ? th.surface2 : th.surface);
                spr.drawRoundRect(6, y, SCREEN_W - 16, 23, 4, sel ? acc : th.border);

                spr.setTextColor(sel ? acc : th.text);
                spr.drawString(it.name, 11, y + 3);

                spr.setTextColor(th.textDim);
                String d = it.desc;
                if ((int)d.length() > 30) d = d.substring(0, 29) + "~";
                spr.drawString(d, 11, y + 13);

                spr.setTextDatum(TR_DATUM);
                spr.setTextColor(inst ? th.ok : acc);
                spr.drawString(inst ? "OPEN" : ("[GET " + it.category.substring(0, 4) + "]"), SCREEN_W - 14, y + 3);
                spr.setTextDatum(TL_DATUM);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, 45, 74, (int)m_filtered.size(), 3, m_storeScroll);
        }
        spr.setTextColor(th.textDim);
        spr.drawString("[Ent]Get/Open [X]Rem [R]Sync [U]URL", 6, 125);
    }

    void renderBinsTab(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.setTextSize(1);
        if (m_bins.empty()) {
            spr.fillRoundRect(8, 36, SCREEN_W - 16, 74, 5, th.surface);
            spr.drawRoundRect(8, 36, SCREEN_W - 16, 74, 5, th.border);
            spr.setTextColor(th.warn);
            spr.drawString("No .bin Apps in /apps/", 16, 46);
            spr.setTextColor(th.textDim);
            spr.drawString("Drop .bin apps into SD /apps/.", 16, 64);
            spr.setTextColor(acc);
            spr.drawString("[U] USB Disk   [R] Rescan SD", 16, 86);
            return;
        }

        for (int i = 0; i < 4; ++i) {
            int idx = m_binScroll + i;
            if (idx >= (int)m_bins.size()) break;
            int y = 35 + i * 21;
            bool sel = (idx == m_binSel);
            const auto& b = m_bins[idx];
            bool installed = isBinAlreadyInstalled(b.path.c_str(), b.size);
            bool pinned = isBinPinned(b.path);

            spr.fillRoundRect(6, y, SCREEN_W - 16, 19, 4, sel ? th.surface2 : th.surface);
            spr.drawRoundRect(6, y, SCREEN_W - 16, 19, 4, sel ? acc : th.border);

            spr.setTextColor(sel ? acc : th.text);
            String nm = String(pinned ? "* " : "") + b.name;
            if ((int)nm.length() > 21) nm = nm.substring(0, 20) + "~";
            spr.drawString(nm, 11, y + 5);

            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(installed ? th.ok : th.textDim);
            spr.drawString(installed ? "READY" : formatBytes(b.size), SCREEN_W - 14, y + 5);
            spr.setTextDatum(TL_DATUM);
        }
        ui::drawScrollbar(spr, SCREEN_W - 6, 35, 82, (int)m_bins.size(), 4, m_binScroll);

        spr.setTextColor(th.textDim);
        spr.drawString("[Ent]Run [P]Pin [F]Flash [U]USB [R]Scan", 6, 125);
    }

    void renderUrlEdit(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "Store GitHub URL");
        spr.drawRoundRect(8, 26, SCREEN_W - 16, 78, 5, acc);
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("Raw catalog.json URL:", 16, 36);
        spr.fillRoundRect(16, 52, SCREEN_W - 32, 20, 3, th.surface2);
        spr.drawRoundRect(16, 52, SCREEN_W - 32, 20, 3, acc);
        spr.setTextColor(th.text);
        String disp = m_urlInput + "_";
        if ((int)disp.length() > 32) disp = "..." + disp.substring(disp.length() - 29);
        spr.drawString(disp, 20, 58);
        spr.setTextColor(th.textDim);
        spr.drawString("Enter: Save & Sync   ESC: Cancel", 16, 84);
    }
};

} // namespace

App* createStoreApp() { return new StoreApp(); }
