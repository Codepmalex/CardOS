#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../hardware.h"
#include "../net_weather.h"
#include "../app_installer.h"
#include "../bt_manager.h"
#include "../bsod.h"
#include <time.h>
#include <vector>

extern void switchApp(AppId id);
extern M5Canvas* getScreenCanvas();

namespace {

enum SetCat : uint8_t {
    CAT_APPEARANCE = 0,
    CAT_DISPLAY,
    CAT_BUTTONS_AUDIO,
    CAT_LED,
    CAT_WIFI,
    CAT_BLUETOOTH,
    CAT_TIME_WEATHER,
    CAT_NOTIF,
    CAT_SYSTEM,
    CAT_COUNT
};

static const char* CAT_NAMES[CAT_COUNT] = {
    "Appearance",
    "Display",
    "Buttons & Audio",
    "RGB LED",
    "Wi-Fi Network",
    "Bluetooth",
    "Time & Weather",
    "Notifications",
    "System & Info"
};

class SettingsApp : public App {
public:
    AppId id() const override { return APP_SETTINGS; }
    const char* name() const override { return "Settings"; }

    void onEnter() override {
        m_inCategory = false;
        m_wifiScanning = false;
        m_wifiPassPrompt = false;
        m_cityPrompt = false;
        m_btNamePrompt = false;
        m_tzMapMode = false;
        m_notifAppMode = false;
        m_subSlideX = 0.0f;
        m_catAnimY = 20.0f + (m_catSel - m_catScroll) * 21.0f;
        m_itemAnimY = 20.0f + (m_itemSel - m_itemScroll) * 21.0f;
    }

    void onExit() override {
        std::vector<WifiNetInfo>().swap(m_nets);
        std::vector<BtPeerInfo>().swap(m_btPeers);
        std::vector<String>().swap(m_ddOptions);
        std::vector<int>().swap(m_ddValues);
        m_ddTitle = String();
        m_pendingSsid = String();
        m_pendingPass = String();
        m_pendingCity = String();
        m_pendingBtName = String();
    }

    void enterTimezoneMap() {
        m_inCategory = true;
        m_catSel = CAT_TIME_WEATHER;
        m_itemSel = 1;
        m_tzMapSel = g_settings.tzIdx % TZ_COUNT;
        m_tzMapMode = true;
    }

