#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <ArduinoJson.h>
#include <time.h>
#include <vector>

extern void switchApp(AppId id);

namespace {

struct CalEvent {
    String date; // YYYY-MM-DD
    String title;
};

class CalendarApp : public App {
public:
    AppId id() const override { return APP_CALENDAR; }
    const char* name() const override { return "Calendar"; }

    void onEnter() override {
        ensureDir("/calendar");
        loadEvents();

        struct tm tmInfo;
        if (getSafeLocalTime(tmInfo)) {
            m_todayY = tmInfo.tm_year + 1900;
            m_todayM = tmInfo.tm_mon + 1;
            m_todayD = tmInfo.tm_mday;
        } else {
            m_todayY = 2026; m_todayM = 11; m_todayD = 24;
        }
        m_year = m_todayY;
        m_month = m_todayM;
        m_day = m_todayD;
        m_adding = false;
        updateTodayEventCount();
    }

    void onInput(const InputEvent& ev) override {
        if (m_adding) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
                m_adding = false;
                return;
            }
            if (ev.code == CK_BACKSPACE && !m_newTitle.isEmpty()) {
                m_newTitle.remove(m_newTitle.length() - 1);
                return;
            }
            if (ev.code == CK_ENTER) {
                m_newTitle.trim();
                if (!m_newTitle.isEmpty()) {
                    m_events.push_back({fmtDate(m_year, m_month, m_day), m_newTitle});
                    saveEvents();
                    updateTodayEventCount();
                    showToast("Event added", 1200);
                }
                m_adding = false;
                return;
            }
            if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_newTitle.length() < 26) {
                m_newTitle += ev.ch;
            }
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        int maxD = daysInMonth(m_year, m_month);
        if (isNavLeft(ev)) {
            if (m_day > 1) m_day--;
            else { prevMonth(); m_day = daysInMonth(m_year, m_month); }
        } else if (isNavRight(ev)) {
            if (m_day < maxD) m_day++;
            else { nextMonth(); m_day = 1; }
        } else if (isNavUp(ev)) {
            if (m_day > 7) m_day -= 7;
        } else if (isNavDown(ev)) {
            if (m_day + 7 <= maxD) m_day += 7;
        } else if (ev.code == CK_CHAR && ev.ch == '[') {
            prevMonth();
        } else if (ev.code == CK_CHAR && ev.ch == ']') {
            nextMonth();
        } else if (ev.code == CK_CHAR && (ev.ch == 'a' || ev.ch == 'A' || ev.ch == 'n' || ev.ch == 'N') || ev.code == CK_ENTER) {
            m_newTitle = "";
            m_adding = true;
        } else if (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D')) {
            String key = fmtDate(m_year, m_month, m_day);
            for (int i = (int)m_events.size() - 1; i >= 0; --i) {
                if (m_events[i].date == key) {
                    m_events.erase(m_events.begin() + i);
                    saveEvents();
                    updateTodayEventCount();
                    showToast("Event deleted", 1200);
                    break;
                }
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        static const char* MONS[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "%s %d", MONS[(m_month - 1) % 12], m_year);
        ui::drawStatusBar(spr, hdr);

        // Left side: 7-col calendar grid (x: 4..138)
        static const char* WDAYS[7] = {"S","M","T","W","T","F","S"};
        spr.setTextSize(1);
        for (int c = 0; c < 7; ++c) {
            spr.setTextColor(th.textDim);
            spr.drawString(WDAYS[c], 10 + c * 18, 20);
        }

        int firstWday = dayOfWeek(m_year, m_month, 1);
        int maxD = daysInMonth(m_year, m_month);
        for (int d = 1; d <= maxD; ++d) {
            int pos = firstWday + (d - 1);
            int row = pos / 7;
            int col = pos % 7;
            int x = 5 + col * 18;
            int y = 30 + row * 16;

            bool isSel = (d == m_day);
            bool isToday = (m_year == m_todayY && m_month == m_todayM && d == m_todayD);
            bool hasEv = hasEventsOn(fmtDate(m_year, m_month, d));

            if (isSel) spr.fillRoundRect(x, y, 16, 14, 2, acc);
            else if (isToday) spr.drawRoundRect(x, y, 16, 14, 2, th.ok);

            char ds[4];
            snprintf(ds, sizeof(ds), "%2d", d);
            spr.setTextColor(isSel ? th.bg : (isToday ? th.ok : th.text));
            spr.drawString(ds, x + 2, y + 3);
            if (hasEv) {
                spr.fillCircle(x + 8, y + 12, 1, isSel ? th.bg : th.warn);
            }
        }

        // Right panel: Events for selected day (x: 136..236)
        spr.fillRoundRect(136, 20, 100, 96, 4, th.surface);
        spr.drawRoundRect(136, 20, 100, 96, 4, th.border);
        spr.setTextColor(acc);
        spr.drawString(fmtDate(m_year, m_month, m_day), 142, 25);

        String selKey = fmtDate(m_year, m_month, m_day);
        int ey = 38;
        int shown = 0;
        for (auto& e : m_events) {
            if (e.date == selKey && shown < 5) {
                String t = e.title;
                if ((int)t.length() > 14) t = t.substring(0, 13) + "~";
                spr.setTextColor(th.text);
                spr.drawString("- " + t, 141, ey);
                ey += 13;
                shown++;
            }
        }
        if (shown == 0) {
            spr.setTextColor(th.textDim);
            spr.drawString("No events", 142, 42);
        }

        // Footer
        spr.setTextColor(th.textDim);
        spr.drawString("[/] Mo  [A] Add  [D] Del", 6, 124);

        if (m_adding) {
            spr.fillRoundRect(20, 40, 200, 56, 5, th.surface2);
            spr.drawRoundRect(20, 40, 200, 56, 5, acc);
            spr.setTextColor(acc);
            spr.drawString("New Event (" + selKey + ")", 28, 47);
            spr.setTextColor(th.text);
            spr.drawString(m_newTitle + "_", 28, 63);
            spr.setTextColor(th.textDim);
            spr.drawString("Enter: Save   ESC: Cancel", 28, 80);
        }

        ui::drawToastOverlay(spr);
    }

private:
    int m_year = 2026, m_month = 11, m_day = 24;
    int m_todayY = 2026, m_todayM = 11, m_todayD = 24;
    std::vector<CalEvent> m_events;
    bool m_adding = false;
    String m_newTitle;

    static String fmtDate(int y, int m, int d) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
        return String(buf);
    }

