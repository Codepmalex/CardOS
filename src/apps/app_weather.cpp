#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../net_weather.h"

extern void switchApp(AppId id);

namespace {

static const char* compassDir(int deg) {
    static const char* D[8] = {"N","NE","E","SE","S","SW","W","NW"};
    return D[((deg + 22) % 360) / 45];
}

class WeatherApp : public App {
public:
    AppId id() const override { return APP_WEATHER; }
    const char* name() const override { return "Weather"; }

    void onEnter() override {
        m_editingCity = false;
        m_tab = 0;
        m_hourPage = 0;
        if (g_sys.wifiConnected && (!g_sys.weather.valid || g_sys.weather.lastFetchMs == 0) && !g_sys.weather.fetching) {
            requestWeatherRefresh();
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_editingCity) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
                m_editingCity = false;
                return;
            }
            if (ev.code == CK_BACKSPACE && !m_cityInput.isEmpty()) {
                m_cityInput.remove(m_cityInput.length() - 1);
                return;
            }
            if (ev.code == CK_ENTER) {
                m_cityInput.trim();
                if (m_cityInput.isEmpty()) m_cityInput = "Auto";
                strlcpy(g_settings.weatherCity, m_cityInput.c_str(), sizeof(g_settings.weatherCity));
                g_settings.weatherLat = 0.0f;
                g_settings.weatherLon = 0.0f;
                saveSettings();
                requestWeatherRefresh();
                m_editingCity = false;
                showToast("Updating location...", 1500);
                return;
            }
            if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_cityInput.length() < 30) {
                m_cityInput += ev.ch;
            }
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavLeft(ev)) {
            m_tab = (m_tab + 2) % 3;
        } else if (isNavRight(ev)) {
            m_tab = (m_tab + 1) % 3;
        } else if (isNavUp(ev) && m_tab == 1) {
            m_hourPage = 0;
        } else if (isNavDown(ev) && m_tab == 1) {
            m_hourPage = 1;
        } else if (ev.code == CK_CHAR && ev.ch == '1') { m_tab = 0; }
          else if (ev.code == CK_CHAR && ev.ch == '2') { m_tab = 1; }
          else if (ev.code == CK_CHAR && ev.ch == '3') { m_tab = 2; }
        else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            requestWeatherRefresh();
            showToast("Refreshing weather...", 1400);
        } else if (ev.code == CK_CHAR && (ev.ch == 'l' || ev.ch == 'L')) {
            m_cityInput = g_settings.weatherCity;
            m_editingCity = true;
        } else if (ev.code == CK_CHAR && (ev.ch == 'u' || ev.ch == 'U')) {
            g_settings.tempFahrenheit = !g_settings.tempFahrenheit;
            saveSettings();
        } else if (ev.code == CK_CHAR && (ev.ch == 'w' || ev.ch == 'W')) {
            switchApp(APP_WIFI);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Weather");

        if (m_editingCity) {
            spr.drawRoundRect(14, 34, SCREEN_W - 28, 66, 5, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("City Name (or 'Auto' for IP):", 22, 43);
            spr.fillRoundRect(22, 58, SCREEN_W - 44, 18, 3, th.surface2);
            spr.setTextColor(th.text);
            spr.drawString(m_cityInput + "_", 27, 63);
            spr.setTextColor(th.textDim);
            spr.drawString("Enter: Save   ESC: Cancel", 22, 84);
            return;
        }

        WeatherData w;
        if (xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(10))) {
            w = g_sys.weather;
            xSemaphoreGive(g_sysMutex);
        }

        if (!w.valid && !g_sys.wifiConnected) {
            spr.fillRoundRect(8, 24, SCREEN_W - 16, 84, 5, th.surface);
            spr.drawRoundRect(8, 24, SCREEN_W - 16, 84, 5, th.border);
            spr.setTextSize(1);
            spr.setTextColor(th.warn);
            spr.drawString("Wi-Fi Not Connected", 16, 32);
            spr.setTextColor(th.textDim);
            spr.drawString("Weather uses Open-Meteo over Wi-Fi", 16, 50);
            spr.drawString("(100% free, no API key required).", 16, 62);
            spr.setTextColor(acc);
            spr.drawString("Press [W] to open Wi-Fi Setup", 16, 82);
            ui::drawToastOverlay(spr);
            return;
        }

        if (!w.valid) {
            spr.setTextSize(1);
            spr.setTextColor(th.text);
            spr.drawString(w.fetching ? "Fetching live weather..." : "Waiting for weather...", 14, 46);
            if (strlen(w.errorMsg) > 0) {
                spr.setTextColor(th.err);
                spr.drawString(w.errorMsg, 14, 66);
            }
            spr.setTextColor(th.textDim);
            spr.drawString("[R] Retry   [L] City   [W] Wi-Fi", 14, 118);
            ui::drawToastOverlay(spr);
            return;
        }

        // Tab Strip (y: 18..31)
        const char* tabNames[3] = {"Now", "Hourly", "5-Day"};
        for (int t = 0; t < 3; ++t) {
            int tx = 6 + t * 48;
            bool act = (t == m_tab);
            spr.fillRoundRect(tx, 18, 45, 13, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.setTextDatum(MC_DATUM);
            spr.drawString(tabNames[t], tx + 22, 24);
            spr.setTextDatum(TL_DATUM);
        }
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.setTextDatum(TR_DATUM);
        spr.drawString(w.city, SCREEN_W - 6, 21);
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            renderNowTab(spr, w, th, acc);
        } else if (m_tab == 1) {
            renderHourlyTab(spr, w, th, acc);
        } else {
            render5DayTab(spr, w, th, acc);
        }

        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(m_tab == 1 ? "</>Tab [R]Sync [U]F/C" : "</>Tab [R]Sync [L]City [U]F/C", 6, 125);
        ui::drawToastOverlay(spr);
    }