    void onTick() override {
        m_subSlideX = ui::smoothMove(m_subSlideX, 0.0f, 0.42f);
        m_catAnimY = ui::smoothMove(m_catAnimY, 20.0f + (m_catSel - m_catScroll) * 21.0f, 0.48f);
        m_itemAnimY = ui::smoothMove(m_itemAnimY, 20.0f + (m_itemSel - m_itemScroll) * 21.0f, 0.48f);

        if (!m_ddOpen) return;
        if (m_ddClosing) {
            m_ddAnim -= 0.32f;
            if (m_ddAnim <= 0.0f) {
                m_ddAnim = 0.0f;
                m_ddOpen = false;
                m_ddClosing = false;
            }
        } else if (m_ddAnim < 1.0f) {
            m_ddAnim = min(1.0f, m_ddAnim + 0.30f);
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_ddOpen) {
            handleDropdownInput(ev);
            return;
        }
        if (m_tzMapMode) {
            handleTzMapInput(ev);
            return;
        }
        if (m_notifAppMode) {
            handleNotifAppInput(ev);
            return;
        }

        if (m_wifiPassPrompt) {
            handleWifiPassInput(ev);
            return;
        }
        if (m_cityPrompt) {
            handleCityPromptInput(ev);
            return;
        }
        if (m_btNamePrompt) {
            handleBtNameInput(ev);
            return;
        }
        if (m_btScanning) {
            handleBtScanInput(ev);
            return;
        }
        if (m_wifiScanning) {
            handleWifiScanListInput(ev);
            return;
        }

        if (!m_inCategory) {
            if (isNavBack(ev)) { switchApp(APP_HOME); return; }
            if (isNavUp(ev) && m_catSel > 0) {
                m_catSel--;
                if (m_catSel < m_catScroll) m_catScroll = m_catSel;
            } else if (isNavDown(ev) && m_catSel < CAT_COUNT - 1) {
                m_catSel++;
                if (m_catSel >= m_catScroll + 5) m_catScroll = m_catSel - 4;
            } else if (ev.code == CK_ENTER || isNavRight(ev)) {
                m_inCategory = true;
                m_itemSel = 0;
                m_itemScroll = 0;
                m_itemAnimY = 20.0f;
                m_subSlideX = 64.0f;
            }
            return;
        }

        // Inside a category
        if (isNavBack(ev)) {
            m_inCategory = false;
            m_subSlideX = -64.0f;
            m_catAnimY = 20.0f + (m_catSel - m_catScroll) * 21.0f;
            return;
        }
        int cnt = itemCountForCat((SetCat)m_catSel);
        if (isNavUp(ev) && m_itemSel > 0) {
            m_itemSel--;
            if (m_itemSel < m_itemScroll) m_itemScroll = m_itemSel;
        } else if (isNavDown(ev) && m_itemSel < cnt - 1) {
            m_itemSel++;
            if (m_itemSel >= m_itemScroll + 5) m_itemScroll = m_itemSel - 4;
        } else if (isNavLeft(ev)) {
            adjustItem((SetCat)m_catSel, m_itemSel, -1);
        } else if (ev.code == CK_ENTER) {
            if (!openDropdownForItem((SetCat)m_catSel, m_itemSel)) {
                adjustItem((SetCat)m_catSel, m_itemSel, 1);
            }
        } else if (isNavRight(ev)) {
            adjustItem((SetCat)m_catSel, m_itemSel, 1);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        if (m_tzMapMode) {
            renderTzMap(spr);
            ui::drawToastOverlay(spr);
            return;
        }
        if (m_notifAppMode) {
            renderNotifAppEditor(spr);
            if (m_ddOpen) renderDropdown(spr);
            ui::drawToastOverlay(spr);
            return;
        }

        if (m_wifiPassPrompt) {
            ui::drawStatusBar(spr, "Wi-Fi Password");
            spr.drawRoundRect(12, 30, SCREEN_W - 24, 72, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("SSID: " + m_pendingSsid, 20, 38);
            spr.setTextColor(th.textDim);
            spr.drawString("Enter password (blank if open):", 20, 52);
            spr.fillRoundRect(20, 66, SCREEN_W - 40, 18, 3, th.surface2);
            spr.setTextColor(th.text);
            spr.drawString(m_pendingPass + "_", 25, 71);
            return;
        }

        if (m_btNamePrompt) {
            ui::drawStatusBar(spr, "Bluetooth Name");
            spr.drawRoundRect(12, 30, SCREEN_W - 24, 72, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("Device Name:", 20, 42);
            spr.fillRoundRect(20, 60, SCREEN_W - 40, 18, 3, th.surface2);
            spr.setTextColor(th.text);
            spr.drawString(m_pendingBtName + "_", 25, 65);
            return;
        }

        if (m_cityPrompt) {
            ui::drawStatusBar(spr, "Weather City");
            spr.drawRoundRect(12, 30, SCREEN_W - 24, 72, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("City (or 'Auto'):", 20, 42);
            spr.fillRoundRect(20, 60, SCREEN_W - 40, 18, 3, th.surface2);
            spr.setTextColor(th.text);
            spr.drawString(m_pendingCity + "_", 25, 65);
            return;
        }

        if (m_btScanning) {
            ui::drawStatusBar(spr, "Nearby BLE Devices");
            spr.setTextSize(1);
            if (m_btPeers.empty()) {
                spr.setTextColor(th.textDim);
                spr.drawString("No BLE devices found. ESC back.", 12, 54);
            } else {
                for (int i = 0; i < 5; ++i) {
                    int idx = m_btScroll + i;
                    if (idx >= (int)m_btPeers.size()) break;
                    int y = 20 + i * 20;
                    bool sel = (idx == m_btSel);
                    spr.fillRoundRect(6, y, SCREEN_W - 16, 18, 3, sel ? th.surface2 : th.surface);
                    if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 18, 3, acc);
                    spr.setTextColor(sel ? acc : th.text);
                    String s = m_btPeers[idx].name;
                    if ((int)s.length() > 21) s = s.substring(0, 20) + "~";
                    spr.drawString(s, 11, y + 5);
                    spr.setTextDatum(TR_DATUM);
                    spr.setTextColor(th.textDim);
                    char rbuf[12];
                    snprintf(rbuf, sizeof(rbuf), "%ddBm", m_btPeers[idx].rssi);
                    spr.drawString(rbuf, SCREEN_W - 14, y + 5);
                    spr.setTextDatum(TL_DATUM);
                }
                ui::drawScrollbar(spr, SCREEN_W - 6, 20, 98, (int)m_btPeers.size(), 5, m_btScroll);
            }
            ui::drawToastOverlay(spr);
            return;
        }

        if (m_wifiScanning) {
            ui::drawStatusBar(spr, "Select Wi-Fi");
            spr.setTextSize(1);
            if (m_nets.empty()) {
                spr.setTextColor(th.textDim);
                spr.drawString("No networks found. ESC to go back.", 12, 54);
            } else {
                for (int i = 0; i < 5; ++i) {
                    int idx = m_netScroll + i;
                    if (idx >= (int)m_nets.size()) break;
                    int y = 20 + i * 20;
                    bool sel = (idx == m_netSel);
                    spr.fillRoundRect(6, y, SCREEN_W - 16, 18, 3, sel ? th.surface2 : th.surface);
                    if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 18, 3, acc);
                    spr.setTextColor(sel ? acc : th.text);
                    String s = m_nets[idx].ssid;
                    if ((int)s.length() > 21) s = s.substring(0, 20) + "~";
                    spr.drawString(s, 11, y + 5);
                    spr.setTextDatum(TR_DATUM);
                    spr.setTextColor(th.textDim);
                    char rbuf[12];
                    snprintf(rbuf, sizeof(rbuf), "%ddBm", m_nets[idx].rssi);
                    spr.drawString(rbuf, SCREEN_W - 14, y + 5);
                    spr.setTextDatum(TL_DATUM);
                }
                ui::drawScrollbar(spr, SCREEN_W - 6, 20, 98, (int)m_nets.size(), 5, m_netScroll);
            }
            ui::drawToastOverlay(spr);
            return;
        }

        if (!m_inCategory) {
            ui::drawStatusBar(spr, "Settings");
            int ox = (int)roundf(m_subSlideX);
            spr.setClipRect(0, 18, SCREEN_W - 6, 108);
            spr.setTextSize(1);
            for (int i = 0; i < 5; ++i) {
                int idx = m_catScroll + i;
                if (idx >= CAT_COUNT) break;
                int y = 20 + i * 21;
                bool sel = (idx == m_catSel);
                spr.fillRoundRect(6 + ox, y, SCREEN_W - 16, 19, 4, sel ? th.surface2 : th.surface);
                int tx = 12 + ox;
                if (idx == CAT_WIFI) {
                    uint16_t wc = g_sys.wifiConnected ? th.ok : acc;
                    spr.fillCircle(16 + ox, y + 13, 1, wc);
                    spr.drawArc(16 + ox, y + 13, 4, 4, 225, 315, wc);
                    spr.drawArc(16 + ox, y + 13, 7, 7, 225, 315, wc);
                    tx = 27 + ox;
                } else if (idx == CAT_BLUETOOTH) {
                    uint16_t bc = g_sys.btConnected ? th.ok : (g_sys.btEnabled ? acc : th.textDim);
                    spr.drawFastVLine(16 + ox, y + 4, 10, bc);
                    spr.drawLine(16 + ox, y + 4, 19 + ox, y + 6, bc);
                    spr.drawLine(19 + ox, y + 6, 14 + ox, y + 10, bc);
                    spr.drawLine(16 + ox, y + 13, 19 + ox, y + 11, bc);
                    spr.drawLine(19 + ox, y + 11, 14 + ox, y + 7, bc);
                    tx = 27 + ox;
                }
                spr.setTextColor(sel ? acc : th.text);
                spr.drawString(CAT_NAMES[idx], tx, y + 6);
                spr.setTextDatum(TR_DATUM);
                if (idx == CAT_WIFI) {
                    spr.setTextColor(g_sys.wifiConnected ? th.ok : th.textDim);
                    spr.drawString(g_sys.wifiConnected ? "ON >" : "OFF >", SCREEN_W - 14 + ox, y + 6);
                } else if (idx == CAT_BLUETOOTH) {
                    spr.setTextColor(g_sys.btEnabled ? th.ok : th.textDim);
                    spr.drawString(g_sys.btEnabled ? "ON >" : "OFF >", SCREEN_W - 14 + ox, y + 6);
                } else {
                    spr.setTextColor(th.textDim);
                    spr.drawString(">", SCREEN_W - 14 + ox, y + 6);
                }
                spr.setTextDatum(TL_DATUM);
            }
            spr.drawRoundRect(6 + ox, (int)roundf(m_catAnimY), SCREEN_W - 16, 19, 4, acc);
            spr.clearClipRect();
            ui::drawScrollbar(spr, SCREEN_W - 6, 20, 104, CAT_COUNT, 5, m_catScroll);
        } else {
            ui::drawStatusBar(spr, CAT_NAMES[m_catSel]);
            int ox = (int)roundf(m_subSlideX);
            spr.setClipRect(0, 18, SCREEN_W - 6, 108);
            int cnt = itemCountForCat((SetCat)m_catSel);
            spr.setTextSize(1);
            for (int i = 0; i < 5; ++i) {
                int idx = m_itemScroll + i;
                if (idx >= cnt) break;
                int y = 20 + i * 21;
                bool sel = (idx == m_itemSel);
                spr.fillRoundRect(6 + ox, y, SCREEN_W - 16, 19, 3, sel ? th.surface2 : th.surface);

                String lbl, val;
                getItemText((SetCat)m_catSel, idx, lbl, val);
                spr.setTextColor(th.text);
                spr.drawString(lbl, 11 + ox, y + 6);
                spr.setTextDatum(TR_DATUM);
                spr.setTextColor(sel ? acc : th.textDim);
                spr.drawString(val, SCREEN_W - 14 + ox, y + 6);
                spr.setTextDatum(TL_DATUM);
            }
            spr.drawRoundRect(6 + ox, (int)roundf(m_itemAnimY), SCREEN_W - 16, 19, 3, acc);
            spr.clearClipRect();
            ui::drawScrollbar(spr, SCREEN_W - 6, 20, 104, cnt, 5, m_itemScroll);
            spr.setTextColor(th.textDim);
            spr.drawString("</> Adjust  [Enter] Menu  ESC Back", 6, 125);
        }
        if (m_ddOpen) renderDropdown(spr);
        ui::drawToastOverlay(spr);
    }

private:
    bool m_inCategory = false;
    int m_catSel = 0, m_catScroll = 0;
    int m_itemSel = 0, m_itemScroll = 0;
    float m_catAnimY = 20.0f;
    float m_itemAnimY = 20.0f;
    float m_subSlideX = 0.0f;

    bool m_wifiScanning = false;
    std::vector<WifiNetInfo> m_nets;
    int m_netSel = 0, m_netScroll = 0;

    bool m_wifiPassPrompt = false;
    String m_pendingSsid, m_pendingPass;

    bool m_cityPrompt = false;
    String m_pendingCity;

    bool m_btNamePrompt = false;
    String m_pendingBtName;

    bool m_btScanning = false;
    std::vector<BtPeerInfo> m_btPeers;
    int m_btSel = 0, m_btScroll = 0;

    bool m_tzMapMode = false;
    int m_tzMapSel = 5;

    bool m_notifAppMode = false;
    int m_notifAppIdx = 0;
    int m_notifRuleSel = 0;
    int m_notifRuleScroll = 0;

    // Animated Dropdown state
    bool m_ddOpen = false;
    bool m_ddClosing = false;
    float m_ddAnim = 0.0f;
    String m_ddTitle;
    std::vector<String> m_ddOptions;
    std::vector<int> m_ddValues;
    int m_ddSel = 0;
    int m_ddScroll = 0;
    SetCat m_ddCat = CAT_APPEARANCE;
    int m_ddIdx = 0;
    bool m_ddNotifRule = false;

    void beginDropdown(const String& title, const std::vector<String>& opts, const std::vector<int>& vals,
                       int curVal, SetCat c, int idx, bool isNotifRule) {
        m_ddTitle = title;
        m_ddOptions = opts;
        m_ddValues = vals;
        m_ddCat = c;
        m_ddIdx = idx;
        m_ddNotifRule = isNotifRule;
        m_ddSel = 0;
        for (int i = 0; i < (int)vals.size(); ++i) {
            if (vals[i] == curVal) { m_ddSel = i; break; }
        }
        m_ddScroll = constrain(m_ddSel - 2, 0, max(0, (int)opts.size() - 5));
        m_ddClosing = false;
        m_ddAnim = 0.15f;
        m_ddOpen = true;
    }

    bool openDropdownForItem(SetCat c, int idx) {
        std::vector<String> opts;
        std::vector<int> vals;
        if (c == CAT_APPEARANCE) {
            if (idx == 0) {
                for (int i = 0; i < THEME_COUNT; ++i) { opts.push_back(THEMES[i].name); vals.push_back(i); }
                beginDropdown("Color Theme", opts, vals, g_settings.themeIdx, c, idx, false);
                return true;
            }
            if (idx == 1) {
                for (int i = 0; i < ACCENT_COUNT; ++i) { opts.push_back(ACCENT_NAMES[i]); vals.push_back(i); }
                beginDropdown("Accent Color", opts, vals, g_settings.accentIdx, c, idx, false);
                return true;
            }
            if (idx == 2) {
                for (int w = 0; w < WIDGET_COUNT; ++w) {
                    if (isWidgetAvailable((uint8_t)w)) {
                        opts.push_back(HOME_WIDGET_NAMES[w]);
                        vals.push_back(w);
                    }
                }
                beginDropdown("Home Widget", opts, vals, g_settings.homeWidget, c, idx, false);
                return true;
            }
        } else if (c == CAT_DISPLAY && idx == 1) {
            const uint16_t ts[5] = {0, 30, 60, 120, 300};
            const char* lbls[5] = {"Never", "30s", "60s", "120s", "300s"};
            for (int i = 0; i < 5; ++i) { opts.push_back(lbls[i]); vals.push_back(ts[i]); }
            beginDropdown("Screen Timeout", opts, vals, g_settings.screenTimeoutSec, c, idx, false);
            return true;
        } else if (c == CAT_DISPLAY && idx == 6) {
            for (int w = 0; w < WIDGET_COUNT; ++w) {
                if (isWidgetAvailable((uint8_t)w)) {
                    opts.push_back(HOME_WIDGET_NAMES[w]);
                    vals.push_back(w);
                }
            }
            beginDropdown("Saver Widget", opts, vals, g_settings.screensaverWidget, c, idx, false);
            return true;
        } else if (c == CAT_BUTTONS_AUDIO) {
            if (idx == 0) {
                for (int i = 0; i < PROF_COUNT; ++i) { opts.push_back(PROFILE_NAMES[i]); vals.push_back(i); }
                beginDropdown("Device Mode", opts, vals, g_settings.deviceProfile, c, idx, false);
                return true;
            }
            if (idx == 1 || idx == 2) {
                for (int i = 0; i < BTN_ACT_COUNT; ++i) { opts.push_back(BTN_ACTION_NAMES[i]); vals.push_back(i); }
                int cur = (idx == 1) ? g_settings.btnShortAction : g_settings.btnLongAction;
                beginDropdown(idx == 1 ? "G0 Short Press" : "G0 Long Press", opts, vals, cur, c, idx, false);
                return true;
            }
        } else if (c == CAT_LED) {
            if (idx == 1) {
                for (int i = 0; i < LED_MODE_COUNT; ++i) { opts.push_back(LED_MODE_NAMES[i]); vals.push_back(i); }
                beginDropdown("LED Mode", opts, vals, g_settings.ledMode, c, idx, false);
                return true;
            }
            if (idx == 2) {
                int cur = 0;
                for (int i = 0; i < 7; ++i) {
                    opts.push_back(LED_COLOR_NAMES[i]);
                    vals.push_back(i);
                    if (g_settings.ledColor == LED_PRESETS[i]) cur = i;
                }
                beginDropdown("LED Color", opts, vals, cur, c, idx, false);
                return true;
            }
        } else if (c == CAT_TIME_WEATHER && idx == 4) {
            const uint16_t rs[4] = {15, 30, 60, 120};
            for (int i = 0; i < 4; ++i) { opts.push_back(String(rs[i]) + " min"); vals.push_back(rs[i]); }
            beginDropdown("Weather Sync", opts, vals, g_settings.weatherRefreshMin, c, idx, false);
            return true;
        } else if (c == CAT_NOTIF) {
            if (idx == 0) {
                for (int i = 0; i < PROF_COUNT; ++i) { opts.push_back(PROFILE_NAMES[i]); vals.push_back(i); }
                beginDropdown("Device Mode", opts, vals, g_settings.deviceProfile, c, idx, false);
                return true;
            }
            if (idx == 2) {
                const uint8_t pcts[4] = {30, 50, 70, 90};
                for (int i = 0; i < 4; ++i) { opts.push_back(String(pcts[i]) + "%+"); vals.push_back(pcts[i]); }
                beginDropdown("Rain Alert", opts, vals, g_settings.rainAlertPct, c, idx, false);
                return true;
            }
        }
        return false;
    }

    bool openDropdownForNotifRule(int ruleIdx) {
        std::vector<String> opts;
        std::vector<int> vals;
        const auto& r = g_settings.notifRules[m_notifAppIdx];
        if (ruleIdx == 2) {
            for (int i = 0; i < 4; ++i) { opts.push_back(NOTIF_TONE_NAMES[i]); vals.push_back(i); }
            beginDropdown("Sound Tone", opts, vals, r.toneIdx, CAT_NOTIF, ruleIdx, true);
            return true;
        }
        if (ruleIdx == 4) {
            for (int i = 0; i < 7; ++i) { opts.push_back(LED_COLOR_NAMES[i]); vals.push_back(i); }
            beginDropdown("LED Color", opts, vals, r.ledColorIdx, CAT_NOTIF, ruleIdx, true);
            return true;
        }
        if (ruleIdx == 5) {
            for (int i = 0; i < 4; ++i) { opts.push_back(NOTIF_LED_PAT_NAMES[i]); vals.push_back(i); }
            beginDropdown("LED Pattern", opts, vals, r.ledPattern, CAT_NOTIF, ruleIdx, true);
            return true;
        }
        return false;
    }

    void applyDropdownSelection(int val) {
        if (m_ddNotifRule) {
            auto& r = g_settings.notifRules[m_notifAppIdx];
            if (m_ddIdx == 2) r.toneIdx = (uint8_t)(val % 4);
            else if (m_ddIdx == 4) r.ledColorIdx = (uint8_t)(val % 7);
            else if (m_ddIdx == 5) r.ledPattern = (uint8_t)(val % 4);
        } else {
            if (m_ddCat == CAT_APPEARANCE) {
                if (m_ddIdx == 0) g_settings.themeIdx = (uint8_t)(val % THEME_COUNT);
                else if (m_ddIdx == 1) g_settings.accentIdx = (uint8_t)(val % ACCENT_COUNT);
                else if (m_ddIdx == 2) g_settings.homeWidget = (uint8_t)(val % WIDGET_COUNT);
            } else if (m_ddCat == CAT_DISPLAY && m_ddIdx == 1) {
                g_settings.screenTimeoutSec = (uint16_t)val;
            } else if (m_ddCat == CAT_DISPLAY && m_ddIdx == 6) {
                g_settings.screensaverWidget = (uint8_t)(val % WIDGET_COUNT);
            } else if (m_ddCat == CAT_BUTTONS_AUDIO) {
                if (m_ddIdx == 0) { g_settings.deviceProfile = (uint8_t)(val % PROF_COUNT); updateLedEngine(); }
                else if (m_ddIdx == 1) g_settings.btnShortAction = (uint8_t)(val % BTN_ACT_COUNT);
                else if (m_ddIdx == 2) g_settings.btnLongAction = (uint8_t)(val % BTN_ACT_COUNT);
            } else if (m_ddCat == CAT_LED) {
                if (m_ddIdx == 1) g_settings.ledMode = (uint8_t)(val % LED_MODE_COUNT);
                else if (m_ddIdx == 2) g_settings.ledColor = LED_PRESETS[val % 7];
                updateLedEngine();
            } else if (m_ddCat == CAT_TIME_WEATHER && m_ddIdx == 4) {
                g_settings.weatherRefreshMin = (uint16_t)val;
            } else if (m_ddCat == CAT_NOTIF) {
                if (m_ddIdx == 0) { g_settings.deviceProfile = (uint8_t)(val % PROF_COUNT); updateLedEngine(); }
                else if (m_ddIdx == 2) g_settings.rainAlertPct = (uint8_t)val;
            }
        }
        saveSettings();
    }

    void handleDropdownInput(const InputEvent& ev) {
        if (m_ddClosing) return;
        if (isNavBack(ev)) {
            m_ddClosing = true;
            return;
        }
        if (isNavUp(ev) && m_ddSel > 0) {
            m_ddSel--;
            if (m_ddSel < m_ddScroll) m_ddScroll = m_ddSel;
            return;
        }
        if (isNavDown(ev) && m_ddSel < (int)m_ddOptions.size() - 1) {
            m_ddSel++;
            if (m_ddSel >= m_ddScroll + 5) m_ddScroll = m_ddSel - 4;
            return;
        }
        if (ev.code == CK_ENTER && !m_ddValues.empty()) {
            applyDropdownSelection(m_ddValues[m_ddSel]);
            m_ddClosing = true;
        }
    }

    void renderDropdown(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        int vis = min(5, (int)m_ddOptions.size());
        int fullH = vis * 16 + 22;
        float t = m_ddAnim;
        float eased = 1.0f - (1.0f - t) * (1.0f - t);
        int curH = max(14, (int)roundf(14.0f + (fullH - 14) * eased));

        int dw = 152;
        int dx = (SCREEN_W - dw) / 2;
        int dy = (SCREEN_H - fullH) / 2;

        spr.setClipRect(dx, dy, dw, curH);
        spr.fillRoundRect(dx, dy, dw, fullH, 5, th.surface2);
        spr.drawRoundRect(dx, dy, dw, fullH, 5, acc);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString(m_ddTitle, dx + 8, dy + 4);
        spr.drawFastHLine(dx + 4, dy + 15, dw - 8, th.border);

        for (int i = 0; i < vis; ++i) {
            int idx = m_ddScroll + i;
            if (idx >= (int)m_ddOptions.size()) break;
            int ry = dy + 18 + i * 16;
            bool sel = (idx == m_ddSel);
            if (sel) {
                spr.fillRoundRect(dx + 5, ry, dw - 14, 14, 3, acc);
            }
            spr.setTextColor(sel ? th.bg : th.text);
            spr.drawString(m_ddOptions[idx], dx + 9, ry + 3);
        }
        if ((int)m_ddOptions.size() > 5) {
            ui::drawScrollbar(spr, dx + dw - 6, dy + 18, vis * 16 - 2, (int)m_ddOptions.size(), 5, m_ddScroll);
        }
        spr.clearClipRect();
    }

    void handleNotifAppInput(const InputEvent& ev) {
        if (isNavBack(ev)) { m_notifAppMode = false; return; }
        if (isNavUp(ev) && m_notifRuleSel > 0) {
            m_notifRuleSel--;
            if (m_notifRuleSel < m_notifRuleScroll) m_notifRuleScroll = m_notifRuleSel;
            return;
        }
        if (isNavDown(ev) && m_notifRuleSel < 6) {
            m_notifRuleSel++;
            if (m_notifRuleSel >= m_notifRuleScroll + 5) m_notifRuleScroll = m_notifRuleSel - 4;
            return;
        }
        if (ev.code == CK_ENTER && openDropdownForNotifRule(m_notifRuleSel)) {
            return;
        }
        int d = 0;
        if (isNavLeft(ev)) d = -1;
        else if (isNavRight(ev) || ev.code == CK_ENTER) d = 1;
        if (d == 0) return;

        auto& r = g_settings.notifRules[m_notifAppIdx];
        switch (m_notifRuleSel) {
            case 0: r.enabled = !r.enabled; break;
            case 1: r.volume = (uint8_t)constrain((int)r.volume + d * 25, 0, 255); break;
            case 2: r.toneIdx = (r.toneIdx + d + 4) % 4; break;
            case 3: r.ledEnabled = !r.ledEnabled; break;
            case 4: r.ledColorIdx = (r.ledColorIdx + d + 7) % 7; break;
            case 5: r.ledPattern = (r.ledPattern + d + 4) % 4; break;
            case 6:
                if (d > 0) {
                    saveSettings();
                    triggerNotification((NotifApp)m_notifAppIdx,
                                        (String("Test: ") + NOTIF_APP_NAMES[m_notifAppIdx]).c_str());
                }
                return;
        }
        saveSettings();
    }

    void renderNotifAppEditor(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        String hdr = String(NOTIF_APP_NAMES[m_notifAppIdx]) + " Notif";
        ui::drawStatusBar(spr, hdr.c_str());

        const auto& r = g_settings.notifRules[m_notifAppIdx];
        const char* labels[7] = {"Enabled", "Volume", "Sound Tone", "LED Flash", "LED Color", "LED Pattern", "Test Notif"};
        String vals[7] = {
            r.enabled ? "ON" : "OFF",
            String((r.volume * 100) / 255) + "%",
            String(NOTIF_TONE_NAMES[r.toneIdx % 4]) + " v",
            r.ledEnabled ? "ON" : "OFF",
            String(LED_COLOR_NAMES[r.ledColorIdx % 7]) + " v",
            String(NOTIF_LED_PAT_NAMES[r.ledPattern % 4]) + " v",
            "[Enter]"
        };

        spr.setTextSize(1);
        for (int i = 0; i < 5; ++i) {
            int idx = m_notifRuleScroll + i;
            if (idx >= 7) break;
            int y = 20 + i * 21;
            bool sel = (idx == m_notifRuleSel);
            spr.fillRoundRect(6, y, SCREEN_W - 16, 19, 3, sel ? th.surface2 : th.surface);
            if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 19, 3, acc);

            spr.setTextColor(th.text);
            spr.drawString(labels[idx], 11, y + 6);
            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(sel ? acc : th.textDim);
            spr.drawString(vals[idx], SCREEN_W - 14, y + 6);
            spr.setTextDatum(TL_DATUM);
        }
        ui::drawScrollbar(spr, SCREEN_W - 6, 20, 104, 7, 5, m_notifRuleScroll);
        spr.setTextColor(th.textDim);
        spr.drawString("</> Change   ESC Back", 6, 125);
    }

    void handleTzMapInput(const InputEvent& ev) {
        if (isNavBack(ev)) {
            m_tzMapMode = false;
            return;
        }
        if (isNavLeft(ev)) {
            m_tzMapSel = (m_tzMapSel - 1 + TZ_COUNT) % TZ_COUNT;
        } else if (isNavRight(ev)) {
            m_tzMapSel = (m_tzMapSel + 1) % TZ_COUNT;
        } else if (ev.code == CK_CHAR && (ev.ch == 't' || ev.ch == 'T')) {
            g_settings.use24h = !g_settings.use24h;
            saveSettings();
            showToast(g_settings.use24h ? "Clock: 24 Hour" : "Clock: 12h AM/PM", 1200);
        } else if (ev.code == CK_ENTER) {
            g_settings.tzIdx = (uint8_t)m_tzMapSel;
            applyTimezoneConfig();
            saveSettings();
            m_tzMapMode = false;
            showToast("Timezone saved!", 1400);
        }
    }

    void renderTzMap(M5Canvas& spr) {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        ui::drawStatusBar(spr, "Timezone Map");

        const int mapY = 18;
        const int mapH = 66;
        const auto& tz = TZ_LIST[m_tzMapSel % TZ_COUNT];

        // Ocean & faint lat/lon grid
        spr.fillRect(0, mapY, SCREEN_W, mapH, th.bg);
        for (int gx = 0; gx < SCREEN_W; gx += 30) {
            spr.drawFastVLine(gx, mapY, mapH, th.surface);
        }
        spr.drawFastHLine(0, mapY + mapH / 2, SCREEN_W, th.surface);

        // Selected timezone vertical band
        spr.fillRect(constrain(tz.mapX - 6, 0, SCREEN_W - 12), mapY, 12, mapH, th.surface2);

        // Continents (simplified world map vector silhouettes)
        uint16_t land = rgb565(64, 92, 118);
        uint16_t landEdge = th.border;
        // North America
        spr.fillTriangle(18, 22, 62, 22, 34, 42, land);
        spr.fillTriangle(34, 42, 62, 22, 74, 34, land);
        spr.fillTriangle(34, 42, 74, 34, 62, 48, land);
        spr.fillTriangle(48, 46, 62, 48, 68, 56, land);
        // Greenland
        spr.fillTriangle(78, 20, 96, 20, 86, 29, land);
        // South America
        spr.fillTriangle(68, 54, 88, 56, 76, 78, land);
        spr.fillTriangle(88, 56, 94, 65, 76, 78, land);
        // Europe & UK
        spr.fillRoundRect(114, 24, 30, 14, 3, land);
        spr.fillRect(114, 29, 4, 5, land);
        // Africa
        spr.fillTriangle(112, 41, 144, 42, 130, 72, land);
        spr.fillTriangle(112, 41, 130, 72, 118, 56, land);
        // Asia
        spr.fillRoundRect(142, 22, 66, 24, 4, land);
        spr.fillTriangle(164, 44, 178, 44, 171, 56, land); // India
        spr.fillTriangle(180, 44, 198, 44, 188, 56, land); // SE Asia
        spr.fillRect(210, 34, 5, 11, land);                // Japan
        // Australia & NZ
        spr.fillRoundRect(194, 58, 26, 15, 4, land);
        spr.fillRect(225, 68, 4, 6, land);
        spr.drawFastHLine(0, mapY + mapH, SCREEN_W, landEdge);

        // Draw all timezone dots
        for (int i = 0; i < TZ_COUNT; ++i) {
            spr.fillCircle(TZ_LIST[i].mapX, TZ_LIST[i].mapY, 1, th.textDim);
        }

        // Active timezone crosshair & pin
        spr.drawFastVLine(tz.mapX, mapY, mapH, acc);
        spr.drawFastHLine(0, tz.mapY, SCREEN_W, th.surface2);
        spr.fillCircle(tz.mapX, tz.mapY, 4, acc);
        spr.drawCircle(tz.mapX, tz.mapY, 6, th.text);

        // Bottom info card (y: 86..133)
        spr.fillRect(0, 86, SCREEN_W, 49, th.surface);
        spr.drawFastHLine(0, 86, SCREEN_W, th.border);
        spr.setTextSize(1);
        spr.setTextColor(acc);
        String zLine = String("< ") + tz.name + " (" + tz.utcLabel + ") >";
        spr.drawString(zLine, 6, 91);

        // Live preview of current time in selected zone / format
        struct tm tmInfo;
        char tPrev[16] = "--:--";
        if (getSafeLocalTime(tmInfo)) {
            int h = tmInfo.tm_hour;
            if (g_settings.use24h) {
                snprintf(tPrev, sizeof(tPrev), "%02d:%02d", h, tmInfo.tm_min);
            } else {
                const char* ap = (h >= 12) ? "PM" : "AM";
                h = h % 12; if (h == 0) h = 12;
                snprintf(tPrev, sizeof(tPrev), "%d:%02d%s", h, tmInfo.tm_min, ap);
            }
        }
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(th.text);
        spr.drawString(tPrev, SCREEN_W - 6, 91);
        spr.setTextDatum(TL_DATUM);

        spr.setTextColor(th.text);
        spr.drawString(String("Format: ") + (g_settings.use24h ? "24 Hour" : "12h AM/PM"), 6, 105);

        spr.setTextColor(th.textDim);
        spr.drawString("</> Zone  [T] 12/24h  [Enter] Save", 6, 121);
    }

    int itemCountForCat(SetCat c) const {
        switch (c) {
            case CAT_APPEARANCE:    return 3;
            case CAT_DISPLAY:       return 8;
            case CAT_BUTTONS_AUDIO: return 6;
            case CAT_LED:           return 4;
            case CAT_WIFI:          return 4;
            case CAT_BLUETOOTH:     return 5;
            case CAT_TIME_WEATHER:  return 5;
            case CAT_NOTIF:         return 10;
            case CAT_SYSTEM:        return 9;
            default:                return 0;
        }
    }

    void getItemText(SetCat c, int idx, String& lbl, String& val) const {
        switch (c) {
            case CAT_APPEARANCE:
                if (idx == 0)      { lbl = "Color Theme"; val = String(activeTheme().name) + " v"; }
                else if (idx == 1) { lbl = "Accent Color"; val = String(ACCENT_NAMES[g_settings.accentIdx % ACCENT_COUNT]) + " v"; }
                else               { lbl = "Home Widget"; val = String(HOME_WIDGET_NAMES[g_settings.homeWidget % WIDGET_COUNT]) + " v"; }
                break;
            case CAT_DISPLAY:
                if (idx == 0)      { lbl = "Brightness"; val = String((g_settings.brightness * 100) / 255) + "%"; }
                else if (idx == 1) { lbl = "Screen Timeout"; val = (g_settings.screenTimeoutSec == 0 ? "Never" : String(g_settings.screenTimeoutSec) + "s") + " v"; }
                else if (idx == 2) { lbl = "Dim Before Sleep"; val = g_settings.dimBeforeSleep ? "ON" : "OFF"; }
                else if (idx == 3) { lbl = "Screen Flip"; val = g_settings.flipScreen ? "180 deg" : "Normal"; }
                else if (idx == 4) { lbl = "Screensaver"; val = g_settings.screensaverEnabled ? "ON" : "OFF"; }
                else if (idx == 5) { lbl = "Saver Bright"; val = String((g_settings.screensaverBright * 100) / 255) + "%"; }
                else if (idx == 6) { lbl = "Saver Widget"; val = String(HOME_WIDGET_NAMES[g_settings.screensaverWidget % WIDGET_COUNT]) + " v"; }
                else               { lbl = "Test Saver"; val = "[Enter]"; }
                break;
            case CAT_BUTTONS_AUDIO:
                if (idx == 0)      { lbl = "Device Mode"; val = String(PROFILE_NAMES[g_settings.deviceProfile % PROF_COUNT]) + " v"; }
                else if (idx == 1) { lbl = "G0 Short Press"; val = String(BTN_ACTION_NAMES[g_settings.btnShortAction % BTN_ACT_COUNT]) + " v"; }
                else if (idx == 2) { lbl = "G0 Long Press"; val = String(BTN_ACTION_NAMES[g_settings.btnLongAction % BTN_ACT_COUNT]) + " v"; }
                else if (idx == 3) { lbl = "Key Click Sound"; val = g_settings.keyClick ? "ON" : "OFF"; }
                else if (idx == 4) { lbl = "Key Click Vol"; val = String((g_settings.volume * 100) / 255) + "%"; }
                else               { lbl = "Media Output Vol"; val = String((g_settings.mediaVolume * 100) / 255) + "%"; }
                break;
            case CAT_LED:
                if (idx == 0)      { lbl = "RGB LED"; val = g_settings.ledEnabled ? "ON" : "OFF"; }
                else if (idx == 1) { lbl = "LED Mode"; val = String(LED_MODE_NAMES[g_settings.ledMode % LED_MODE_COUNT]) + " v"; }
                else if (idx == 2) {
                    lbl = "LED Color";
                    val = "Custom v";
                    for (int i = 0; i < 7; ++i) if (g_settings.ledColor == LED_PRESETS[i]) val = String(LED_COLOR_NAMES[i]) + " v";
                }
                else               { lbl = "LED Brightness"; val = String((g_settings.ledBrightness * 100) / 255) + "%"; }
                break;
            case CAT_WIFI:
                if (idx == 0) { lbl = "Scan & Connect"; val = "[Enter]"; }
                else if (idx == 1) { lbl = "Status"; val = g_sys.wifiStatus; }
                else if (idx == 2) { lbl = "Auto-Connect"; val = g_settings.wifiAutoConnect ? "ON" : "OFF"; }
                else          { lbl = "Disconnect"; val = g_sys.wifiConnected ? "[Enter]" : "-"; }
                break;
            case CAT_BLUETOOTH:
                if (idx == 0)      { lbl = "Bluetooth"; val = g_settings.btEnabled ? "ON" : "OFF"; }
                else if (idx == 1) { lbl = "Device Name"; val = g_settings.btName; }
                else if (idx == 2) { lbl = "Scan Nearby BLE"; val = "[Enter]"; }
                else if (idx == 3) { lbl = "Status"; val = btStatusText(); }
                else               { lbl = "Disconnect"; val = btIsConnected() ? "[Enter]" : "-"; }
                break;
            case CAT_TIME_WEATHER:
                if (idx == 0)      { lbl = "Clock Format"; val = g_settings.use24h ? "24 Hour" : "12h AM/PM"; }
                else if (idx == 1) { lbl = "Timezone (Map)"; val = TZ_LIST[g_settings.tzIdx % TZ_COUNT].utcLabel; }
                else if (idx == 2) { lbl = "Temp Unit"; val = g_settings.tempFahrenheit ? "F" : "C"; }
                else if (idx == 3) { lbl = "Weather City"; val = g_settings.weatherCity; }
                else               { lbl = "Refresh Every"; val = String(g_settings.weatherRefreshMin) + "m v"; }
                break;
            case CAT_NOTIF:
                if (idx == 0)      { lbl = "Device Mode"; val = String(PROFILE_NAMES[g_settings.deviceProfile % PROF_COUNT]) + " v"; }
                else if (idx == 1) { lbl = "Master Notifs"; val = g_settings.notifEnabled ? "ON" : "OFF"; }
                else if (idx == 2) { lbl = "Rain Alert >="; val = String(g_settings.rainAlertPct) + "% v"; }
                else if (idx == 3) { lbl = "Calendar App"; val = g_settings.notifRules[NOTIF_CALENDAR].enabled ? "ON >" : "OFF >"; }
                else if (idx == 4) { lbl = "Weather / Rain"; val = g_settings.notifRules[NOTIF_WEATHER].enabled ? "ON >" : "OFF >"; }
                else if (idx == 5) { lbl = "Battery Alerts"; val = g_settings.notifRules[NOTIF_BATTERY].enabled ? "ON >" : "OFF >"; }
                else if (idx == 6) { lbl = "System Alerts"; val = g_settings.notifRules[NOTIF_SYSTEM].enabled ? "ON >" : "OFF >"; }
                else if (idx == 7) { lbl = "Miner Alerts"; val = g_settings.notifRules[NOTIF_MINER].enabled ? "ON >" : "OFF >"; }
                else if (idx == 8) { lbl = "Update Alerts"; val = g_settings.notifRules[NOTIF_UPDATES].enabled ? "ON >" : "OFF >"; }
                else               { lbl = "Phone Alerts"; val = g_settings.notifRules[NOTIF_PHONE].enabled ? "ON >" : "OFF >"; }
                break;
            case CAT_SYSTEM:
                if (idx == 0)      { lbl = "Low Batt Mode"; val = g_settings.lowPowerMode ? "ON (ECO)" : "OFF"; }
                else if (idx == 1) { lbl = "Storage"; val = g_sys.sdMounted ? (String(g_sys.sdTotalMB) + "MB SD") : "SPIFFS"; }
                else if (idx == 2) { lbl = "Free Heap"; val = String(ESP.getFreeHeap() / 1024) + "KB"; }
                else if (idx == 3) { lbl = "App1 Slot"; val = strlen(g_settings.installedBinName) ? "Loaded" : "Empty"; }
                else if (idx == 4) { lbl = "Boot App1 Now"; val = "[Enter]"; }
                else if (idx == 5) { lbl = "Software Update"; val = String("v") + CARDOS_VERSION + " [Ent]"; }
                else if (idx == 6) { lbl = "Test BSOD"; val = "[Enter]"; }
                else if (idx == 7) { lbl = "Factory Reset"; val = "[Enter]"; }
                else               { lbl = "Reboot CardOS"; val = "[Enter]"; }
                break;
            default: break;
        }
    }

    void adjustItem(SetCat c, int idx, int d) {
        switch (c) {
            case CAT_APPEARANCE:
                if (idx == 0)      g_settings.themeIdx = (g_settings.themeIdx + d + THEME_COUNT) % THEME_COUNT;
                else if (idx == 1) g_settings.accentIdx = (g_settings.accentIdx + d + ACCENT_COUNT) % ACCENT_COUNT;
                else {
                    for (int step = 0; step < WIDGET_COUNT; ++step) {
                        g_settings.homeWidget = (g_settings.homeWidget + d + WIDGET_COUNT) % WIDGET_COUNT;
                        if (isWidgetAvailable(g_settings.homeWidget)) break;
                    }
                }
                break;
            case CAT_DISPLAY:
                if (idx == 0) {
                    int b = (int)g_settings.brightness + d * 25;
                    g_settings.brightness = (uint8_t)constrain(b, 30, 255);
                    applyBrightness(g_settings.brightness);
                } else if (idx == 1) {
                    const uint16_t ts[5] = {0, 30, 60, 120, 300};
                    int cur = 3;
                    for (int i = 0; i < 5; ++i) if (g_settings.screenTimeoutSec == ts[i]) cur = i;
                    g_settings.screenTimeoutSec = ts[(cur + d + 5) % 5];
                } else if (idx == 2) {
                    g_settings.dimBeforeSleep = !g_settings.dimBeforeSleep;
                } else if (idx == 3) {
                    g_settings.flipScreen = !g_settings.flipScreen;
                    M5Cardputer.Display.setRotation(g_settings.flipScreen ? 2 : 0);
                } else if (idx == 4) {
                    g_settings.screensaverEnabled = !g_settings.screensaverEnabled;
                } else if (idx == 5) {
                    int sb = (int)g_settings.screensaverBright + d * 20;
                    g_settings.screensaverBright = (uint8_t)constrain(sb, 10, 200);
                } else if (idx == 6) {
                    for (int step = 0; step < WIDGET_COUNT; ++step) {
                        g_settings.screensaverWidget = (g_settings.screensaverWidget + d + WIDGET_COUNT) % WIDGET_COUNT;
                        if (isWidgetAvailable(g_settings.screensaverWidget)) break;
                    }
                } else if (d > 0) {
                    saveSettings();
                    enterScreensaver();
                    return;
                }
                break;
            case CAT_BUTTONS_AUDIO:
                if (idx == 0) {
                    g_settings.deviceProfile = (g_settings.deviceProfile + d + PROF_COUNT) % PROF_COUNT;
                    updateLedEngine();
                }
                else if (idx == 1) g_settings.btnShortAction = (g_settings.btnShortAction + d + BTN_ACT_COUNT) % BTN_ACT_COUNT;
                else if (idx == 2) g_settings.btnLongAction = (g_settings.btnLongAction + d + BTN_ACT_COUNT) % BTN_ACT_COUNT;
                else if (idx == 3) g_settings.keyClick = !g_settings.keyClick;
                else if (idx == 4) {
                    int v = (int)g_settings.volume + d * 25;
                    g_settings.volume = (uint8_t)constrain(v, 0, 255);
                } else {
                    int mv = (int)g_settings.mediaVolume + d * 25;
                    g_settings.mediaVolume = (uint8_t)constrain(mv, 0, 255);
                    M5Cardputer.Speaker.setVolume(g_settings.mediaVolume);
                }
                break;
            case CAT_LED:
                if (idx == 0) g_settings.ledEnabled = !g_settings.ledEnabled;
                else if (idx == 1) g_settings.ledMode = (g_settings.ledMode + d + LED_MODE_COUNT) % LED_MODE_COUNT;
                else if (idx == 2) {
                    int cur = 0;
                    for (int i = 0; i < 7; ++i) if (g_settings.ledColor == LED_PRESETS[i]) cur = i;
                    g_settings.ledColor = LED_PRESETS[(cur + d + 7) % 7];
                } else {
                    int lb = (int)g_settings.ledBrightness + d * 20;
                    g_settings.ledBrightness = (uint8_t)constrain(lb, 5, 255);
                }
                updateLedEngine();
                break;
            case CAT_WIFI:
                if (idx == 0 && d > 0) {
                    switchApp(APP_WIFI);
                    return;
                } else if (idx == 2) {
                    g_settings.wifiAutoConnect = !g_settings.wifiAutoConnect;
                } else if (idx == 3 && d > 0) {
                    requestWifiDisconnect();
                    showToast("Disconnected", 1200);
                }
                break;
            case CAT_BLUETOOTH:
                if (idx == 0) {
                    btSetEnabled(!g_settings.btEnabled);
                    showToast(g_settings.btEnabled ? "Bluetooth ON" : "Bluetooth OFF", 1300);
                } else if (idx == 1 && d > 0) {
                    m_pendingBtName = g_settings.btName;
                    m_btNamePrompt = true;
                } else if (idx == 2 && d > 0) {
                    showToast("Scanning BLE (3s)...", 3000);
                    g_sys.busyCount++;
                    m_btPeers = btScanNearby(3);
                    if (g_sys.busyCount > 0) g_sys.busyCount--;
                    m_btSel = 0;
                    m_btScroll = 0;
                    m_btScanning = true;
                } else if (idx == 4 && d > 0) {
                    btDisconnect();
                    showToast("BT Disconnected", 1200);
                }
                break;
            case CAT_TIME_WEATHER:
                if (idx == 0) g_settings.use24h = !g_settings.use24h;
                else if (idx == 1) {
                    m_tzMapSel = g_settings.tzIdx % TZ_COUNT;
                    m_tzMapMode = true;
                    return;
                } else if (idx == 2) {
                    g_settings.tempFahrenheit = !g_settings.tempFahrenheit;
                } else if (idx == 3 && d > 0) {
                    m_pendingCity = g_settings.weatherCity;
                    m_cityPrompt = true;
                } else if (idx == 4) {
                    const uint16_t rs[4] = {15, 30, 60, 120};
                    int cur = 1;
                    for (int i = 0; i < 4; ++i) if (g_settings.weatherRefreshMin == rs[i]) cur = i;
                    g_settings.weatherRefreshMin = rs[(cur + d + 4) % 4];
                }
                break;
            case CAT_NOTIF:
                if (idx == 0) {
                    g_settings.deviceProfile = (g_settings.deviceProfile + d + PROF_COUNT) % PROF_COUNT;
                    updateLedEngine();
                } else if (idx == 1) {
                    g_settings.notifEnabled = !g_settings.notifEnabled;
                } else if (idx == 2) {
                    const uint8_t pcts[4] = {30, 50, 70, 90};
                    int cur = 1;
                    for (int i = 0; i < 4; ++i) if (g_settings.rainAlertPct == pcts[i]) cur = i;
                    g_settings.rainAlertPct = pcts[(cur + d + 4) % 4];
                } else if (d > 0) {
                    m_notifAppIdx = idx - 3;
                    m_notifRuleSel = 0;
                    m_notifRuleScroll = 0;
                    m_notifAppMode = true;
                    return;
                }
                break;
            case CAT_SYSTEM:
                if (idx == 0) {
                    g_settings.lowPowerMode = !g_settings.lowPowerMode;
                    applyPowerProfile();
                    showToast(g_settings.lowPowerMode ? "Low Batt Mode ON" : "Low Batt Mode OFF", 1400);
                } else if (idx == 4 && d > 0) {
                    if (strlen(g_settings.installedBinName) == 0) {
                        showToast("No app installed in app1", 1500);
                    } else {
                        String err;
                        if (!launchInstalledApp1(err)) showToast(err.c_str(), 1800);
                    }
                } else if (idx == 5 && d > 0) {
                    saveSettings();
                    runGithubOsUpdateInteractive();
                    return;
                } else if (idx == 6 && d > 0) {
                    if (getScreenCanvas()) {
                        showBsodScreen(*getScreenCanvas(), "TEST_BSOD_TRIGGERED", "Triggered manually from Settings -> System", true);
                    }
                    return;
                } else if (idx == 7 && d > 0) {
                    factoryResetSettings();
                    applyPowerProfile();
                    showToast("Settings reset!", 1500);
                } else if (idx == 8 && d > 0) {
                    saveWeatherAndTimeCache();
                    delay(40);
                    rebootToCardOS();
                }
                break;
            default: break;
        }
        saveSettings();
    }

    void handleWifiScanListInput(const InputEvent& ev) {
        if (isNavBack(ev)) { m_wifiScanning = false; return; }
        if (isNavUp(ev) && m_netSel > 0) {
            m_netSel--;
            if (m_netSel < m_netScroll) m_netScroll = m_netSel;
        } else if (isNavDown(ev) && m_netSel < (int)m_nets.size() - 1) {
            m_netSel++;
            if (m_netSel >= m_netScroll + 5) m_netScroll = m_netSel - 4;
        } else if (ev.code == CK_ENTER && !m_nets.empty()) {
            m_pendingSsid = m_nets[m_netSel].ssid;
            m_pendingPass = "";
            m_wifiScanning = false;
            if (m_nets[m_netSel].open) {
                strlcpy(g_settings.wifiSsid, m_pendingSsid.c_str(), sizeof(g_settings.wifiSsid));
                g_settings.wifiPass[0] = '\0';
                saveSettings();
                requestWifiConnect();
                showToast("Connecting...", 1500);
            } else {
                m_wifiPassPrompt = true;
            }
        }
    }

    void handleWifiPassInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_wifiPassPrompt = false;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_pendingPass.isEmpty()) {
            m_pendingPass.remove(m_pendingPass.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            strlcpy(g_settings.wifiSsid, m_pendingSsid.c_str(), sizeof(g_settings.wifiSsid));
            strlcpy(g_settings.wifiPass, m_pendingPass.c_str(), sizeof(g_settings.wifiPass));
            saveSettings();
            requestWifiConnect();
            m_wifiPassPrompt = false;
            showToast("Connecting...", 1800);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_pendingPass.length() < 60) {
            m_pendingPass += ev.ch;
        }
    }

