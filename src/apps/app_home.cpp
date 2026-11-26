#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include "../app_installer.h"
#include "../net_weather.h"
#include <time.h>
#include <vector>

extern void switchApp(AppId id);
extern const char* getAppName(AppId id);

namespace {

struct HomeTile {
    bool isBin = false;
    bool isLua = false;
    AppId stockId = APP_HOME;
    String label;
    String luaId;
    String binPath;
    uint32_t binSize = 0;
};

class HomeApp : public App {
public:
    AppId id() const override { return APP_HOME; }
    const char* name() const override { return "Home"; }

    void onEnter() override {
        refreshTiles();
        m_scrollY = m_scrollRow * 37.0f;
        if (g_sys.focusSearchOnHome) {
            g_sys.focusSearchOnHome = false;
            m_searchFocused = true;
        }
    }

    void onExit() override {
        std::vector<SearchMatch>().swap(m_results);
        m_searchQuery = String();
        m_searchFocused = false;
    }

    void onTick() override {
        float targetY = m_scrollRow * 37.0f;
        float diff = targetY - m_scrollY;
        if (fabsf(diff) < 0.8f) {
            m_scrollY = targetY;
        } else {
            m_scrollY += diff * 0.34f;
        }
    }

    void setSearchQuery(const String& q) {
        m_searchFocused = true;
        m_searchQuery = q;
        runSearch();
    }