private:
    bool m_editingCity = false;
    String m_cityInput;
    int m_tab = 0;
    int m_hourPage = 0;

    void renderNowTab(M5Canvas& spr, const WeatherData& w, const ThemePalette& th, uint16_t acc) {
        // Left Main Card (6..114, y: 34..120)
        spr.fillRoundRect(6, 34, 108, 86, 5, th.surface);
        spr.drawRoundRect(6, 34, 108, 86, 5, th.border);
        ui::drawWeatherIcon(spr, 26, 56, 11, w.weatherCode, w.isDay);

        spr.setTextSize(2);
        spr.setTextColor(th.text);
        spr.drawString(ui::formatTemp(w.tempC), 46, 40);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString(weatherCodeToShort(w.weatherCode), 46, 60);

        spr.setTextColor(th.textDim);
        spr.drawString("Feels " + ui::formatTemp(w.feelsLikeC), 12, 76);
        String hiLo = "H:" + ui::formatTemp(w.forecast[0].tempMaxC, false) +
                      " L:" + ui::formatTemp(w.forecast[0].tempMinC, false);
        spr.drawString(hiLo, 12, 90);
        spr.setTextColor(w.lastFetchMs > 0 ? th.ok : th.warn);
        spr.drawString(w.lastFetchMs > 0 ? "Live" : "Cached", 12, 104);

        // Right 2x3 Metric Cards (x: 118..234, y: 34..120)
        char b0[16], b1[16], b2[16], b3[16], b4[16], b5[16];
        snprintf(b0, sizeof(b0), "%d%%", w.humidity);
        snprintf(b1, sizeof(b1), "%.0f %s", w.windKmh, compassDir(w.windDirDeg));
        snprintf(b2, sizeof(b2), "%d%%", w.precipProbNow);
        snprintf(b3, sizeof(b3), "%.0f", w.pressureHpa);
        snprintf(b4, sizeof(b4), "%.1f", w.uvMax);
        snprintf(b5, sizeof(b5), "%s", w.sunrise);

        drawMiniMetric(spr, 118, 34, 56, 27, "Hum", b0, th, acc);
        drawMiniMetric(spr, 178, 34, 56, 27, "Wind", b1, th, acc);
        drawMiniMetric(spr, 118, 63, 56, 27, "Rain%", b2, th, rgb565(56, 189, 248));
        drawMiniMetric(spr, 178, 63, 56, 27, "hPa", b3, th, acc);
        drawMiniMetric(spr, 118, 92, 56, 27, "UV", b4, th, th.warn);
        drawMiniMetric(spr, 178, 92, 56, 27, "Sun", b5, th, th.text);
    }

    void drawMiniMetric(M5Canvas& spr, int x, int y, int w, int h, const char* lbl, const char* val, const ThemePalette& th, uint16_t valCol) {
        spr.fillRoundRect(x, y, w, h, 3, th.surface);
        spr.drawRoundRect(x, y, w, h, 3, th.border);
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString(lbl, x + 4, y + 3);
        spr.setTextColor(valCol);
        spr.drawString(val, x + 4, y + 15);
    }

    void renderHourlyTab(M5Canvas& spr, const WeatherData& w, const ThemePalette& th, uint16_t acc) {
        if (w.hourlyCount == 0) {
            spr.setTextSize(1);
            spr.setTextColor(th.textDim);
            spr.drawString("No hourly data yet. Press [R].", 16, 62);
            return;
        }
        int start = (m_hourPage == 0) ? 0 : 6;
        int cw = 37;
        for (int i = 0; i < 6; ++i) {
            int idx = start + i;
            if (idx >= w.hourlyCount) break;
            int x = 6 + i * (cw + 1);
            int y = 34;
            const auto& h = w.hourly[idx];

            spr.fillRoundRect(x, y, cw, 86, 4, th.surface);
            spr.drawRoundRect(x, y, cw, 86, 4, th.border);

            spr.setTextSize(1);
            spr.setTextDatum(TC_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString(h.label, x + cw / 2, y + 4);

            ui::drawWeatherIcon(spr, x + cw / 2, y + 24, 6, h.weatherCode, true);

            spr.setTextColor(th.text);
            spr.drawString(ui::formatTemp(h.tempC, false), x + cw / 2, y + 38);

            // Precip probability bar (bottom of card)
            int barH = (constrain(h.precipProb, 0, 100) * 20) / 100;
            spr.fillRect(x + 12, y + 50, 13, 20, th.bg);
            if (barH > 0) {
                spr.fillRect(x + 12, y + 70 - barH, 13, barH, rgb565(56, 189, 248));
            }
            spr.setTextColor(rgb565(56, 189, 248));
            spr.drawString(String(h.precipProb) + "%", x + cw / 2, y + 74);
            spr.setTextDatum(TL_DATUM);
        }
        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.setTextDatum(TR_DATUM);
        spr.drawString(m_hourPage == 0 ? "1-6h [Dn]" : "7-12h [Up]", SCREEN_W - 6, 125);
        spr.setTextDatum(TL_DATUM);
    }

    void render5DayTab(M5Canvas& spr, const WeatherData& w, const ThemePalette& th, uint16_t acc) {
        int cw = 44;
        for (int i = 0; i < 5; ++i) {
            int cx = 6 + i * (cw + 2);
            int y = 34;
            const auto& d = w.forecast[i];
            spr.fillRoundRect(cx, y, cw, 86, 4, th.surface);
            spr.drawRoundRect(cx, y, cw, 86, 4, th.border);

            spr.setTextSize(1);
            spr.setTextDatum(TC_DATUM);
            spr.setTextColor(i == 0 ? acc : th.textDim);
            spr.drawString(d.dayLabel, cx + cw / 2, y + 5);

            ui::drawWeatherIcon(spr, cx + cw / 2, y + 28, 7, d.weatherCode, true);

            spr.setTextColor(th.text);
            spr.drawString(ui::formatTemp(d.tempMaxC, false) + "H", cx + cw / 2, y + 44);
            spr.setTextColor(th.textDim);
            spr.drawString(ui::formatTemp(d.tempMinC, false) + "L", cx + cw / 2, y + 56);

            spr.setTextColor(rgb565(56, 189, 248));
            spr.drawString(String(d.precipProbMax) + "%", cx + cw / 2, y + 72);
            spr.setTextDatum(TL_DATUM);
        }
    }
};

} // namespace

App* createWeatherApp() { return new WeatherApp(); }