    void handleCityPromptInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_cityPrompt = false;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_pendingCity.isEmpty()) {
            m_pendingCity.remove(m_pendingCity.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            m_pendingCity.trim();
            if (m_pendingCity.isEmpty()) m_pendingCity = "Auto";
            strlcpy(g_settings.weatherCity, m_pendingCity.c_str(), sizeof(g_settings.weatherCity));
            g_settings.weatherLat = 0.0f;
            g_settings.weatherLon = 0.0f;
            saveSettings();
            requestWeatherRefresh();
            m_cityPrompt = false;
            showToast("City saved", 1400);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_pendingCity.length() < 32) {
            m_pendingCity += ev.ch;
        }
    }

    void handleBtNameInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_btNamePrompt = false;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_pendingBtName.isEmpty()) {
            m_pendingBtName.remove(m_pendingBtName.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            m_pendingBtName.trim();
            if (m_pendingBtName.isEmpty()) m_pendingBtName = "CardOS";
            strlcpy(g_settings.btName, m_pendingBtName.c_str(), sizeof(g_settings.btName));
            saveSettings();
            btApplyName();
            m_btNamePrompt = false;
            showToast("BT name saved", 1300);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_pendingBtName.length() < 20) {
            m_pendingBtName += ev.ch;
        }
    }

    void handleBtScanInput(const InputEvent& ev) {
        if (isNavBack(ev)) { m_btScanning = false; return; }
        if (isNavUp(ev) && m_btSel > 0) {
            m_btSel--;
            if (m_btSel < m_btScroll) m_btScroll = m_btSel;
        } else if (isNavDown(ev) && m_btSel < (int)m_btPeers.size() - 1) {
            m_btSel++;
            if (m_btSel >= m_btScroll + 5) m_btScroll = m_btSel - 4;
        } else if (ev.code == CK_ENTER && !m_btPeers.empty()) {
            String nm = m_btPeers[m_btSel].name;
            showToast(("Connecting " + nm).c_str(), 1800);
            bool ok = btConnectPeer(m_btPeers[m_btSel].addr);
            m_btScanning = false;
            showToast(ok ? ("Paired: " + nm).c_str() : "BLE Connect failed", 1600);
        }
    }
};

} // namespace

static SettingsApp* s_settingsInstance = nullptr;

App* createSettingsApp() {
    if (!s_settingsInstance) s_settingsInstance = new SettingsApp();
    return s_settingsInstance;
}

void openTimezoneMap() {
    switchApp(APP_SETTINGS);
    if (s_settingsInstance) {
        s_settingsInstance->enterTimezoneMap();
    }
}