    void onInput(const InputEvent& ev) override {
        if (m_searchFocused) {
            handleSearchInput(ev);
            return;
        }

        // Grid navigation
        int total = (int)m_tiles.size();
        if (total == 0) return;

        if (isNavLeft(ev)) {
            if (m_selIdx > 0) m_selIdx--;
        } else if (isNavRight(ev)) {
            if (m_selIdx < total - 1) m_selIdx++;
        } else if (isNavUp(ev)) {
            if (m_selIdx >= 4) {
                m_selIdx -= 4;
            } else {
                // Focus search bar
                m_searchFocused = true;
                runSearch();
            }
        } else if (isNavDown(ev)) {
            if (m_selIdx + 4 < total) {
                m_selIdx += 4;
            } else if (m_selIdx < total - 1 && (m_selIdx / 4) < ((total - 1) / 4)) {
                m_selIdx = total - 1;
            }
        } else if (ev.code == CK_TAB || (ev.code == CK_CHAR && ev.ch == ' ')) {
            m_searchFocused = true;
            runSearch();
        } else if (ev.code == CK_ENTER) {
            launchSelectedTile();
        } else if (ev.code == CK_CHAR && (ev.ch == '[' || ev.ch == ']')) {
            int dir = (ev.ch == '[') ? -1 : 1;
            moveSelectedTile(dir);
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            refreshTiles();
            requestWeatherRefresh();
            showToast("Refreshed", 1000);
        } else if (ev.code == CK_CHAR && isalnum(ev.ch)) {
            // Typing a letter/digit jumps right into search!
            m_searchFocused = true;
            m_searchQuery = String(ev.ch);
            runSearch();
        }

        ensureGridVisible();
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        // --- Header (y: 0..38): Time, Day/Date, Weather, Battery/WiFi ---
        spr.fillRect(0, 0, SCREEN_W, 38, th.surface);
        spr.drawFastHLine(0, 37, SCREEN_W, th.border);

        // Time & Date (Left)
        struct tm tmInfo;
        char timeBuf[16] = "--:--";
        char ampmBuf[4] = "";
        char dateBuf[24] = "No NTP Sync";
        if (getSafeLocalTime(tmInfo)) {
            int h = tmInfo.tm_hour;
            if (!g_settings.use24h) {
                strlcpy(ampmBuf, (h >= 12) ? "PM" : "AM", sizeof(ampmBuf));
                h = h % 12;
                if (h == 0) h = 12;
            }
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", h, tmInfo.tm_min);
            static const char* WDAYS[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            static const char* MONS[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
                     WDAYS[tmInfo.tm_wday % 7], MONS[tmInfo.tm_mon % 12], tmInfo.tm_mday);
        }
        spr.setTextSize(2);
        spr.setTextColor(th.text);
        spr.drawString(timeBuf, 6, 4);
        if (ampmBuf[0] != '\0') {
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString(ampmBuf, 6 + (int)strlen(timeBuf) * 12 + 2, 11);
        }

        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(dateBuf, 6, 24);

        // Top-right status icons identical to every other page (without small time)
        ui::drawStatusIcons(spr, false);

        // Configurable Home Widget (y: 20..34, right-aligned)
        int wxEnd = SCREEN_W - 6;
        spr.setTextSize(1);
        switch (g_settings.homeWidget % WIDGET_COUNT) {
            case WIDGET_WEATHER:
                if (g_sys.weather.valid) {
                    String tempStr = ui::formatTemp(g_sys.weather.tempC);
                    String condStr = weatherCodeToShort(g_sys.weather.weatherCode);
                    int condX = wxEnd - (int)condStr.length() * 6;
                    int tempX = condX - 4 - (int)tempStr.length() * 6;
                    ui::drawWeatherIcon(spr, tempX - 9, 27, 6, g_sys.weather.weatherCode, g_sys.weather.isDay);
                    spr.setTextColor(th.text);
                    spr.drawString(tempStr, tempX, 24);
                    spr.setTextColor(th.textDim);
                    spr.drawString(condStr, condX, 24);
                } else {
                    const char* wMsg = g_sys.wifiConnected ? "Sync..." : "Offline";
                    int msgX = wxEnd - (int)strlen(wMsg) * 6;
                    ui::drawWeatherIcon(spr, msgX - 9, 27, 6, 2, true);
                    spr.setTextColor(th.textDim);
                    spr.drawString(wMsg, msgX, 24);
                }
                break;
            case WIDGET_MINER: {
                char mb[32];
                if (strlen(g_settings.minerWallet) == 0) {
                    snprintf(mb, sizeof(mb), "Miner: No Key");
                    spr.setTextColor(th.textDim);
                } else if (g_sys.minerPool.valid) {
                    snprintf(mb, sizeof(mb), "%.0fH/s (%d/%d)", g_sys.minerPool.totalHash30m,
                             g_sys.minerPool.onlineWorkers, g_sys.minerPool.totalWorkers);
                    spr.setTextColor(g_sys.minerPool.onlineWorkers > 0 ? th.ok : th.warn);
                } else {
                    snprintf(mb, sizeof(mb), "Miner: Sync...");
                    spr.setTextColor(th.textDim);
                }
                spr.setTextDatum(TR_DATUM);
                spr.drawString(mb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_SYSINFO: {
                char sb[32];
                snprintf(sb, sizeof(sb), "%uMHz %luKB", (unsigned)getCpuFrequencyMhz(),
                         (unsigned long)(ESP.getFreeHeap() / 1024));
                spr.setTextColor(acc);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(sb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_CALENDAR: {
                char cb[28];
                snprintf(cb, sizeof(cb), "Today: %d event%s", g_sys.todayEventsCount,
                         g_sys.todayEventsCount == 1 ? "" : "s");
                spr.setTextColor(g_sys.todayEventsCount > 0 ? acc : th.textDim);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(cb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_NOTES: {
                char nb[28];
                snprintf(nb, sizeof(nb), "Notes: %d saved", g_sys.notesCount);
                spr.setTextColor(th.textDim);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(nb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_SNAKE: {
                char nb[28];
                snprintf(nb, sizeof(nb), "Snake Best: %u", (unsigned)g_settings.snakeHighScore);
                spr.setTextColor(th.ok);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(nb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_FLAPPY: {
                char nb[28];
                snprintf(nb, sizeof(nb), "Flappy Best: %u", (unsigned)g_settings.flappyHighScore);
                spr.setTextColor(rgb565(250, 204, 21));
                spr.setTextDatum(TR_DATUM);
                spr.drawString(nb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_MUSIC: {
                String mw;
                uint16_t mc = rgb565(148, 163, 184);
                if (g_sys.music.trackName[0] != '\0') {
                    mw = String(g_sys.music.playing ? (g_sys.music.paused ? "|| " : "> ") : "")
                       + g_sys.music.trackName;
                    if (mw.length() > 19) mw = mw.substring(0, 16) + "...";
                    mc = g_sys.music.playing ? rgb565(52, 211, 153) : rgb565(186, 230, 253);
                } else {
                    mw = "Music: Idle";
                }
                spr.setTextColor(mc);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(mw, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_NONE:
            default:
                break;
        }

        // --- Search Bar (y: 41..56) ---
        int sbX = 6, sbY = 41, sbW = SCREEN_W - 12, sbH = 16;
        spr.fillRoundRect(sbX, sbY, sbW, sbH, 4, m_searchFocused ? th.surface2 : th.surface);
        spr.drawRoundRect(sbX, sbY, sbW, sbH, 4, m_searchFocused ? acc : th.border);
        // Magnifier icon
        spr.drawCircle(sbX + 9, sbY + 7, 3, m_searchFocused ? acc : th.textDim);
        spr.drawLine(sbX + 11, sbY + 9, sbX + 14, sbY + 12, m_searchFocused ? acc : th.textDim);

        spr.setTextSize(1);
        if (m_searchQuery.isEmpty()) {
            spr.setTextColor(th.textDim);
            spr.drawString(m_searchFocused ? "Type to search apps/files..." : "Search apps/files (Tab)  [/]Move", sbX + 18, sbY + 4);
        } else {
            spr.setTextColor(th.text);
            String dispQ = m_searchQuery;
            if ((int)dispQ.length() > 30) dispQ = dispQ.substring(dispQ.length() - 30);
            if (m_searchFocused && ((millis() / 400) % 2 == 0)) dispQ += "_";
            spr.drawString(dispQ, sbX + 18, sbY + 4);
        }

        // --- Either Search Dropdown OR 4-Col App Grid (y: 60..133) ---
        if (m_searchFocused && !m_searchQuery.isEmpty()) {
            renderSearchResults(spr);
        } else {
            renderAppGrid(spr);
        }

        ui::drawToastOverlay(spr);
    }

private:
    std::vector<HomeTile> m_tiles;
    int m_selIdx = 0;
    int m_scrollRow = 0;
    float m_scrollY = 0.0f;

    bool m_searchFocused = false;
    String m_searchQuery;
    std::vector<SearchMatch> m_results;
    int m_resIdx = 0;
    int m_resScroll = 0;

    void refreshTiles() {
        m_tiles.clear();
        for (const auto& k : g_layout.order) {
            if (isLuaStoreAppInstalled(k)) {
                HomeTile t;
                t.isLua = true;
                t.stockId = APP_LUA_RUNNER;
                t.luaId = k;
                t.label = getStoreAppDisplayName(k);
                m_tiles.push_back(t);
            } else {
                AppId aid = appIdFromKey(k);
                if (aid != APP_COUNT) {
                    HomeTile t;
                    t.stockId = aid;
                    t.label = getAppName(aid);
                    m_tiles.push_back(t);
                }
            }
        }

        // Also show pinned SD .bin apps + any .bin apps if user hasn't explicitly unpinned
        auto bins = listSdBinApps();
        for (auto& b : bins) {
            if (!isBinPinned(b.path)) continue;
            HomeTile t;
            t.isBin = true;
            t.label = b.name;
            if (t.label.endsWith(".bin") || t.label.endsWith(".BIN")) {
                t.label = t.label.substring(0, t.label.length() - 4);
            }
            int dash = t.label.indexOf('-');
            if (dash > 2 && t.label.length() > 8) {
                t.label = t.label.substring(0, dash);
            }
            t.binPath = b.path;
            t.binSize = b.size;
            m_tiles.push_back(t);
        }
        if (m_selIdx >= (int)m_tiles.size()) m_selIdx = 0;
        ensureGridVisible();
    }

    void moveSelectedTile(int dir) {
        if (m_tiles.empty()) return;
        int stockCount = (int)g_layout.order.size();
        if (m_selIdx < stockCount) {
            int target = m_selIdx + dir;
            if (target >= 0 && target < stockCount) {
                std::swap(g_layout.order[m_selIdx], g_layout.order[target]);
                m_selIdx = target;
                saveHomeLayout();
                refreshTiles();
                showToast("Order saved", 900);
            }
        } else {
            int pIdx = m_selIdx - stockCount;
            int pTarget = pIdx + dir;
            if (pIdx >= 0 && pTarget >= 0 && pTarget < (int)g_layout.pinnedBins.size()) {
                std::swap(g_layout.pinnedBins[pIdx], g_layout.pinnedBins[pTarget]);
                m_selIdx = stockCount + pTarget;
                saveHomeLayout();
                refreshTiles();
                showToast("Order saved", 900);
            }
        }
    }

    void ensureGridVisible() {
        int row = m_selIdx / 4;
        if (row < m_scrollRow) m_scrollRow = row;
        if (row >= m_scrollRow + 2) m_scrollRow = row - 1;
    }

    void launchSelectedTile() {
        if (m_selIdx < 0 || m_selIdx >= (int)m_tiles.size()) return;
        const auto& t = m_tiles[m_selIdx];
        if (t.isLua) {
            launchLuaApp(t.luaId);
        } else if (!t.isBin) {
            switchApp(t.stockId);
        } else {
            installAndBootBinInteractive(t.binPath, t.label);
        }
    }

    void runSearch() {
        m_results.clear();
        m_resIdx = 0;
        m_resScroll = 0;
        String q = m_searchQuery;
        q.trim();
        q.toLowerCase();
        if (q.isEmpty()) return;

        // 1. Match stock apps
        for (int i = 1; i < APP_COUNT; ++i) {
            String nm = getAppName((AppId)i);
            String nmLow = nm;
            nmLow.toLowerCase();
            if (nmLow.indexOf(q) >= 0) {
                SearchMatch m;
                m.kind = SearchMatch::MATCH_STOCK_APP;
                m.title = nm;
                m.subtitle = "Stock App";
                m.stockAppId = (uint8_t)i;
                m_results.push_back(m);
            }
        }
        // 2. Match SD files, folders, and .bin apps
        auto fsMatches = searchFilesystem(m_searchQuery, 14);
        for (auto& fm : fsMatches) {
            m_results.push_back(fm);
        }
    }

    void handleSearchInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            if (!m_searchQuery.isEmpty()) {
                m_searchQuery = "";
                m_results.clear();
            } else {
                m_searchFocused = false;
            }
            return;
        }
        if (ev.code == CK_TAB) {
            m_searchFocused = false;
            return;
        }
        if (ev.code == CK_DOWN) {
            if (m_results.empty()) {
                m_searchFocused = false;
            } else if (m_resIdx < (int)m_results.size() - 1) {
                m_resIdx++;
                if (m_resIdx >= m_resScroll + 4) m_resScroll = m_resIdx - 3;
            }
            return;
        }
        if (ev.code == CK_UP) {
            if (m_resIdx > 0) {
                m_resIdx--;
                if (m_resIdx < m_resScroll) m_resScroll = m_resIdx;
            }
            return;
        }
        if (ev.code == CK_BACKSPACE) {
            if (!m_searchQuery.isEmpty()) {
                m_searchQuery.remove(m_searchQuery.length() - 1);
                runSearch();
            } else {
                m_searchFocused = false;
            }
            return;
        }
        if (ev.code == CK_ENTER) {
            if (!m_results.empty() && m_resIdx < (int)m_results.size()) {
                openSearchMatch(m_results[m_resIdx]);
            } else {
                m_searchFocused = false;
            }
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126) {
            if (m_searchQuery.length() < 32) {
                m_searchQuery += ev.ch;
                runSearch();
            }
        }
    }

    void openSearchMatch(const SearchMatch& m) {
        m_searchFocused = false;
        m_searchQuery = "";
        switch (m.kind) {
            case SearchMatch::MATCH_STOCK_APP:
                switchApp((AppId)m.stockAppId);
                break;
            case SearchMatch::MATCH_SD_BIN:
                installAndBootBinInteractive(m.path, m.title);
                break;
            case SearchMatch::MATCH_DIR:
                strlcpy(g_sys.pendingFolderPath, m.path.c_str(), sizeof(g_sys.pendingFolderPath));
                switchApp(APP_FOLDERS);
                break;
            case SearchMatch::MATCH_FILE: {
                String low = m.title;
                low.toLowerCase();
                if (low.endsWith(".txt") || low.endsWith(".md") || low.endsWith(".json") || low.endsWith(".log") || low.endsWith(".csv")) {
                    strlcpy(g_sys.pendingNotePath, m.path.c_str(), sizeof(g_sys.pendingNotePath));
                    g_sys.returnApp = APP_HOME;
                    switchApp(APP_NOTES);
                } else {
                    int sl = m.path.lastIndexOf('/');
                    String parent = (sl > 0) ? m.path.substring(0, sl) : "/";
                    strlcpy(g_sys.pendingFolderPath, parent.c_str(), sizeof(g_sys.pendingFolderPath));
                    switchApp(APP_FOLDERS);
                }
                break;
            }
        }
    }

    void renderAppGrid(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();

        int startY = 60;
        int cellW = 55;
        int cellH = 34;
        int gapX = 3;
        int gapY = 3;
        int startX = 6;

        int totalRows = ((int)m_tiles.size() + 3) / 4;
        spr.setClipRect(0, startY, SCREEN_W - 6, SCREEN_H - startY);
        for (int rowIdx = 0; rowIdx < totalRows; ++rowIdx) {
            int y = startY + rowIdx * (cellH + gapY) - (int)roundf(m_scrollY);
            if (y + cellH < startY || y >= SCREEN_H) continue;
            for (int c = 0; c < 4; ++c) {
                int idx = rowIdx * 4 + c;
                if (idx >= (int)m_tiles.size()) break;

                int x = startX + c * (cellW + gapX);
                bool sel = (!m_searchFocused && idx == m_selIdx);

                spr.fillRoundRect(x, y, cellW, cellH, 5, sel ? th.surface2 : th.surface);
                spr.drawRoundRect(x, y, cellW, cellH, 5, sel ? acc : th.border);

                const auto& t = m_tiles[idx];
                uint16_t iconCol = sel ? acc : (t.isBin ? th.warn : (t.isLua ? acc : th.accent2));
                ui::drawAppIcon(spr, x + cellW / 2, y + 12, t.isLua ? APP_LUA_RUNNER : t.stockId, iconCol, t.isBin);

                String lbl = t.label;
                if ((int)lbl.length() > 8) lbl = lbl.substring(0, 8);
                spr.setTextSize(1);
                spr.setTextColor(sel ? th.text : th.textDim);
                spr.setTextDatum(TC_DATUM);
                spr.drawString(lbl, x + cellW / 2, y + 23);
                spr.setTextDatum(TL_DATUM);
            }
        }
        spr.clearClipRect();
        ui::drawScrollbar(spr, SCREEN_W - 5, startY, 70, totalRows, 2, m_scrollRow);
    }

    void renderSearchResults(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        int y0 = 60;

        if (m_results.empty()) {
            spr.setTextSize(1);
            spr.setTextColor(th.textDim);
            spr.drawString("No matches found.", 12, y0 + 12);
            return;
        }

        for (int i = 0; i < 4; ++i) {
            int idx = m_resScroll + i;
            if (idx >= (int)m_results.size()) break;
            int y = y0 + i * 18;
            bool sel = (idx == m_resIdx);

            spr.fillRoundRect(6, y, SCREEN_W - 14, 16, 3, sel ? th.surface2 : th.surface);
            if (sel) spr.drawRoundRect(6, y, SCREEN_W - 14, 16, 3, acc);

            const auto& m = m_results[idx];
            spr.setTextSize(1);
            spr.setTextColor(sel ? acc : th.text);
            String title = m.title;
            if ((int)title.length() > 16) title = title.substring(0, 15) + "~";
            spr.drawString(title, 11, y + 4);

            spr.setTextColor(th.textDim);
            spr.setTextDatum(TR_DATUM);
            String sub = m.subtitle;
            if ((int)sub.length() > 18) sub = sub.substring(0, 17) + "~";
            spr.drawString(sub, SCREEN_W - 12, y + 4);
            spr.setTextDatum(TL_DATUM);
        }
        ui::drawScrollbar(spr, SCREEN_W - 5, y0, 72, (int)m_results.size(), 4, m_resScroll);
    }
};

} // namespace

static HomeApp* s_homeInstance = nullptr;

App* createHomeApp() {
    if (!s_homeInstance) s_homeInstance = new HomeApp();
    return s_homeInstance;
}

void triggerHomeSearch(const String& q) {
    switchApp(APP_HOME);
    if (s_homeInstance) {
        s_homeInstance->setSearchQuery(q);
    }
}

