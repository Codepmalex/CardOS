#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern void switchApp(AppId id);

namespace {

static bool s_apActive = false;

static String urlEncodeParam(const String& s) {
    String out;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else if (c == ' ') {
            out += '+';
        } else {
            char b[4];
            snprintf(b, sizeof(b), "%%%02X", (unsigned char)c);
            out += b;
        }
    }
    return out;
}

static bool httpBridgeCall(const String& pathAndQuery, const String* postBody, String& out) {
    HTTPClient http;
    http.setTimeout(15000);
    String url = "http://" + String(g_settings.macBridgeIp) + ":8765" + pathAndQuery;
    if (!http.begin(url)) {
        out = "HTTP init failed";
        return false;
    }
    int code = postBody ? http.POST(*postBody) : http.GET();
    if (code <= 0) {
        http.end();
        out = "Unreachable (" + url + ")\nRun tools/cardos_mac_bridge.py on Mac";
        return false;
    }
    out = http.getString();
    http.end();
    return (code == 200);
}

class MacLinkApp : public App {
public:
    AppId id() const override { return APP_MACLINK; }
    const char* name() const override { return "MacLink"; }

    void onEnter() override {
        m_editingIp = false;
        m_infoModal = false;
        if (m_output.isEmpty()) {
            m_output = "Connect Mac to CardOS-AP (pass: cardos1234), run tools/cardos_mac_bridge.py, then type a command below.";
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_infoModal) {
            if (isNavUp(ev) && m_infoScroll > 0) m_infoScroll--;
            else if (isNavDown(ev) && m_infoScroll < max(0, m_infoLines - 8)) m_infoScroll++;
            else if (isNavBack(ev) || ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) {
                m_infoModal = false;
            }
            return;
        }
        if (m_editingIp) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) { m_editingIp = false; return; }
            if (ev.code == CK_BACKSPACE && !m_ipBuf.isEmpty()) { m_ipBuf.remove(m_ipBuf.length() - 1); return; }
            if (ev.code == CK_ENTER) {
                m_ipBuf.trim();
                if (!m_ipBuf.isEmpty()) {
                    strlcpy(g_settings.macBridgeIp, m_ipBuf.c_str(), sizeof(g_settings.macBridgeIp));
                    saveSettings();
                    showToast("Mac IP saved!", 1200);
                }
                m_editingIp = false;
                return;
            }
            if (ev.code == CK_CHAR && (isdigit(ev.ch) || ev.ch == '.')) {
                if (m_ipBuf.length() < 24) m_ipBuf += ev.ch;
            }
            return;
        }

        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            switchApp(APP_HOME);
            return;
        }
        if (ev.code == CK_TAB) {
            m_tab = (m_tab + 1) % 2;
            return;
        }

        if (m_tab == 0) {
            // Compute / Terminal command input
            if (isNavUp(ev) && m_outScroll > 0) {
                m_outScroll--;
            } else if (isNavDown(ev) && m_outScroll < max(0, m_outLines - 5)) {
                m_outScroll++;
            } else if (ev.code == CK_BACKSPACE && !m_cmd.isEmpty()) {
                m_cmd.remove(m_cmd.length() - 1);
            } else if (ev.code == CK_ENTER) {
                if (!m_cmd.isEmpty()) {
                    m_output = "Running on Mac...";
                    m_outScroll = 0;
                    String res;
                    macLinkExecCommand(m_cmd, res);
                    m_output = res;
                    m_outScroll = 0;
                    m_cmd = "";
                }
            } else if (ev.code == CK_CHAR && ev.ch == 25) {
                // ignore
            } else if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126) {
                if (m_cmd.isEmpty() && (ev.ch == 'a' || ev.ch == 'A') && false) {
                    // typing in cmd directly; AP toggle via [Tab] or Fn+A? Keep Tab -> Remote tab for [A]/[M]
                }
                if (m_cmd.length() < 96) m_cmd += ev.ch;
            }
        } else {
            // Remote & Stats Tab
            if (ev.code == CK_CHAR) {
                if (ev.ch == 'a' || ev.ch == 'A') {
                    toggleAp();
                } else if (ev.ch == 'm' || ev.ch == 'M') {
                    m_ipBuf = g_settings.macBridgeIp;
                    m_editingIp = true;
                } else if (ev.ch == 'r' || ev.ch == 'R') {
                    fetchMacStats();
                } else if (ev.ch == 'i' || ev.ch == 'I') {
                    m_infoTitle = "MacLink Setup Guide";
                    m_infoBody = "1. Press [A] to start CardOS-AP (Pass: cardos1234, IP 192.168.4.1).\n"
                                 "2. Connect your Mac to CardOS-AP Wi-Fi.\n"
                                 "3. On Mac run: python3 cardos_mac_bridge.py\n"
                                 "4. Mac IP defaults to 192.168.4.2 (change with [M]).\n"
                                 "5. Switch to Compute tab [Tab] to run Python/shell on your Mac!";
                    m_infoScroll = 0;
                    m_infoModal = true;
                } else if (ev.ch == '1') { String r; httpBridgeCall("/media?a=playpause", &r, r); showToast(r.c_str(), 1200); }
                else if (ev.ch == '2')   { String r; httpBridgeCall("/media?a=next", &r, r); showToast(r.c_str(), 1200); }
                else if (ev.ch == '3')   { String r; httpBridgeCall("/media?a=voldown", &r, r); showToast(r.c_str(), 1200); }
                else if (ev.ch == '4')   { String r; httpBridgeCall("/media?a=volup", &r, r); showToast(r.c_str(), 1200); }
                else if (ev.ch == '5')   { String r; httpBridgeCall("/media?a=lock", &r, r); showToast(r.c_str(), 1200); }
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "MacLink AP & Compute", true);

        const char* tabs[2] = {"Compute", "AP & Remote"};
        for (int i = 0; i < 2; ++i) {
            int x = 6 + i * 82;
            bool act = (m_tab == i);
            spr.fillRoundRect(x, 18, 78, 12, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.drawString(tabs[i], x + 39, 24);
        }
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(s_apActive ? th.ok : th.textDim);
        spr.drawString(s_apActive ? "AP:ON" : "AP:OFF", SCREEN_W - 6, 20);
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            // Command box
            spr.fillRoundRect(6, 33, SCREEN_W - 12, 16, 4, th.surface2);
            spr.drawRoundRect(6, 33, SCREEN_W - 12, 16, 4, acc);
            spr.setTextColor(acc);
            spr.drawString("$", 10, 37);
            spr.setTextColor(th.text);
            String dc = m_cmd;
            if ((int)dc.length() > 31) dc = dc.substring(dc.length() - 31);
            if ((millis() / 400) % 2 == 0) dc += "_";
            spr.drawString(dc, 19, 37);

            // Wrapped output box
            spr.fillRoundRect(6, 52, SCREEN_W - 12, 68, 4, th.surface);
            spr.drawRoundRect(6, 52, SCREEN_W - 12, 68, 4, th.border);
            spr.setTextColor(th.text);
            ui::drawWrappedText(spr, m_output, 10, 56, SCREEN_W - 26, 60, 10, m_outScroll, &m_outLines);
            if (m_outLines > 6) {
                ui::drawScrollbar(spr, SCREEN_W - 10, 54, 64, m_outLines, 6, m_outScroll);
            }
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent]Run on Mac [Tab]AP/Setup [ESC]Home", 6, 125);
        } else {
            spr.fillRoundRect(6, 33, SCREEN_W - 12, 87, 5, th.surface);
            spr.drawRoundRect(6, 33, SCREEN_W - 12, 87, 5, th.border);
            String st = String("SSID: CardOS-AP  Pass: cardos1234  [A]Toggle\nMac IP: ")
                      + g_settings.macBridgeIp + ":8765 [M]Edit\n"
                      + (m_statsText.length() ? m_statsText : "[R] Read Mac Stats  [I] Full Guide\n[1]Play [2]Next [3/4]Vol [5]Lock");
            spr.setTextColor(th.text);
            ui::drawWrappedText(spr, st, 10, 38, SCREEN_W - 20, 78, 11);
            spr.setTextColor(acc);
            spr.drawString("[A]AP [M]IP [R]Stats [I]Info [1-5]Ctl", 6, 125);
        }

        if (m_editingIp) {
            spr.fillRoundRect(20, 42, SCREEN_W - 40, 48, 5, th.surface2);
            spr.drawRoundRect(20, 42, SCREEN_W - 40, 48, 5, acc);
            spr.setTextColor(acc);
            spr.drawString("Mac Bridge IP:", 28, 48);
            spr.setTextColor(th.text);
            spr.drawString(m_ipBuf + "_", 28, 62);
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent] Save   [ESC] Cancel", 28, 76);
        }

        ui::drawToastOverlay(spr);
        if (m_infoModal) {
            ui::drawTextModal(spr, m_infoTitle.c_str(), m_infoBody, m_infoScroll, &m_infoLines);
        }
    }