    static int daysInMonth(int y, int m) {
        static const int dm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int d = dm[(m - 1) % 12];
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) d = 29;
        return d;
    }

    static int dayOfWeek(int y, int m, int d) {
        if (m < 3) { m += 12; y--; }
        int k = y % 100;
        int j = y / 100;
        int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
        return (h + 6) % 7; // 0=Sun..6=Sat
    }

    void prevMonth() {
        m_month--;
        if (m_month < 1) { m_month = 12; m_year--; }
        m_day = min(m_day, daysInMonth(m_year, m_month));
    }
    void nextMonth() {
        m_month++;
        if (m_month > 12) { m_month = 1; m_year++; }
        m_day = min(m_day, daysInMonth(m_year, m_month));
    }

    bool hasEventsOn(const String& key) const {
        for (auto& e : m_events) if (e.date == key) return true;
        return false;
    }

    void updateTodayEventCount() {
        String key = fmtDate(m_todayY, m_todayM, m_todayD);
        int cnt = 0;
        for (auto& e : m_events) if (e.date == key) cnt++;
        g_sys.todayEventsCount = cnt;
    }

    void loadEvents() {
        m_events.clear();
        String raw = readTextFile("/calendar/events.json", 8192);
        if (raw.isEmpty()) return;
        JsonDocument doc;
        if (deserializeJson(doc, raw) != DeserializationError::Ok) return;
        for (JsonObject o : doc.as<JsonArray>()) {
            CalEvent ev;
            ev.date = String(o["d"] | "");
            ev.title = String(o["t"] | "");
            if (!ev.date.isEmpty() && !ev.title.isEmpty()) m_events.push_back(ev);
        }
    }

    void saveEvents() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (auto& e : m_events) {
            JsonObject o = arr.add<JsonObject>();
            o["d"] = e.date;
            o["t"] = e.title;
        }
        String out;
        serializeJson(doc, out);
        writeTextFile("/calendar/events.json", out);
    }
};

} // namespace

App* createCalendarApp() { return new CalendarApp(); }

