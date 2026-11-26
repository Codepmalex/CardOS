#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../net_weather.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

class WifiApp : public App {
public:
    AppId id() const override { return APP_WIFI; }
    const char* name() const override { return "Wi-Fi"; }

    void onEnter() override {
        m_mode = MODE_LIST;
        m_sel = 0;
        m_scroll = 0;
        m_lastSeq = getWifiScanSeq();
        m_nets = getWifiScanResults();
        requestWifiScan();
    }

    void onTick() override {
        uint32_t seq = getWifiScanSeq();
        if (seq != m_lastSeq) {
            m_lastSeq = seq;
            m_nets = getWifiScanResults();
            m_sel = 0;
            m_scroll = 0;
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_mode == MODE_MANUAL_SSID) {
            handleManualSsidInput(ev);
            return;
        }
        if (m_mode == MODE_PASSWORD) {
            handlePasswordInput(ev);
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavUp(ev) && m_sel > 0) {
            m_sel--;
            if (m_sel < m_scroll) m_scroll = m_sel;
        } else if (isNavDown(ev) && m_sel < (int)m_nets.size() - 1) {
            m_sel++;
            if (m_sel >= m_scroll + 4) m_scroll = m_sel - 3;
        } else if (ev.code == CK_ENTER && !m_nets.empty()) {
            const auto& net = m_nets[m_sel];
            m_targetSsid = net.ssid;
            const char* savedPass = findSavedWifiPass(net.ssid.c_str());
            if (net.open) {
                connectWith(m_targetSsid, "");
            } else if (savedPass != nullptr) {
                connectWith(m_targetSsid, String(savedPass));
            } else {
                m_passInput = "";
                m_showPass = false;
                m_mode = MODE_PASSWORD;
            }
        } else if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P') && !m_nets.empty()) {
            const auto& net = m_nets[m_sel];
            m_targetSsid = net.ssid;
            const char* savedPass = findSavedWifiPass(net.ssid.c_str());
            m_passInput = savedPass ? String(savedPass) : "";
            m_showPass = false;
            m_mode = MODE_PASSWORD;
        } else if (ev.code == CK_CHAR && (ev.ch == 'f' || ev.ch == 'F') && !m_nets.empty()) {
            if (removeSavedWifi(m_nets[m_sel].ssid.c_str())) {
                saveSettings();
                showToast("Forgot Wi-Fi", 1200);
            }
        } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            requestWifiScan();
            showToast("Scanning Wi-Fi...", 1200);
        } else if (ev.code == CK_CHAR && (ev.ch == 'm' || ev.ch == 'M')) {
            m_targetSsid = g_settings.wifiSsid;
            m_mode = MODE_MANUAL_SSID;
        } else if (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D')) {
            requestWifiDisconnect();
            showToast("Wi-Fi Disconnected", 1400);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Wi-Fi Setup");

        if (m_mode == MODE_MANUAL_SSID) {
            spr.drawRoundRect(12, 28, SCREEN_W - 24, 76, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("Manual Network SSID:", 20, 36);
            spr.fillRoundRect(20, 52, SCREEN_W - 40, 18, 3, th.surface2);
            spr.setTextColor(th.text);
            spr.drawString(m_targetSsid + "_", 25, 57);
            spr.setTextColor(th.textDim);
            spr.drawString("Enter: Next   ESC: Cancel", 20, 82);
            ui::drawToastOverlay(spr);
            return;
        }

        if (m_mode == MODE_PASSWORD) {
            spr.drawRoundRect(12, 26, SCREEN_W - 24, 84, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            String s = m_targetSsid;
            if ((int)s.length() > 26) s = s.substring(0, 25) + "~";
            spr.drawString("SSID: " + s, 20, 34);

            spr.setTextColor(th.textDim);
            spr.drawString("Password (blank if open):", 20, 48);

            spr.fillRoundRect(20, 62, SCREEN_W - 40, 18, 3, th.surface2);
            spr.drawRoundRect(20, 62, SCREEN_W - 40, 18, 3, acc);
            spr.setTextColor(th.text);
            String disp = m_showPass ? m_passInput : maskedStr(m_passInput.length());
            if ((int)disp.length() > 29) disp = disp.substring(disp.length() - 29);
            spr.drawString(disp + "_", 25, 67);

            spr.setTextColor(th.textDim);
            spr.drawString("[Tab] Show/Hide  [Enter] Join", 20, 90);
            ui::drawToastOverlay(spr);
            return;
        }

        // Top connection status card (y: 19..35)
        spr.fillRoundRect(6, 19, SCREEN_W - 12, 17, 3, th.surface);
        spr.setTextSize(1);
        if (g_sys.wifiConnected) {
            spr.setTextColor(th.ok);
            char buf[48];
            snprintf(buf, sizeof(buf), "OK: %s (%s)", g_sys.wifiStatus, g_sys.wifiIp);
            spr.drawString(buf, 10, 24);
        } else if (g_sys.wifiConnecting) {
            spr.setTextColor(th.warn);
            spr.drawString(String("Connecting: ") + g_settings.wifiSsid, 10, 24);
        } else {
            spr.setTextColor(th.textDim);
            spr.drawString("Offline - Pick a Wi-Fi network", 10, 24);
        }

        bool scanning = isWifiScanInProgress();
        if (scanning && m_nets.empty()) {
            static const char* DOTS[4] = {"", ".", "..", "..."};
            int phase = (millis() / 300) % 4;
            spr.setTextColor(acc);
            spr.drawString(String("Scanning Wi-Fi networks") + DOTS[phase], 14, 62);
        } else if (m_nets.empty()) {
            spr.setTextColor(th.textDim);
            spr.drawString("No networks. Press [R] to rescan.", 10, 62);
        } else {
            for (int i = 0; i < 4; ++i) {
                int idx = m_scroll + i;
                if (idx >= (int)m_nets.size()) break;
                int y = 39 + i * 20;
                bool sel = (idx == m_sel);
                const auto& n = m_nets[idx];
                bool isCur = (g_sys.wifiConnected && n.ssid == g_sys.wifiStatus);

                spr.fillRoundRect(6, y, SCREEN_W - 16, 18, 3, sel ? th.surface2 : th.surface);
                if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 18, 3, acc);

                spr.setTextColor(isCur ? th.ok : (sel ? acc : th.text));
                bool isSaved = (findSavedWifiPass(n.ssid.c_str()) != nullptr);
                String nm = String(isSaved ? "* " : "") + n.ssid;
                if ((int)nm.length() > 21) nm = nm.substring(0, 20) + "~";
                spr.drawString(nm, 11, y + 5);

                spr.setTextColor(th.textDim);
                spr.drawString(n.open ? "Open" : "Lock", SCREEN_W - 62, y + 5);
                drawRssiBars(spr, SCREEN_W - 30, y + 13, n.rssi, acc, th.border);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, 39, 78, (int)m_nets.size(), 4, m_scroll);
        }

        spr.setTextColor(th.textDim);
        spr.drawString(scanning ? "Scanning...    [F]Forget [M]Manual" : "[Ent]Join [R]Scan [F]Forget [D]Disc", 6, 124);
        ui::drawToastOverlay(spr);
    }

private:
    enum Mode { MODE_LIST, MODE_PASSWORD, MODE_MANUAL_SSID } m_mode = MODE_LIST;
    std::vector<WifiNetInfo> m_nets;
    int m_sel = 0;
    int m_scroll = 0;
    uint32_t m_lastSeq = 0;

    String m_targetSsid;
    String m_passInput;
    bool m_showPass = false;

    static String maskedStr(size_t len) {
        String s;
        for (size_t i = 0; i < len; ++i) s += '*';
        return s;
    }

    static void drawRssiBars(M5Canvas& spr, int x, int baseY, int rssi, uint16_t onCol, uint16_t offCol) {
        int bars = (rssi >= -55) ? 4 : (rssi >= -67) ? 3 : (rssi >= -78) ? 2 : 1;
        for (int b = 0; b < 4; ++b) {
            int bh = 3 + b * 2;
            spr.fillRect(x + b * 4, baseY - bh, 3, bh, (b < bars) ? onCol : offCol);
        }
    }

    void connectWith(const String& ssid, const String& pass) {
        strlcpy(g_settings.wifiSsid, ssid.c_str(), sizeof(g_settings.wifiSsid));
        strlcpy(g_settings.wifiPass, pass.c_str(), sizeof(g_settings.wifiPass));
        upsertSavedWifi(ssid.c_str(), pass.c_str());
        g_settings.wifiAutoConnect = true;
        saveSettings();
        requestWifiConnect();
        m_mode = MODE_LIST;
        showToast("Connecting...", 2000);
    }

    void handleManualSsidInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_mode = MODE_LIST;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_targetSsid.isEmpty()) {
            m_targetSsid.remove(m_targetSsid.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            m_targetSsid.trim();
            if (!m_targetSsid.isEmpty()) {
                m_passInput = "";
                m_showPass = false;
                m_mode = MODE_PASSWORD;
            }
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_targetSsid.length() < 60) {
            m_targetSsid += ev.ch;
        }
    }

    void handlePasswordInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_mode = MODE_LIST;
            return;
        }
        if (ev.code == CK_TAB) {
            m_showPass = !m_showPass;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_passInput.isEmpty()) {
            m_passInput.remove(m_passInput.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            connectWith(m_targetSsid, m_passInput);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_passInput.length() < 60) {
            m_passInput += ev.ch;
        }
    }
};

} // namespace

App* createWifiApp() { return new WifiApp(); }
