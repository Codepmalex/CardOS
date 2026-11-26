#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../net_weather.h"

extern void switchApp(AppId id);

namespace {

class MinerApp : public App {
public:
    AppId id() const override { return APP_MINER; }
    const char* name() const override { return "Miner"; }

    void onEnter() override {
        m_editing = false;
        m_workerSel = 0;
        m_workerScroll = 0;
        if (g_sys.wifiConnected && strlen(g_settings.minerWallet) > 0 && !g_sys.minerPool.valid) {
            requestMinerRefresh();
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_editing) {
            handleEditInput(ev);
            return;
        }
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavLeft(ev)) {
            m_tab = (m_tab + 2) % 3;
            return;
        }
        if (isNavRight(ev)) {
            m_tab = (m_tab + 1) % 3;
            return;
        }
        if (m_tab == 1) {
            int n = g_sys.minerPool.workerCount;
            if (isNavUp(ev) && m_workerSel > 0) {
                m_workerSel--;
                if (m_workerSel < m_workerScroll) m_workerScroll = m_workerSel;
                return;
            }
            if (isNavDown(ev) && m_workerSel < n - 1) {
                m_workerSel++;
                if (m_workerSel >= m_workerScroll + 4) m_workerScroll = m_workerSel - 3;
                return;
            }
        }
        if (ev.code == CK_CHAR && (ev.ch == 'e' || ev.ch == 'E')) {
            startEdit();
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            if (!g_sys.wifiConnected) {
                showToast("No Wi-Fi!", 1400);
            } else if (strlen(g_settings.minerWallet) == 0) {
                showToast("Set wallet first ([E])", 1500);
            } else {
                requestMinerRefresh();
                showToast("Syncing Kryptex...", 1200);
            }
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'w' || ev.ch == 'W')) {
            switchApp(APP_WIFI);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        if (m_editing) {
            renderEditMode(spr, th, acc);
            ui::drawToastOverlay(spr);
            return;
        }

        ui::drawStatusBar(spr, "Miner");

        if (strlen(g_settings.minerWallet) == 0) {
            spr.fillRoundRect(8, 24, SCREEN_W - 16, 86, 5, th.surface);
            spr.drawRoundRect(8, 24, SCREEN_W - 16, 86, 5, th.border);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("Kryptex Pool Monitor", 16, 34);
            spr.setTextColor(th.text);
            spr.drawString("No wallet/API key configured.", 16, 52);
            spr.setTextColor(th.warn);
            spr.drawString("Press [E] to enter Coin & Wallet", 16, 70);
            spr.setTextColor(th.textDim);
            spr.drawString("Example coin: xtm-rx", 16, 88);
            spr.drawString("[E] Edit Wallet   [ESC] Home", 8, 122);
            ui::drawToastOverlay(spr);
            return;
        }

        MinerPoolLive p;
        if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(30))) {
            p = g_sys.minerPool;
            xSemaphoreGive(g_sysMutex);
        } else {
            p = g_sys.minerPool;
        }

        // Tab strip (y: 18..31)
        const char* tabs[3] = {"Overview", "Workers", "Config"};
        for (int t = 0; t < 3; ++t) {
            int tx = 6 + t * 56;
            bool act = (t == m_tab);
            spr.fillRoundRect(tx, 18, 53, 13, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.setTextDatum(MC_DATUM);
            spr.drawString(tabs[t], tx + 26, 24);
            spr.setTextDatum(TL_DATUM);
        }
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.setTextDatum(TR_DATUM);
        spr.drawString(g_settings.minerCoin, SCREEN_W - 6, 21);
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            renderOverviewTab(spr, p, th, acc);
        } else if (m_tab == 1) {
            renderWorkersTab(spr, p, th, acc);
        } else {
            renderConfigTab(spr, p, th, acc);
        }

        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("</>Tab [R]Sync [E]Edit [ESC]Home", 6, 125);
        ui::drawToastOverlay(spr);
    }

