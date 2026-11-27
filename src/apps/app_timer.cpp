#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../hardware.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

// Background-persistent timer state so Countdown & Pomodoro keep running across apps
static bool     s_swRunning = false;
static uint32_t s_swAccumMs = 0;
static uint32_t s_swStartMs = 0;
static std::vector<uint32_t> s_swLaps;

static bool     s_cdRunning = false;
static uint32_t s_cdSetSec  = 300;
static uint32_t s_cdRemMs   = 300000;
static uint32_t s_cdLastMs  = 0;

static bool     s_pmRunning  = false;
static bool     s_pmBreak    = false;
static int      s_pmSessions = 0;
static uint32_t s_pmRemMs    = 25UL * 60000UL;
static uint32_t s_pmLastMs   = 0;

static uint32_t swCurrentMs() {
    return s_swRunning ? (s_swAccumMs + (millis() - s_swStartMs)) : s_swAccumMs;
}

class TimerApp : public App {
public:
    AppId id() const override { return APP_TIMER; }
    const char* name() const override { return "Timer"; }

    void onEnter() override {}

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavLeft(ev)) {
            m_tab = (m_tab + 2) % 3;
            return;
        }
        if (isNavRight(ev) || ev.code == CK_TAB) {
            m_tab = (m_tab + 1) % 3;
            return;
        }

        char ch = (ev.code == CK_CHAR) ? (char)tolower((unsigned char)ev.ch) : 0;
        bool toggleKey = (ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' '));

        if (m_tab == 0) {
            // Stopwatch
            if (toggleKey) {
                if (s_swRunning) {
                    s_swAccumMs += millis() - s_swStartMs;
                    s_swRunning = false;
                } else {
                    s_swStartMs = millis();
                    s_swRunning = true;
                }
            } else if (ch == 'l') {
                uint32_t cur = swCurrentMs();
                if (cur > 0) {
                    s_swLaps.insert(s_swLaps.begin(), cur);
                    if (s_swLaps.size() > 6) s_swLaps.resize(6);
                }
            } else if (ch == 'r') {
                s_swRunning = false;
                s_swAccumMs = 0;
                std::vector<uint32_t>().swap(s_swLaps);
                showToast("Stopwatch reset", 800);
            }
        } else if (m_tab == 1) {
            // Countdown
            if (toggleKey) {
                if (s_cdRunning) {
                    s_cdRunning = false;
                } else {
                    if (s_cdRemMs == 0) s_cdRemMs = s_cdSetSec * 1000UL;
                    s_cdLastMs = millis();
                    s_cdRunning = true;
                }
            } else if (!s_cdRunning && isNavUp(ev)) {
                s_cdSetSec = min((uint32_t)5940, s_cdSetSec + 60);
                s_cdRemMs = s_cdSetSec * 1000UL;
            } else if (!s_cdRunning && isNavDown(ev)) {
                s_cdSetSec = (s_cdSetSec > 60) ? (s_cdSetSec - 60) : 15;
                s_cdRemMs = s_cdSetSec * 1000UL;
            } else if (!s_cdRunning && (ch == '=' || ch == '+' || ch == 'w')) {
                s_cdSetSec = min((uint32_t)5940, s_cdSetSec + 15);
                s_cdRemMs = s_cdSetSec * 1000UL;
            } else if (!s_cdRunning && (ch == '-' || ch == '_' || ch == 's')) {
                s_cdSetSec = (s_cdSetSec > 15) ? (s_cdSetSec - 15) : 15;
                s_cdRemMs = s_cdSetSec * 1000UL;
            } else if (ch == 'r') {
                s_cdRunning = false;
                s_cdRemMs = s_cdSetSec * 1000UL;
                showToast("Countdown reset", 800);
            }
        } else {
            // Pomodoro (25m focus / 5m break)
            if (toggleKey) {
                if (s_pmRunning) {
                    s_pmRunning = false;
                } else {
                    s_pmLastMs = millis();
                    s_pmRunning = true;
                }
            } else if (ch == 's') {
                s_pmBreak = !s_pmBreak;
                s_pmRemMs = (s_pmBreak ? 5UL : 25UL) * 60000UL;
                s_pmLastMs = millis();
                showToast(s_pmBreak ? "Switched to 5m Break" : "Switched to 25m Focus", 1000);
            } else if (ch == 'r') {
                s_pmRunning = false;
                s_pmBreak = false;
                s_pmRemMs = 25UL * 60000UL;
                showToast("Pomodoro reset", 800);
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Timer");

        // Tabs (y: 18..31)
        static const char* TABS[3] = {"Stopwatch", "Countdown", "Pomodoro"};
        int tw = (SCREEN_W - 16) / 3;
        spr.setTextSize(1);
        spr.setTextDatum(MC_DATUM);
        for (int i = 0; i < 3; ++i) {
            int tx = 8 + i * tw;
            bool sel = (i == m_tab);
            spr.fillRoundRect(tx + 1, 18, tw - 2, 13, 3, sel ? acc : th.surface);
            spr.setTextColor(sel ? th.bg : th.textDim);
            spr.drawString(TABS[i], tx + tw / 2, 24);
        }
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            uint32_t ms = swCurrentMs();
            uint32_t mins = (ms / 60000UL) % 100;
            uint32_t secs = (ms / 1000UL) % 60;
            uint32_t tenths = (ms / 100UL) % 10;
            char tbuf[20];
            snprintf(tbuf, sizeof(tbuf), "%02lu:%02lu.%lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)tenths);

            spr.fillRoundRect(10, 35, SCREEN_W - 20, 44, 6, th.surface);
            spr.drawRoundRect(10, 35, SCREEN_W - 20, 44, 6, s_swRunning ? th.ok : th.border);
            spr.setTextDatum(MC_DATUM);
            spr.setTextSize(3);
            spr.setTextColor(s_swRunning ? th.ok : th.text);
            spr.drawString(tbuf, SCREEN_W / 2, 57);

            spr.setTextSize(1);
            spr.setTextDatum(TL_DATUM);
            if (s_swLaps.empty()) {
                spr.setTextColor(th.textDim);
                spr.drawString("No laps yet ([L] saves a split)", 14, 86);
            } else {
                for (int i = 0; i < (int)s_swLaps.size() && i < 4; ++i) {
                    uint32_t lms = s_swLaps[i];
                    char lb[24];
                    snprintf(lb, sizeof(lb), "#%d %02lu:%02lu.%lu",
                             (int)s_swLaps.size() - i,
                             (unsigned long)((lms / 60000UL) % 100),
                             (unsigned long)((lms / 1000UL) % 60),
                             (unsigned long)((lms / 100UL) % 10));
                    int col = i % 2;
                    int row = i / 2;
                    spr.setTextColor(i == 0 ? acc : th.text);
                    spr.drawString(lb, 16 + col * 112, 84 + row * 12);
                }
            }
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent/Spc]Start/Stop [L]Lap [R]Reset", 6, 124);
        } else if (m_tab == 1) {
            uint32_t totSec = (s_cdRemMs + 999UL) / 1000UL;
            uint32_t mins = totSec / 60;
            uint32_t secs = totSec % 60;
            char tbuf[16];
            snprintf(tbuf, sizeof(tbuf), "%02lu:%02lu", (unsigned long)mins, (unsigned long)secs);

            spr.fillRoundRect(10, 35, SCREEN_W - 20, 52, 6, th.surface);
            spr.drawRoundRect(10, 35, SCREEN_W - 20, 52, 6, s_cdRunning ? acc : th.border);
            spr.setTextDatum(MC_DATUM);
            spr.setTextSize(3);
            spr.setTextColor(s_cdRunning ? acc : th.text);
            spr.drawString(tbuf, SCREEN_W / 2, 56);

            // Progress bar
            int bx = 22, by = 75, bw = SCREEN_W - 44, bh = 6;
            spr.fillRoundRect(bx, by, bw, bh, 2, th.bg);
            uint32_t fullMs = max<uint32_t>(1000U, s_cdSetSec * 1000U);
            int fw = (int)(((uint64_t)bw * s_cdRemMs) / fullMs);
            if (fw > 0) spr.fillRoundRect(bx, by, fw, bh, 2, s_cdRunning ? th.ok : acc);

            spr.setTextSize(1);
            spr.setTextColor(s_cdRunning ? th.ok : th.textDim);
            spr.drawString(s_cdRunning ? "RUNNING (stays active in background)" : "PAUSED  Up/Dn:+/-1m  +/-:+/-15s", SCREEN_W / 2, 102);
            spr.setTextDatum(TL_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent/Spc]Start [R]Reset [</>]Tab", 6, 124);
        } else {
            uint32_t totSec = (s_pmRemMs + 999UL) / 1000UL;
            uint32_t mins = totSec / 60;
            uint32_t secs = totSec % 60;
            char tbuf[16];
            snprintf(tbuf, sizeof(tbuf), "%02lu:%02lu", (unsigned long)mins, (unsigned long)secs);

            uint16_t pCol = s_pmBreak ? th.ok : acc;
            spr.fillRoundRect(10, 35, SCREEN_W - 20, 54, 6, th.surface);
            spr.drawRoundRect(10, 35, SCREEN_W - 20, 54, 6, s_pmRunning ? pCol : th.border);
            spr.setTextDatum(MC_DATUM);
            spr.setTextSize(1);
            spr.setTextColor(pCol);
            char st[36];
            snprintf(st, sizeof(st), "%s  |  Sessions: %d", s_pmBreak ? "BREAK TIME (5m)" : "FOCUS TIME (25m)", s_pmSessions);
            spr.drawString(st, SCREEN_W / 2, 44);

            spr.setTextSize(3);
            spr.setTextColor(th.text);
            spr.drawString(tbuf, SCREEN_W / 2, 63);

            int bx = 22, by = 79, bw = SCREEN_W - 44, bh = 6;
            spr.fillRoundRect(bx, by, bw, bh, 2, th.bg);
            uint32_t fullMs = (s_pmBreak ? 5UL : 25UL) * 60000UL;
            int fw = (int)(((uint64_t)bw * s_pmRemMs) / fullMs);
            if (fw > 0) spr.fillRoundRect(bx, by, fw, bh, 2, pCol);

            spr.setTextSize(1);
            spr.setTextColor(s_pmRunning ? th.ok : th.textDim);
            spr.drawString(s_pmRunning ? "RUNNING (auto-notifies on switch)" : "PAUSED", SCREEN_W / 2, 103);
            spr.setTextDatum(TL_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent/Spc]Start [S]Skip [R]Reset", 6, 124);
        }

        ui::drawToastOverlay(spr);
    }

private:
    int m_tab = 0;
};

} // namespace

void tickTimerService() {
    uint32_t now = millis();
    if (s_cdRunning) {
        uint32_t dt = now - s_cdLastMs;
        s_cdLastMs = now;
        if (dt >= s_cdRemMs) {
            s_cdRemMs = 0;
            s_cdRunning = false;
            triggerNotification(NOTIF_SYSTEM, "Countdown finished!");
        } else {
            s_cdRemMs -= dt;
        }
    }
    if (s_pmRunning) {
        uint32_t dt = now - s_pmLastMs;
        s_pmLastMs = now;
        if (dt >= s_pmRemMs) {
            if (!s_pmBreak) s_pmSessions++;
            s_pmBreak = !s_pmBreak;
            s_pmRemMs = (s_pmBreak ? 5UL : 25UL) * 60000UL;
            triggerNotification(NOTIF_SYSTEM, s_pmBreak ? "Focus done! 5m Break" : "Break done! Focus time");
        } else {
            s_pmRemMs -= dt;
        }
    }
}

App* createTimerApp() { return new TimerApp(); }
