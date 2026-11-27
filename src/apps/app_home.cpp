#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../hardware.h"
#include "../bt_manager.h"
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
        m_moveMode = false;
        m_ctrlOpen = false;
        m_ctrlClosing = false;
        m_ctrlAnim = 0.0f;
        closeNotifPane();
        refreshTiles();
        m_scrollY = m_scrollRow * 37.0f;
        m_selAnimX = 6.0f + (m_selIdx % 4) * 58.0f;
        m_selAnimY = (float)((m_selIdx / 4) * 37);
        m_resSelAnimY = 60.0f;
        if (g_sys.focusSearchOnHome) {
            g_sys.focusSearchOnHome = false;
            m_searchFocused = true;
        }
    }

    void onExit() override {
        m_moveMode = false;
        m_ctrlOpen = false;
        m_ctrlClosing = false;
        m_ctrlAnim = 0.0f;
        closeNotifPane();
        std::vector<HomeTile>().swap(m_tiles);
        std::vector<SearchMatch>().swap(m_results);
        m_searchQuery = String();
        m_searchFocused = false;
    }

    void onTick() override {
        m_scrollY = ui::smoothMove(m_scrollY, m_scrollRow * 37.0f, 0.38f);
        float tgtX = 6.0f + (m_selIdx % 4) * 58.0f;
        float tgtY = (float)((m_selIdx / 4) * 37);
        m_selAnimX = ui::smoothMove(m_selAnimX, tgtX, 0.48f);
        m_selAnimY = ui::smoothMove(m_selAnimY, tgtY, 0.48f);
        m_resSelAnimY = ui::smoothMove(m_resSelAnimY, 60.0f + (m_resIdx - m_resScroll) * 18.0f, 0.48f);

        if (m_notifOpen) {
            if (m_notifClosing) {
                m_notifAnim -= 0.34f;
                if (m_notifAnim <= 0.0f) {
                    closeNotifPane();
                }
            } else {
                if (m_notifAnim < 1.0f) m_notifAnim = min(1.0f, m_notifAnim + 0.30f);
                m_notifSelAnimY = ui::smoothMove(m_notifSelAnimY, 57.0f + (m_notifSel - m_notifScroll) * 21.0f, 0.48f);
            }
        }
        if (m_ctrlOpen) {
            if (m_ctrlClosing) {
                m_ctrlAnim -= 0.34f;
                if (m_ctrlAnim <= 0.0f) {
                    m_ctrlAnim = 0.0f;
                    m_ctrlOpen = false;
                    m_ctrlClosing = false;
                }
            } else {
                if (m_ctrlAnim < 1.0f) m_ctrlAnim = min(1.0f, m_ctrlAnim + 0.30f);
                m_ctrlSelAnimY = ui::smoothMove(m_ctrlSelAnimY, 56.0f + (m_ctrlSel - m_ctrlScroll) * 19.0f, 0.48f);
            }
        }
    }

    void setSearchQuery(const String& q) {
        m_searchFocused = true;
        m_searchQuery = q;
        runSearch();
    }

    void onInput(const InputEvent& ev) override {
        if (m_notifOpen) {
            handleNotifInput(ev);
            return;
        }
        if (m_ctrlOpen) {
            handleCtrlInput(ev);
            return;
        }
        if (m_searchFocused) {
            handleSearchInput(ev);
            return;
        }

        // Grid navigation & Move Mode
        int total = (int)m_tiles.size();
        if (total == 0) return;

        if (m_moveMode) {
            if (isNavLeft(ev))       moveSelectedTile(-1);
            else if (isNavRight(ev)) moveSelectedTile(1);
            else if (isNavUp(ev))    moveSelectedTile(-4);
            else if (isNavDown(ev))  moveSelectedTile(4);
            else if (ev.code == CK_ENTER || isNavBack(ev) || (ev.code == CK_CHAR && ev.ch == '\\')) {
                m_moveMode = false;
                showToast("Order saved", 900);
            }
            ensureGridVisible();
            return;
        }

        if (isNavBack(ev) || (ev.code == CK_CHAR && (ev.ch == '!' || ev.ch == '1' || ev.ch == '['))) {
            openNotifPane();
            return;
        } else if (ev.code == CK_CHAR && ev.ch == ']') {
            m_ctrlOpen = true;
            m_ctrlClosing = false;
            m_ctrlAnim = 0.14f;
            m_ctrlSel = 0;
            m_ctrlScroll = 0;
            m_ctrlSelAnimY = 56.0f;
            return;
        } else if (isNavLeft(ev)) {
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
        } else if (ev.code == CK_CHAR && ev.ch == '\\') {
            m_moveMode = true;
            showToast("Move Mode: Arrows move", 1200);
        } else if (ev.code == CK_CHAR && (ev.ch == '-' || ev.ch == '_' || ev.ch == '=' || ev.ch == '+')) {
            int dir = (ev.ch == '-' || ev.ch == '_') ? -1 : 1;
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
            case WIDGET_TASKS: {
                char tb[28];
                if (g_sys.pendingTasksCount > 0) snprintf(tb, sizeof(tb), "Tasks: %d left", g_sys.pendingTasksCount);
                else snprintf(tb, sizeof(tb), "Tasks: All done");
                spr.setTextColor(g_sys.pendingTasksCount > 0 ? acc : th.ok);
                spr.setTextDatum(TR_DATUM);
                spr.drawString(tb, wxEnd, 24);
                spr.setTextDatum(TL_DATUM);
                break;
            }
            case WIDGET_NONE:
            default:
                break;
        }

        // --- Search / Move Bar (y: 41..56) ---
        int sbX = 6, sbY = 41, sbW = SCREEN_W - 12, sbH = 16;
        spr.fillRoundRect(sbX, sbY, sbW, sbH, 4, (m_searchFocused || m_moveMode) ? th.surface2 : th.surface);
        spr.drawRoundRect(sbX, sbY, sbW, sbH, 4, m_moveMode ? th.warn : (m_searchFocused ? acc : th.border));
        // Magnifier icon (or Move badge)
        if (m_moveMode) {
            spr.setTextSize(1);
            spr.setTextColor(th.warn);
            spr.drawString("MOVE MODE: Arrows move, [Ent]Done", sbX + 8, sbY + 4);
        } else {
            spr.drawCircle(sbX + 9, sbY + 7, 3, m_searchFocused ? acc : th.textDim);
            spr.drawLine(sbX + 11, sbY + 9, sbX + 14, sbY + 12, m_searchFocused ? acc : th.textDim);

            spr.setTextSize(1);
            if (m_searchQuery.isEmpty()) {
                if (g_sys.unreadNotifCount > 0) {
                    spr.setTextColor(acc);
                    char hb[42];
                    snprintf(hb, sizeof(hb), "Search(Tab)  [!]Notifs(%u)", (unsigned)min((int)g_sys.unreadNotifCount, 99));
                    spr.drawString(hb, sbX + 18, sbY + 4);
                } else {
                    spr.setTextColor(th.textDim);
                    spr.drawString(m_searchFocused ? "Type to search ([Up]=Notifs)" : "Tab:Search !:Notif ]:Ctrl \\:Move", sbX + 18, sbY + 4);
                }
            } else {
                spr.setTextColor(th.text);
                String dispQ = m_searchQuery;
                if ((int)dispQ.length() > 30) dispQ = dispQ.substring(dispQ.length() - 30);
                if (m_searchFocused && ((millis() / 400) % 2 == 0)) dispQ += "_";
                spr.drawString(dispQ, sbX + 18, sbY + 4);
            }
        }

        // --- Either Notifications Pane, Control Center, Search Dropdown, OR 4-Col App Grid (y: 60..133) ---
        if (m_notifOpen) {
            renderNotifPane(spr);
        } else if (m_ctrlOpen) {
            renderCtrlCenter(spr);
        } else if (m_searchFocused && !m_searchQuery.isEmpty()) {
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
    float m_selAnimX = 6.0f;
    float m_selAnimY = 0.0f;

    bool m_searchFocused = false;
    bool m_moveMode = false;
    String m_searchQuery;
    std::vector<SearchMatch> m_results;
    int m_resIdx = 0;
    int m_resScroll = 0;
    float m_resSelAnimY = 60.0f;

    bool m_notifOpen = false;
    bool m_notifClosing = false;
    float m_notifAnim = 0.0f;
    float m_notifSelAnimY = 57.0f;
    std::vector<NotifRecord> m_notifs;
    int m_notifSel = 0;
    int m_notifScroll = 0;
    bool m_notifDetail = false;
    int m_notifDetailScroll = 0;

    bool m_ctrlOpen = false;
    bool m_ctrlClosing = false;
    float m_ctrlAnim = 0.0f;
    float m_ctrlSelAnimY = 56.0f;
    int m_ctrlSel = 0;
    int m_ctrlScroll = 0;

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

    void moveSelectedTile(int delta) {
        if (m_tiles.empty() || delta == 0) return;
        int stockCount = (int)g_layout.order.size();
        if (m_selIdx < stockCount) {
            int target = constrain(m_selIdx + delta, 0, stockCount - 1);
            if (target != m_selIdx) {
                int step = (target > m_selIdx) ? 1 : -1;
                for (int i = m_selIdx; i != target; i += step) {
                    std::swap(g_layout.order[i], g_layout.order[i + step]);
                }
                m_selIdx = target;
                saveHomeLayout();
                refreshTiles();
                if (!m_moveMode) showToast("Order saved", 900);
            }
        } else {
            int pCount = (int)g_layout.pinnedBins.size();
            int pIdx = m_selIdx - stockCount;
            int pTarget = constrain(pIdx + delta, 0, pCount - 1);
            if (pIdx >= 0 && pIdx < pCount && pTarget != pIdx) {
                int step = (pTarget > pIdx) ? 1 : -1;
                for (int i = pIdx; i != pTarget; i += step) {
                    std::swap(g_layout.pinnedBins[i], g_layout.pinnedBins[i + step]);
                }
                m_selIdx = stockCount + pTarget;
                saveHomeLayout();
                refreshTiles();
                if (!m_moveMode) showToast("Order saved", 900);
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
            if (m_searchQuery.isEmpty() && m_results.empty()) {
                m_searchFocused = false;
                openNotifPane();
                return;
            }
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
                spr.drawRoundRect(x, y, cellW, cellH, 5, th.border);

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
        if (!m_searchFocused && !m_tiles.empty()) {
            int gx = (int)roundf(m_selAnimX);
            int gy = startY + (int)roundf(m_selAnimY - m_scrollY);
            uint16_t bCol = m_moveMode ? th.warn : acc;
            spr.drawRoundRect(gx, gy, cellW, cellH, 5, bCol);
            if (m_moveMode) spr.drawRoundRect(gx + 1, gy + 1, cellW - 2, cellH - 2, 4, th.warn);
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
        spr.drawRoundRect(6, (int)roundf(m_resSelAnimY), SCREEN_W - 14, 16, 3, acc);
        ui::drawScrollbar(spr, SCREEN_W - 5, y0, 72, (int)m_results.size(), 4, m_resScroll);
    }

    static const char* notifTag(uint8_t app) {
        static const char* TAGS[NOTIF_APP_COUNT] = {"CAL", "WTH", "BAT", "SYS", "MNR", "UPD", "PHN"};
        return (app < NOTIF_APP_COUNT) ? TAGS[app] : "SYS";
    }

    uint16_t notifColor(uint8_t app, const ThemePalette& th) const {
        if (app == NOTIF_BATTERY) return th.err;
        if (app == NOTIF_WEATHER || app == NOTIF_UPDATES) return activeAccent();
        if (app == NOTIF_MINER) return th.warn;
        if (app == NOTIF_CALENDAR || app == NOTIF_PHONE) return th.ok;
        return th.accent2;
    }

    void openNotifPane() {
        m_notifOpen = true;
        m_notifClosing = false;
        m_notifAnim = 0.14f;
        m_notifDetail = false;
        m_notifSel = 0;
        m_notifScroll = 0;
        m_notifSelAnimY = 57.0f;
        m_notifDetailScroll = 0;
        m_searchFocused = false;
        g_sys.unreadNotifCount = 0;
        g_sys.ledNotifyUntilMs = 0;
        updateLedEngine();
        m_notifs = loadNotificationsFromSd(30);
    }

    void closeNotifPane() {
        m_notifOpen = false;
        m_notifClosing = false;
        m_notifAnim = 0.0f;
        m_notifDetail = false;
        std::vector<NotifRecord>().swap(m_notifs);
    }

    void handleNotifInput(const InputEvent& ev) {
        if (m_notifDetail) {
            if (isNavUp(ev) && m_notifDetailScroll > 0) m_notifDetailScroll--;
            else if (isNavDown(ev)) m_notifDetailScroll++;
            else if (isNavBack(ev) || ev.code == CK_ENTER ||
                     (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) {
                m_notifDetail = false;
            }
            return;
        }

        if (isNavBack(ev) || (ev.code == CK_CHAR && (ev.ch == '!' || ev.ch == '1' || ev.ch == '['))) {
            m_notifClosing = true;
            return;
        }

        if (m_notifs.empty()) {
            if (ev.code == CK_ENTER) m_notifClosing = true;
            return;
        }

        if (isNavUp(ev) && m_notifSel > 0) {
            m_notifSel--;
            if (m_notifSel < m_notifScroll) m_notifScroll = m_notifSel;
        } else if (isNavDown(ev) && m_notifSel < (int)m_notifs.size() - 1) {
            m_notifSel++;
            if (m_notifSel >= m_notifScroll + 3) m_notifScroll = m_notifSel - 2;
        } else if (ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) {
            m_notifDetail = true;
            m_notifDetailScroll = 0;
        } else if (ev.code == CK_DEL || ev.code == CK_BACKSPACE ||
                   (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D'))) {
            deleteNotificationOnSd(m_notifSel);
            m_notifs.erase(m_notifs.begin() + m_notifSel);
            if (m_notifSel >= (int)m_notifs.size()) m_notifSel = max(0, (int)m_notifs.size() - 1);
            if (m_notifScroll > max(0, (int)m_notifs.size() - 3)) {
                m_notifScroll = max(0, (int)m_notifs.size() - 3);
            }
            showToast("Dismissed", 800);
        } else if (ev.code == CK_CHAR && (ev.ch == 'c' || ev.ch == 'C')) {
            clearNotificationsOnSd();
            std::vector<NotifRecord>().swap(m_notifs);
            m_notifSel = 0;
            m_notifScroll = 0;
            showToast("Cleared all", 900);
        }
    }

    void renderNotifPane(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        int offY = (int)roundf((1.0f - ui::easeOutCubic(m_notifAnim)) * 96.0f);

        // Smooth pull-down shade drawer below header (y: 39..132)
        spr.setClipRect(4, 39, SCREEN_W - 8, SCREEN_H - 42);
        spr.fillRoundRect(4, 39 - offY, SCREEN_W - 8, SCREEN_H - 42, 6, th.surface);
        spr.drawRoundRect(4, 39 - offY, SCREEN_W - 8, SCREEN_H - 42, 6, acc);

        if (m_notifDetail && m_notifSel >= 0 && m_notifSel < (int)m_notifs.size()) {
            const auto& nr = m_notifs[m_notifSel];
            uint16_t nc = notifColor(nr.app, th);
            spr.fillRoundRect(10, 44 - offY, 26, 11, 3, nc);
            spr.setTextSize(1);
            spr.setTextColor(th.bg);
            spr.drawString(notifTag(nr.app), 14, 46 - offY);
            spr.setTextColor(th.textDim);
            spr.setTextDatum(TR_DATUM);
            spr.drawString(nr.timeStr, SCREEN_W - 12, 46 - offY);
            spr.setTextDatum(TL_DATUM);

            int totalL = 0;
            spr.setTextColor(th.text);
            ui::drawWrappedText(spr, nr.msg, 12, 60 - offY, SCREEN_W - 28, 52, 10, m_notifDetailScroll, &totalL);
            if (totalL > 5) {
                m_notifDetailScroll = constrain(m_notifDetailScroll, 0, totalL - 5);
                ui::drawScrollbar(spr, SCREEN_W - 12, 59 - offY, 50, totalL, 5, m_notifDetailScroll);
            } else {
                m_notifDetailScroll = 0;
            }
            spr.setTextColor(acc);
            spr.drawString("[Up/Dn]Scroll   [Esc/Ent]Back", 12, 121 - offY);
            spr.clearClipRect();
            return;
        }

        spr.setTextSize(1);
        spr.setTextColor(acc);
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "Notifs (%d)", (int)m_notifs.size());
        spr.drawString(hdr, 10, 43 - offY);
        spr.setTextColor(th.textDim);
        spr.setTextDatum(TR_DATUM);
        spr.drawString("[C]Clear [Esc]Close", SCREEN_W - 10, 43 - offY);
        spr.setTextDatum(TL_DATUM);
        spr.drawFastHLine(8, 54 - offY, SCREEN_W - 16, th.border);

        if (m_notifs.empty()) {
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString("No notifications saved on SD", SCREEN_W / 2, 86 - offY);
            spr.setTextColor(acc);
            spr.drawString("Press ESC or [1/!] to close", SCREEN_W / 2, 102 - offY);
            spr.setTextDatum(TL_DATUM);
            spr.clearClipRect();
            return;
        }

        int y0 = 57 - offY;
        for (int r = 0; r < 3; ++r) {
            int idx = m_notifScroll + r;
            if (idx >= (int)m_notifs.size()) break;
            const auto& nr = m_notifs[idx];
            int y = y0 + r * 21;
            bool sel = (idx == m_notifSel);

            spr.fillRoundRect(9, y, SCREEN_W - 22, 19, 4, sel ? th.surface2 : th.bg);
            spr.drawRoundRect(9, y, SCREEN_W - 22, 19, 4, th.border);

            uint16_t nc = notifColor(nr.app, th);
            spr.fillRoundRect(13, y + 4, 24, 11, 2, nc);
            spr.setTextColor(th.bg);
            spr.drawString(notifTag(nr.app), 16, y + 6);

            spr.setTextColor(th.textDim);
            spr.setTextDatum(TR_DATUM);
            spr.drawString(nr.timeStr, SCREEN_W - 17, y + 6);
            spr.setTextDatum(TL_DATUM);

            String msg = nr.msg;
            int maxChars = 24;
            if ((int)msg.length() > maxChars) msg = msg.substring(0, maxChars - 2) + "..";
            spr.setTextColor(sel ? th.text : th.textDim);
            spr.drawString(msg, 41, y + 6);
        }
        spr.drawRoundRect(9, (int)roundf(m_notifSelAnimY) - offY, SCREEN_W - 22, 19, 4, acc);
        ui::drawScrollbar(spr, SCREEN_W - 10, y0, 62, (int)m_notifs.size(), 3, m_notifScroll);
        spr.setTextColor(th.textDim);
        spr.drawString("[Ent]Read  [D]Dismiss  [C]Clear", 10, 122 - offY);
        spr.clearClipRect();
    }

    static constexpr int kCtrlCount = 9;

    void handleCtrlInput(const InputEvent& ev) {
        if (isNavBack(ev) || (ev.code == CK_CHAR && (ev.ch == ']' || ev.ch == '\\'))) {
            m_ctrlClosing = true;
            return;
        }
        if (isNavUp(ev) && m_ctrlSel > 0) {
            m_ctrlSel--;
            if (m_ctrlSel < m_ctrlScroll) m_ctrlScroll = m_ctrlSel;
            return;
        }
        if (isNavDown(ev) && m_ctrlSel < kCtrlCount - 1) {
            m_ctrlSel++;
            if (m_ctrlSel >= m_ctrlScroll + 4) m_ctrlScroll = m_ctrlSel - 3;
            return;
        }

        int dir = isNavLeft(ev) ? -1 : ((isNavRight(ev) || ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' ')) ? 1 : 0);
        if (dir == 0) return;

        switch (m_ctrlSel) {
            case 0:
                g_settings.brightness = (uint8_t)constrain((int)g_settings.brightness + dir * 25, 30, 255);
                applyBrightness(g_settings.brightness);
                saveSettings();
                break;
            case 1:
                g_settings.mediaVolume = (uint8_t)constrain((int)g_settings.mediaVolume + dir * 20, 0, 255);
                saveSettings();
                break;
            case 2:
                g_settings.deviceProfile = (uint8_t)((g_settings.deviceProfile + PROF_COUNT + dir) % PROF_COUNT);
                updateLedEngine();
                saveSettings();
                break;
            case 3:
                if (g_sys.wifiConnected || g_sys.wifiConnecting) {
                    requestWifiDisconnect();
                    showToast("Wi-Fi Off", 1000);
                } else {
                    requestWifiConnect();
                    showToast("Wi-Fi Connecting...", 1200);
                }
                break;
            case 4:
                btSetEnabled(!g_settings.btEnabled);
                break;
            case 5:
                g_settings.lowPowerMode = !g_settings.lowPowerMode;
                applyPowerProfile();
                saveSettings();
                break;
            case 6:
                m_ctrlOpen = false;
                m_ctrlClosing = false;
                m_ctrlAnim = 0.0f;
                setFlashlight(true);
                break;
            case 7:
                m_ctrlOpen = false;
                m_ctrlClosing = false;
                m_ctrlAnim = 0.0f;
                enterScreensaver();
                break;
            case 8: {
                bool ok = remountSdCard();
                if (ok) {
                    char sb[28];
                    snprintf(sb, sizeof(sb), "SD OK (%luMB free)", (unsigned long)g_sys.sdFreeMB);
                    showToast(sb, 1400);
                } else {
                    showToast("SD card not detected", 1400);
                }
                break;
            }
            default:
                break;
        }
    }

    String ctrlValueText(int idx) const {
        switch (idx) {
            case 0: return String((g_settings.brightness * 100) / 255) + "%";
            case 1: return String((g_settings.mediaVolume * 100) / 255) + "%";
            case 2: return PROFILE_NAMES[g_settings.deviceProfile % PROF_COUNT];
            case 3: return g_sys.wifiConnected ? "ON" : (g_sys.wifiConnecting ? "..." : "OFF");
            case 4: return g_sys.btConnected ? "Paired" : (g_sys.btEnabled ? "ON" : "OFF");
            case 5: return g_settings.lowPowerMode ? "ON" : "OFF";
            case 6: return "Launch";
            case 7: return "Start";
            case 8: return g_sys.sdMounted ? (String(g_sys.sdFreeMB) + "MB") : "Retry";
            default: return "";
        }
    }

    void renderCtrlCenter(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        static const char* CTRL_LABELS[kCtrlCount] = {
            "Brightness", "Media Vol", "Mode", "Wi-Fi", "Bluetooth", "ECO Mode", "Flashlight", "Screensaver", "Remount SD"
        };
        int offY = (int)roundf((1.0f - ui::easeOutCubic(m_ctrlAnim)) * 96.0f);

        spr.setClipRect(4, 39, SCREEN_W - 8, SCREEN_H - 42);
        spr.fillRoundRect(4, 39 - offY, SCREEN_W - 8, SCREEN_H - 42, 6, th.surface);
        spr.drawRoundRect(4, 39 - offY, SCREEN_W - 8, SCREEN_H - 42, 6, acc);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("Controls", 10, 43 - offY);
        spr.setTextColor(th.textDim);
        spr.setTextDatum(TR_DATUM);
        spr.drawString("[</>]Adj [Esc]Close", SCREEN_W - 10, 43 - offY);
        spr.setTextDatum(TL_DATUM);
        spr.drawFastHLine(8, 53 - offY, SCREEN_W - 16, th.border);

        int y0 = 56 - offY;
        for (int r = 0; r < 4; ++r) {
            int idx = m_ctrlScroll + r;
            if (idx >= kCtrlCount) break;
            int y = y0 + r * 19;
            bool sel = (idx == m_ctrlSel);
            spr.fillRoundRect(9, y, SCREEN_W - 22, 17, 4, sel ? th.surface2 : th.bg);
            spr.drawRoundRect(9, y, SCREEN_W - 22, 17, 4, th.border);
            spr.setTextColor(sel ? th.text : th.textDim);
            spr.drawString(CTRL_LABELS[idx], 15, y + 5);

            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(sel ? acc : th.accent2);
            spr.drawString(ctrlValueText(idx), SCREEN_W - 17, y + 5);
            spr.setTextDatum(TL_DATUM);
        }
        spr.drawRoundRect(9, (int)roundf(m_ctrlSelAnimY) - offY, SCREEN_W - 22, 17, 4, acc);
        ui::drawScrollbar(spr, SCREEN_W - 10, y0, 74, kCtrlCount, 4, m_ctrlScroll);
        spr.clearClipRect();
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