private:
    int m_tab = 0;
    int m_workerSel = 0;
    int m_workerScroll = 0;

    bool m_editing = false;
    int m_editStep = 0; // 0=Coin, 1=Wallet
    String m_editCoin;
    String m_editWallet;

    void startEdit() {
        m_editing = true;
        m_editStep = 0;
        m_editCoin = g_settings.minerCoin;
        m_editWallet = g_settings.minerWallet;
    }

    void handleEditInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_editing = false;
            return;
        }
        String* cur = (m_editStep == 0) ? &m_editCoin : &m_editWallet;
        if (ev.code == CK_BACKSPACE && !cur->isEmpty()) {
            cur->remove(cur->length() - 1);
            return;
        }
        if (ev.code == CK_TAB) {
            m_editStep = 1 - m_editStep;
            return;
        }
        if (ev.code == CK_ENTER) {
            if (m_editStep == 0) {
                m_editStep = 1;
                return;
            }
            m_editCoin.trim();
            m_editCoin.toLowerCase();
            m_editWallet.trim();
            if (m_editCoin.isEmpty()) m_editCoin = "xtm-rx";
            strlcpy(g_settings.minerCoin, m_editCoin.c_str(), sizeof(g_settings.minerCoin));
            strlcpy(g_settings.minerWallet, m_editWallet.c_str(), sizeof(g_settings.minerWallet));

            if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
                g_sys.minerPool = MinerPoolLive();
                xSemaphoreGive(g_sysMutex);
            }
            saveSettings();
            m_editing = false;
            requestMinerRefresh();
            showToast("Wallet saved!", 1200);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 33 && ev.ch <= 126) {
            int maxLen = (m_editStep == 0) ? 14 : 124;
            if ((int)cur->length() < maxLen) *cur += ev.ch;
        }
    }

    void renderOverviewTab(M5Canvas& spr, const MinerPoolLive& p, const ThemePalette& th, uint16_t acc) {
        if (!p.valid) {
            spr.fillRoundRect(6, 36, SCREEN_W - 12, 82, 5, th.surface);
            spr.drawRoundRect(6, 36, SCREEN_W - 12, 82, 5, th.border);
            spr.setTextSize(1);
            spr.setTextColor(p.fetching ? acc : th.warn);
            spr.drawString(!g_sys.wifiConnected ? "Waiting for Wi-Fi..." :
                           (p.fetching ? "Fetching Kryptex stats..." : "Press [R] to sync Kryptex"), 14, 48);
            if (strlen(p.errorMsg) > 0) {
                spr.setTextColor(th.err);
                spr.drawString(p.errorMsg, 14, 68);
            }
            spr.setTextColor(th.textDim);
            spr.drawString("https://pool.kryptex.com", 14, 92);
            return;
        }

        // Left card: Hashrate & Online Workers (6..118, y: 34..120)
        spr.fillRoundRect(6, 34, 112, 86, 5, th.surface);
        spr.drawRoundRect(6, 34, 112, 86, 5, th.border);
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("30m Hashrate", 12, 40);

        spr.setTextSize(2);
        spr.setTextColor(p.onlineWorkers > 0 ? th.ok : th.warn);
        char hbuf[16];
        if (p.totalHash30m >= 1000.0f) snprintf(hbuf, sizeof(hbuf), "%.2fk", p.totalHash30m / 1000.0f);
        else snprintf(hbuf, sizeof(hbuf), "%.0f", p.totalHash30m);
        spr.drawString(hbuf, 12, 52);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("H/s", 12 + (int)strlen(hbuf) * 12 + 3, 59);

        spr.setTextColor(th.text);
        char wb[32];
        snprintf(wb, sizeof(wb), "Online: %d / %d", p.onlineWorkers, p.totalWorkers);
        spr.drawString(wb, 12, 74);

        spr.setTextColor(th.textDim);
        snprintf(wb, sizeof(wb), "3h:%.0f  24h:%.0f", p.totalHash3h, p.totalHash24h);
        spr.drawString(wb, 12, 88);

        spr.setTextColor(p.fetching ? acc : th.ok);
        spr.drawString(p.fetching ? "Syncing..." : "Kryptex Live", 12, 104);

        // Right card: Balance & Threshold Progress (122..234, y: 34..120)
        spr.fillRoundRect(122, 34, 112, 86, 5, th.surface);
        spr.drawRoundRect(122, 34, 112, 86, 5, th.border);
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("Pool Balance", 128, 40);

        spr.setTextSize(2);
        spr.setTextColor(th.text);
        char bb[16];
        snprintf(bb, sizeof(bb), "%.2f", p.balTotal);
        spr.drawString(bb, 128, 52);

        spr.setTextSize(1);
        spr.setTextColor(th.ok);
        snprintf(wb, sizeof(wb), "Conf:   %.2f", p.balConfirmed);
        spr.drawString(wb, 128, 74);

        spr.setTextColor(th.warn);
        snprintf(wb, sizeof(wb), "Unconf: %.2f", p.balUnconfirmed);
        spr.drawString(wb, 128, 86);

        // Threshold progress bar
        int barX = 128, barY = 100, barW = 100, barH = 7;
        spr.fillRoundRect(barX, barY, barW, barH, 2, th.bg);
        int fw = (barW * constrain((int)p.reachedPct, 0, 100)) / 100;
        if (fw > 0) spr.fillRoundRect(barX, barY, fw, barH, 2, acc);
        spr.setTextColor(th.textDim);
        snprintf(wb, sizeof(wb), "%.0f%% of %.0f", p.reachedPct, p.threshold);
        spr.drawString(wb, 128, 110);
    }

    void renderWorkersTab(M5Canvas& spr, const MinerPoolLive& p, const ThemePalette& th, uint16_t acc) {
        if (p.workerCount == 0) {
            spr.setTextSize(1);
            spr.setTextColor(th.textDim);
            spr.drawString(p.fetching ? "Loading workers..." : "No workers found. Press [R].", 12, 62);
            return;
        }
        int maxVis = 4;
        for (int i = 0; i < maxVis; ++i) {
            int idx = m_workerScroll + i;
            if (idx >= p.workerCount) break;
            const auto& w = p.workers[idx];
            int y = 34 + i * 21;
            bool sel = (idx == m_workerSel);

            spr.fillRoundRect(6, y, SCREEN_W - 14, 19, 3, sel ? th.surface2 : th.surface);
            if (sel) spr.drawRoundRect(6, y, SCREEN_W - 14, 19, 3, acc);

            spr.fillCircle(13, y + 9, 3, w.online ? th.ok : th.err);
            spr.setTextSize(1);
            spr.setTextColor(sel ? acc : th.text);
            char nm[24];
            snprintf(nm, sizeof(nm), "%s[%s]", w.name, w.scheme);
            String ns(nm);
            if ((int)ns.length() > 15) ns = ns.substring(0, 14) + "~";
            spr.drawString(ns, 20, y + 6);

            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(w.online ? th.ok : th.textDim);
            char rb[28];
            snprintf(rb, sizeof(rb), "%.0fH/s %lu/%lu", w.hash30m,
                     (unsigned long)w.valid, (unsigned long)(w.stale + w.invalid));
            spr.drawString(rb, SCREEN_W - 14, y + 6);
            spr.setTextDatum(TL_DATUM);
        }
        ui::drawScrollbar(spr, SCREEN_W - 6, 34, 84, p.workerCount, maxVis, m_workerScroll);
    }

    void renderConfigTab(M5Canvas& spr, const MinerPoolLive& p, const ThemePalette& th, uint16_t acc) {
        spr.fillRoundRect(6, 34, SCREEN_W - 12, 86, 5, th.surface);
        spr.drawRoundRect(6, 34, SCREEN_W - 12, 86, 5, th.border);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("Pool: https://pool.kryptex.com", 14, 42);

        spr.setTextColor(th.text);
        spr.drawString(String("Coin: ") + g_settings.minerCoin, 14, 58);

        String w = String(g_settings.minerWallet);
        String shortW = ((int)w.length() > 28) ? (w.substring(0, 13) + "..." + w.substring(w.length() - 12)) : w;
        spr.setTextColor(th.textDim);
        spr.drawString("Key: " + shortW, 14, 74);

        char sb[40];
        snprintf(sb, sizeof(sb), "Bad shares: %lu  |  Threshold: %.0f",
                 (unsigned long)p.totalBadShares, p.threshold);
        spr.drawString(sb, 14, 92);

        spr.setTextColor(th.warn);
        spr.drawString("Press [E] to change Coin or Wallet", 14, 106);
    }

    void renderEditMode(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        ui::drawStatusBar(spr, "Kryptex Wallet");
        spr.drawRoundRect(8, 22, SCREEN_W - 16, 94, 5, th.border);
        spr.setTextSize(1);

        spr.setTextColor(m_editStep == 0 ? acc : th.textDim);
        spr.drawString("Coin (e.g. xtm-rx):", 16, 30);
        spr.fillRoundRect(16, 42, SCREEN_W - 32, 16, 3, m_editStep == 0 ? th.surface2 : th.surface);
        spr.drawRoundRect(16, 42, SCREEN_W - 32, 16, 3, m_editStep == 0 ? acc : th.border);
        spr.setTextColor(th.text);
        spr.drawString(m_editCoin + (m_editStep == 0 ? "_" : ""), 21, 46);

        spr.setTextColor(m_editStep == 1 ? acc : th.textDim);
        spr.drawString("Wallet / API Address:", 16, 66);
        spr.fillRoundRect(16, 78, SCREEN_W - 32, 18, 3, m_editStep == 1 ? th.surface2 : th.surface);
        spr.drawRoundRect(16, 78, SCREEN_W - 32, 18, 3, m_editStep == 1 ? acc : th.border);
        spr.setTextColor(th.text);
        String wv = m_editWallet + (m_editStep == 1 ? "_" : "");
        if ((int)wv.length() > 32) wv = "..." + wv.substring(wv.length() - 29);
        spr.drawString(wv, 21, 83);

        spr.setTextColor(th.textDim);
        spr.drawString("Enter: Next/Save   ESC: Cancel", 14, 123);
    }
};

} // namespace

App* createMinerApp() { return new MinerApp(); }
