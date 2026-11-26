#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include <M5Cardputer.h>
#include <math.h>

extern void switchApp(AppId id);

namespace {

class GyroApp : public App {
public:
    AppId id() const override { return APP_GYRO; }
    const char* name() const override { return "Gyro"; }

    void onEnter() override {
        m_lastUpdateMs = millis();
        calibrateZero();
    }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (ev.code == CK_CHAR && (ev.ch == 'c' || ev.ch == 'C')) {
            calibrateZero();
            showToast("Zeroed North & Level", 1200);
        }
    }

    void onTick() override {
        uint32_t now = millis();
        float dt = (now - m_lastUpdateMs) / 1000.0f;
        m_lastUpdateMs = now;
        if (dt > 0.2f) dt = 0.02f;

        if (g_sys.imuReady && M5.Imu.update()) {
            M5.Imu.getAccelData(&m_ax, &m_ay, &m_az);
            M5.Imu.getGyroData(&m_gx, &m_gy, &m_gz);

            // Deadband to minimize stationary gyro drift
            float gzCorr = m_gz - m_gzBias;
            if (fabsf(gzCorr) > 0.8f) {
                m_headingDeg -= gzCorr * dt;
                while (m_headingDeg < 0.0f) m_headingDeg += 360.0f;
                while (m_headingDeg >= 360.0f) m_headingDeg -= 360.0f;
            }

            m_pitchDeg = atan2f(m_ax, sqrtf(m_ay * m_ay + m_az * m_az)) * 180.0f / PI;
            m_rollDeg  = atan2f(m_ay, sqrtf(m_ax * m_ax + m_az * m_az)) * 180.0f / PI;
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Gyro / Compass");

        if (!g_sys.imuReady) {
            spr.setTextSize(1);
            spr.setTextColor(th.err);
            spr.drawString("IMU (BMI270) not detected!", 24, 60);
            return;
        }

        // Left: Compass Rose (cx=56, cy=72, r=40)
        int cx = 56, cy = 70, r = 38;
        spr.drawCircle(cx, cy, r, th.border);
        spr.drawCircle(cx, cy, r - 8, th.surface2);

        // N/E/S/W rotated relative to heading
        const char* dirs[4] = {"N", "E", "S", "W"};
        for (int i = 0; i < 4; ++i) {
            float ang = (i * 90.0f - m_headingDeg - 90.0f) * PI / 180.0f;
            int tx = cx + (int)(cosf(ang) * (r - 5));
            int ty = cy + (int)(sinf(ang) * (r - 5));
            spr.setTextSize(1);
            spr.setTextColor(i == 0 ? th.err : th.textDim);
            spr.setTextDatum(MC_DATUM);
            spr.drawString(dirs[i], tx, ty);
        }
        spr.setTextDatum(TL_DATUM);

        // Fixed top pointer triangle + heading readout
        spr.fillTriangle(cx, cy - r - 3, cx - 3, cy - r - 8, cx + 3, cy - r - 8, acc);
        char hbuf[16];
        snprintf(hbuf, sizeof(hbuf), "%3.0f", m_headingDeg);
        spr.setTextSize(2);
        spr.setTextColor(th.text);
        spr.setTextDatum(MC_DATUM);
        spr.drawString(hbuf, cx, cy);
        spr.setTextDatum(TL_DATUM);

        // Right Top: 2D Bubble Level (cx=178, cy=56, r=28)
        int bx = 178, by = 56, br = 26;
        spr.drawCircle(bx, by, br, th.border);
        spr.drawFastHLine(bx - br, by, br * 2, th.surface2);
        spr.drawFastVLine(bx, by - br, br * 2, th.surface2);
        spr.drawCircle(bx, by, 7, th.surface2);

        int dotX = bx + constrain((int)(m_rollDeg * 0.6f), -br + 4, br - 4);
        int dotY = by + constrain((int)(m_pitchDeg * 0.6f), -br + 4, br - 4);
        bool level = (fabsf(m_rollDeg) < 2.5f && fabsf(m_pitchDeg) < 2.5f);
        spr.fillCircle(dotX, dotY, 4, level ? th.ok : acc);

        // Right Bottom: Telemetry
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "P:%+.1f  R:%+.1f", m_pitchDeg, m_rollDeg);
        spr.drawString(tbuf, 134, 90);
        snprintf(tbuf, sizeof(tbuf), "G:%.1f %.1f %.1f", m_gx, m_gy, m_gz);
        spr.drawString(tbuf, 134, 102);

        spr.setTextColor(acc);
        spr.drawString("[C] Calibrate North", 118, 120);
        ui::drawToastOverlay(spr);
    }

private:
    float m_ax = 0, m_ay = 0, m_az = 1;
    float m_gx = 0, m_gy = 0, m_gz = 0;
    float m_gzBias = 0;
    float m_headingDeg = 0;
    float m_pitchDeg = 0, m_rollDeg = 0;
    uint32_t m_lastUpdateMs = 0;

    void calibrateZero() {
        m_headingDeg = 0.0f;
        // Sample gyro-Z bias briefly
        float sum = 0.0f;
        int cnt = 0;
        for (int i = 0; i < 8; ++i) {
            if (g_sys.imuReady && M5.Imu.update()) {
                float gx, gy, gz;
                M5.Imu.getGyroData(&gx, &gy, &gz);
                sum += gz;
                cnt++;
            }
            delay(5);
        }
        if (cnt > 0) m_gzBias = sum / cnt;
    }
};

} // namespace

App* createGyroApp() { return new GyroApp(); }

