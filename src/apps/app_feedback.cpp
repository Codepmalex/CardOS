#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../bug_report.h"
#include <WiFi.h>

extern void switchApp(AppId id);

namespace {

class FeedbackApp : public App {
public:
    AppId id() const override { return APP_FEEDBACK; }
    const char* name() const override { return "Ideas"; }

    void onEnter() override {
        m_field = 0;
        m_status = "";
        m_ok = false;
    }

    void onInput(const InputEvent& ev) override {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            switchApp(APP_HOME);
            return;
        }
        if (ev.code == CK_TAB) {
            m_field = (m_field + 1) % 3;
            return;
        }
        if (isNavUp(ev) && m_field > 0) {
            m_field--;
            return;
        }
        if (isNavDown(ev) && m_field < 2) {
            m_field++;
            return;
        }

        if (m_field == 0) {
            if (ev.code == CK_ENTER) {
                m_field = 1;
            } else if (ev.code == CK_BACKSPACE && !m_title.isEmpty()) {
                m_title.remove(m_title.length() - 1);
            } else if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_title.length() < 60) {
                m_title += ev.ch;
            }
        } else if (m_field == 1) {
            if (ev.code == CK_ENTER) {
                m_field = 2;
            } else if (ev.code == CK_BACKSPACE && !m_details.isEmpty()) {
                m_details.remove(m_details.length() - 1);
            } else if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_details.length() < 240) {
                m_details += ev.ch;
            }
        } else if (m_field == 2) {
            if (ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' ')) {
                if (m_title.length() == 0) {
                    m_status = "Enter a feature title first!";
                    m_ok = false;
                    m_field = 0;
                    showToast("Need a title!", 1300);
                    return;
                }
                showToast("Uploading idea...", 1200);
                m_ok = submitFeatureRequestToGithub(m_title, m_details, m_status);
                showToast(m_status.c_str(), 2200);
                if (m_ok) {
                    m_title = "";
                    m_details = "";
                    m_field = 0;
                }
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Feature Ideas", true);

        spr.setTextSize(1);
        // Header status badge
        bool tokReady = hasGithubToken();
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        const char* stLbl = !wifiOk ? "Offline" : (tokReady ? "GH:Ready" : "GH:NoToken");
        uint16_t stCol = !wifiOk ? th.textDim : (tokReady ? th.ok : th.warn);
        spr.setTextColor(th.textDim);
        spr.drawString("Request to GitHub", 8, 20);
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(stCol);
        spr.drawString(stLbl, SCREEN_W - 8, 20);
        spr.setTextDatum(TL_DATUM);

        // Field 0: Title
        bool f0 = (m_field == 0);
        spr.setTextColor(f0 ? acc : th.textDim);
        spr.drawString("Title:", 8, 32);
        spr.fillRoundRect(46, 30, SCREEN_W - 54, 15, 3, f0 ? th.surface2 : th.surface);
        spr.drawRoundRect(46, 30, SCREEN_W - 54, 15, 3, f0 ? acc : th.border);
        spr.setTextColor(th.text);
        String dispT = m_title;
        if ((int)dispT.length() > 28) dispT = dispT.substring(dispT.length() - 28);
        if (f0 && ((millis() / 400) % 2 == 0)) dispT += "_";
        spr.drawString(dispT, 51, 34);

        // Field 1: Details
        bool f1 = (m_field == 1);
        spr.setTextColor(f1 ? acc : th.textDim);
        spr.drawString("Details:", 8, 49);
        spr.fillRoundRect(8, 59, SCREEN_W - 16, 40, 4, f1 ? th.surface2 : th.surface);
        spr.drawRoundRect(8, 59, SCREEN_W - 16, 40, 4, f1 ? acc : th.border);
        spr.setTextColor(th.text);
        String dShow = m_details;
        if (f1 && ((millis() / 400) % 2 == 0)) dShow += "_";
        int y = 63;
        int start = max(0, (int)dShow.length() - 102);
        for (int pos = start; pos < (int)dShow.length() && y < 92; pos += 34) {
            spr.drawString(dShow.substring(pos, min((int)dShow.length(), pos + 34)), 13, y);
            y += 11;
        }

        // Field 2: Submit button + status message
        bool f2 = (m_field == 2);
        spr.fillRoundRect(8, 103, 114, 17, 4, f2 ? acc : th.surface2);
        spr.drawRoundRect(8, 103, 114, 17, 4, f2 ? th.text : acc);
        spr.setTextColor(f2 ? th.bg : acc);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("Submit to GitHub", 65, 111);
        spr.setTextDatum(TL_DATUM);

        if (!m_status.isEmpty()) {
            spr.setTextColor(m_ok ? th.ok : th.warn);
            String ms = m_status;
            if ((int)ms.length() > 18) ms = ms.substring(0, 17) + "~";
            spr.drawString(ms, 128, 107);
        }

        spr.setTextColor(th.textDim);
        spr.drawString("[Tab/Ent]Next [Up/Dn]Move [ESC]Home", 6, 125);
        ui::drawToastOverlay(spr);
    }

private:
    int m_field = 0;
    String m_title;
    String m_details;
    String m_status;
    bool m_ok = false;
};

} // namespace

App* createFeedbackApp() { return new FeedbackApp(); }