private:
    int m_tab = 1; // Start on AP & Setup tab so [A] AP toggle is immediate
    String m_cmd;
    String m_output;
    int m_outScroll = 0;
    int m_outLines = 0;
    String m_statsText;
    bool m_editingIp = false;
    String m_ipBuf;
    bool m_infoModal = false;
    String m_infoTitle;
    String m_infoBody;
    int m_infoScroll = 0;
    int m_infoLines = 0;

    void toggleAp() {
        if (!s_apActive) {
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
            if (WiFi.softAP("CardOS-AP", "cardos1234")) {
                s_apActive = true;
                showToast("AP ON: CardOS-AP", 1600);
            } else {
                showToast("AP start failed", 1400);
            }
        } else {
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            s_apActive = false;
            showToast("CardOS-AP OFF", 1300);
        }
    }

    void fetchMacStats() {
        String raw;
        if (!httpBridgeCall("/stats", nullptr, raw)) {
            m_statsText = raw;
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, raw) == DeserializationError::Ok) {
            m_statsText = String(doc["host"] | "Mac") + " Bat:" + String(doc["battery"] | "?")
                        + "\nLoad: " + String(doc["load"] | "?")
                        + "\n" + String(doc["mem"] | "");
        } else {
            m_statsText = raw;
        }
    }
};

} // namespace

bool macLinkExecCommand(const String& cmd, String& out) {
    String q = "/exec?cmd=" + urlEncodeParam(cmd);
    return httpBridgeCall(q, nullptr, out);
}

App* createMacLinkApp() { return new MacLinkApp(); }

