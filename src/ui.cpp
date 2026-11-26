#include "ui.h"
#include "state.h"
#include <time.h>

namespace ui {

String formatTemp(float tempC, bool withUnit) {
    float v = g_settings.tempFahrenheit ? (tempC * 9.0f / 5.0f + 32.0f) : tempC;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%s", v, withUnit ? (g_settings.tempFahrenheit ? "F" : "C") : "");
    return String(buf);
}

int drawStatusIcons(M5Canvas& spr, bool showTime) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();

    spr.setTextSize(1);
    spr.setTextDatum(TL_DATUM);

    // Right side: Battery + Wi-Fi + Notification dot + Profile + optional Time
    int rx = SCREEN_W - 4;

    // Battery icon (18x9)
    int bw = 18, bh = 9;
    rx -= bw;
    int by = 3;
    uint16_t batCol = (g_sys.batteryPct <= 15) ? th.err : (g_sys.batteryPct <= 35 ? th.warn : th.ok);
    spr.drawRect(rx, by, bw - 2, bh, th.textDim);
    spr.fillRect(rx + bw - 2, by + 2, 2, bh - 4, th.textDim);
    int fillW = ((bw - 4) * constrain(g_sys.batteryPct, 0, 100)) / 100;
    if (fillW > 0) {
        spr.fillRect(rx + 1, by + 1, fillW, bh - 2, batCol);
    }
    if (g_sys.isCharging) {
        // Crisp lightning bolt over battery icon
        int lx = rx + (bw - 2) / 2;
        uint16_t boltCol = rgb565(250, 204, 21);
        spr.drawLine(lx + 1, by + 1, lx - 2, by + 4, boltCol);
        spr.drawFastHLine(lx - 2, by + 4, 4, boltCol);
        spr.drawLine(lx + 1, by + 4, lx - 1, by + 7, boltCol);
    }
    rx -= 3;

    // Battery %
    char batStr[8];
    snprintf(batStr, sizeof(batStr), "%d%%", g_sys.batteryPct);
    int batTxtW = strlen(batStr) * 6;
    rx -= batTxtW;
    spr.setTextColor(th.textDim);
    spr.drawString(batStr, rx, 4);
    rx -= 5;

    // Wi-Fi indicator (arcs/bars)
    if (g_sys.wifiConnected) {
        spr.fillCircle(rx - 3, 10, 1, th.ok);
        spr.drawArc(rx - 3, 10, 4, 4, 225, 315, th.ok);
        spr.drawArc(rx - 3, 10, 7, 7, 225, 315, th.ok);
    } else if (g_sys.wifiConnecting) {
        spr.fillCircle(rx - 3, 8, 2, th.warn);
    } else {
        spr.drawLine(rx - 6, 5, rx, 11, th.textDim);
        spr.drawLine(rx, 5, rx - 6, 11, th.textDim);
    }
    rx -= 12;

    // Calendar/Notification dot
    if (g_settings.notifEnabled && g_sys.todayEventsCount > 0) {
        spr.fillCircle(rx, 8, 2, acc);
        rx -= 7;
    }

    // Device profile badge when Silent / Anonymous
    if (g_settings.deviceProfile != PROF_SOUND) {
        const char* pTxt = (g_settings.deviceProfile == PROF_ANON) ? "ANON" : "SIL";
        int pw = strlen(pTxt) * 6;
        rx -= pw;
        spr.setTextColor(g_settings.deviceProfile == PROF_ANON ? th.err : th.warn);
        spr.drawString(pTxt, rx, 4);
        rx -= 5;
    }

    // Low Battery ECO badge
    if (g_settings.lowPowerMode) {
        rx -= 18;
        spr.setTextColor(th.ok);
        spr.drawString("ECO", rx, 4);
        rx -= 4;
    }

    // Time (with AM/PM when 12h mode is configured)
    if (showTime) {
        struct tm tmInfo;
        if (getSafeLocalTime(tmInfo)) {
            int h = tmInfo.tm_hour;
            char tStr[10];
            if (g_settings.use24h) {
                snprintf(tStr, sizeof(tStr), "%02d:%02d", h, tmInfo.tm_min);
            } else {
                const char* ampm = (h >= 12) ? "PM" : "AM";
                h = h % 12;
                if (h == 0) h = 12;
                snprintf(tStr, sizeof(tStr), "%d:%02d", h, tmInfo.tm_min);
                rx -= 12;
                spr.setTextColor(acc);
                spr.drawString(ampm, rx, 4);
                rx -= 1;
            }
            int tw = strlen(tStr) * 6;
            rx -= tw;
            spr.setTextColor(th.text);
            spr.drawString(tStr, rx, 4);
        } else {
            const char* tStr = "--:--";
            int tw = 30;
            rx -= tw;
            spr.setTextColor(th.text);
            spr.drawString(tStr, rx, 4);
        }
    }

    return rx;
}

void drawStatusBar(M5Canvas& spr, const char* title, bool showTime) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();

    spr.fillRect(0, 0, SCREEN_W, STATUS_BAR_H, th.surface);
    spr.drawFastHLine(0, STATUS_BAR_H - 1, SCREEN_W, th.border);

    int rightEdge = drawStatusIcons(spr, showTime);

    String t = title ? String(title) : String("");
    int maxChars = max(1, (rightEdge - 9) / 6);
    if ((int)t.length() > maxChars) {
        t = t.substring(0, max(1, maxChars - 1)) + "~";
    }
    spr.setTextSize(1);
    spr.setTextColor(acc);
    spr.drawString(t, 5, 4);
}

void drawToastOverlay(M5Canvas& spr) {
    uint32_t now = millis();
    if (now >= g_sys.toastUntilMs || strlen(g_sys.toastMsg) == 0) return;
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();

    int msgLen = strlen(g_sys.toastMsg);
    int w = min(SCREEN_W - 16, max(80, msgLen * 6 + 18));
    int h = 18;
    int x = (SCREEN_W - w) / 2;

    // Smooth slide-in (110ms) and slide-out (110ms)
    int slideOffset = 0;
    uint32_t elapsed = (now >= g_sys.toastStartMs) ? (now - g_sys.toastStartMs) : 110;
    uint32_t rem = g_sys.toastUntilMs - now;
    if (elapsed < 110) {
        slideOffset = (int)(((110 - elapsed) * 10) / 110);
    } else if (rem < 110) {
        slideOffset = (int)(((110 - rem) * 10) / 110);
    }
    int y = SCREEN_H - h - 4 + slideOffset;

    spr.fillRoundRect(x, y, w, h, 4, th.surface2);
    spr.drawRoundRect(x, y, w, h, 4, acc);
    spr.setTextSize(1);
    spr.setTextColor(th.text);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(g_sys.toastMsg, SCREEN_W / 2, y + h / 2);
    spr.setTextDatum(TL_DATUM);
}

void drawWeatherIcon(M5Canvas& spr, int cx, int cy, int r, int code, bool isDay) {
    uint16_t sunCol   = rgb565(250, 204, 21);
    uint16_t cloudCol = rgb565(203, 213, 225);
    uint16_t rainCol  = rgb565(56, 189, 248);
    uint16_t snowCol  = rgb565(240, 249, 255);

    if (code <= 1) {
        // Clear / Mainly clear
        uint16_t col = isDay ? sunCol : rgb565(147, 197, 253);
        spr.fillCircle(cx, cy, r / 2, col);
        for (int i = 0; i < 8; ++i) {
            float a = i * (PI / 4.0f);
            int x1 = cx + (int)(cosf(a) * (r * 0.65f));
            int y1 = cy + (int)(sinf(a) * (r * 0.65f));
            int x2 = cx + (int)(cosf(a) * (r * 0.95f));
            int y2 = cy + (int)(sinf(a) * (r * 0.95f));
            spr.drawLine(x1, y1, x2, y2, col);
        }
        return;
    }

    if (code == 2) {
        // Partly cloudy
        spr.fillCircle(cx - r / 3, cy - r / 3, r / 3, sunCol);
    }

    // Cloud base
    int cw = r;
    spr.fillCircle(cx - cw / 3, cy + 1, cw / 3, cloudCol);
    spr.fillCircle(cx + cw / 4, cy + 1, cw / 3, cloudCol);
    spr.fillCircle(cx, cy - cw / 5, cw / 2 - 1, cloudCol);
    spr.fillRect(cx - cw / 3, cy + 1, (cw * 2) / 3, cw / 3, cloudCol);

    if (code >= 51 && code <= 67 || (code >= 80 && code <= 82)) {
        // Rain drops
        spr.drawLine(cx - 4, cy + cw / 2, cx - 6, cy + cw / 2 + 4, rainCol);
        spr.drawLine(cx + 1, cy + cw / 2, cx - 1, cy + cw / 2 + 4, rainCol);
        spr.drawLine(cx + 6, cy + cw / 2, cx + 4, cy + cw / 2 + 4, rainCol);
    } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        spr.fillCircle(cx - 4, cy + cw / 2 + 2, 1, snowCol);
        spr.fillCircle(cx + 1, cy + cw / 2 + 3, 1, snowCol);
        spr.fillCircle(cx + 5, cy + cw / 2 + 1, 1, snowCol);
    } else if (code >= 95) {
        // Lightning bolt
        spr.drawLine(cx + 1, cy + 2, cx - 2, cy + cw / 2 + 2, sunCol);
        spr.drawLine(cx - 2, cy + cw / 2 + 2, cx + 2, cy + cw / 2 + 2, sunCol);
        spr.drawLine(cx + 2, cy + cw / 2 + 2, cx - 1, cy + cw / 2 + 6, sunCol);
    }
}

void drawAppIcon(M5Canvas& spr, int cx, int cy, AppId id, uint16_t color, bool isBinApp) {
    if (isBinApp) {
        // SD / Chip icon
        spr.drawRoundRect(cx - 7, cy - 7, 14, 14, 2, color);
        spr.fillRect(cx - 4, cy - 4, 8, 8, color);
        spr.drawFastVLine(cx - 3, cy - 9, 2, color);
        spr.drawFastVLine(cx + 3, cy - 9, 2, color);
        spr.drawFastVLine(cx - 3, cy + 7, 2, color);
        spr.drawFastVLine(cx + 3, cy + 7, 2, color);
        return;
    }

    switch (id) {
        case APP_NOTES:
            spr.drawRoundRect(cx - 6, cy - 7, 12, 14, 2, color);
            spr.drawFastHLine(cx - 3, cy - 3, 6, color);
            spr.drawFastHLine(cx - 3, cy,     6, color);
            spr.drawFastHLine(cx - 3, cy + 3, 4, color);
            break;
        case APP_CALENDAR:
            spr.drawRoundRect(cx - 7, cy - 6, 14, 13, 2, color);
            spr.fillRect(cx - 7, cy - 6, 14, 4, color);
            spr.fillRect(cx - 4, cy, 3, 3, color);
            spr.fillRect(cx + 1, cy, 3, 3, color);
            break;
        case APP_CALC:
            spr.drawRoundRect(cx - 6, cy - 7, 12, 14, 2, color);
            spr.fillRect(cx - 4, cy - 5, 8, 3, color);
            spr.fillCircle(cx - 3, cy + 1, 1, color);
            spr.fillCircle(cx,     cy + 1, 1, color);
            spr.fillCircle(cx + 3, cy + 1, 1, color);
            spr.fillCircle(cx - 3, cy + 4, 1, color);
            spr.fillCircle(cx,     cy + 4, 1, color);
            spr.fillCircle(cx + 3, cy + 4, 1, color);
            break;
        case APP_GYRO:
            spr.drawCircle(cx, cy, 7, color);
            spr.fillTriangle(cx, cy - 5, cx - 2, cy, cx + 2, cy, rgb565(248, 113, 113));
            spr.fillTriangle(cx, cy + 5, cx - 2, cy, cx + 2, cy, color);
            break;
        case APP_CONVERTER:
            spr.drawFastHLine(cx - 6, cy - 3, 12, color);
            spr.drawLine(cx + 3, cy - 6, cx + 6, cy - 3, color);
            spr.drawLine(cx + 3, cy,     cx + 6, cy - 3, color);
            spr.drawFastHLine(cx - 6, cy + 3, 12, color);
            spr.drawLine(cx - 3, cy,     cx - 6, cy + 3, color);
            spr.drawLine(cx - 3, cy + 6, cx - 6, cy + 3, color);
            break;
        case APP_WEATHER:
            drawWeatherIcon(spr, cx, cy, 7, 2, true);
            break;
        case APP_FOLDERS:
            spr.fillRect(cx - 7, cy - 6, 6, 3, color);
            spr.drawRoundRect(cx - 7, cy - 4, 14, 10, 2, color);
            break;
        case APP_USB_MSC:
            spr.drawRoundRect(cx - 5, cy - 4, 10, 10, 2, color);
            spr.drawRect(cx - 3, cy - 8, 6, 4, color);
            spr.fillCircle(cx, cy + 1, 2, color);
            break;
        case APP_STORE:
            spr.drawRoundRect(cx - 6, cy - 4, 12, 10, 2, color);
            spr.drawArc(cx, cy - 4, 4, 4, 180, 360, color);
            spr.drawFastVLine(cx, cy - 1, 5, color);
            spr.drawLine(cx - 2, cy + 2, cx, cy + 4, color);
            spr.drawLine(cx + 2, cy + 2, cx, cy + 4, color);
            break;
        case APP_WIFI:
            spr.fillCircle(cx, cy + 4, 2, color);
            spr.drawArc(cx, cy + 4, 5, 5, 220, 320, color);
            spr.drawArc(cx, cy + 4, 9, 9, 220, 320, color);
            break;
        case APP_SETTINGS:
            spr.drawCircle(cx, cy, 5, color);
            spr.drawCircle(cx, cy, 2, color);
            for (int i = 0; i < 4; ++i) {
                float a = i * (PI / 2.0f);
                spr.drawLine(cx + (int)(cosf(a) * 5), cy + (int)(sinf(a) * 5),
                             cx + (int)(cosf(a) * 7), cy + (int)(sinf(a) * 7), color);
            }
            break;
        case APP_SLEEP:
            spr.fillArc(cx, cy, 7, 4, 45, 255, color);
            spr.fillCircle(cx + 4, cy - 4, 1, color);
            break;
        case APP_MINER:
            // Mining pickaxe / hash diamond
            spr.drawLine(cx - 5, cy + 5, cx + 4, cy - 4, color);
            spr.drawLine(cx - 4, cy + 5, cx + 5, cy - 4, color);
            spr.drawLine(cx - 1, cy - 6, cx + 6, cy + 1, rgb565(250, 204, 21));
            spr.drawLine(cx - 1, cy - 5, cx + 5, cy + 1, rgb565(250, 204, 21));
            break;
        case APP_SYSINFO:
            // CPU / Monitor bars
            spr.drawRect(cx - 5, cy - 5, 10, 10, color);
            spr.fillRect(cx - 3, cy - 1, 2, 4, color);
            spr.fillRect(cx,     cy - 3, 2, 6, color);
            spr.fillRect(cx + 3, cy + 1, 2, 2, color);
            break;
        case APP_SNAKE:
            // Coiled snake + food dot
            spr.fillRoundRect(cx - 6, cy - 4, 8, 3, 1, color);
            spr.fillRoundRect(cx - 6, cy - 1, 3, 6, 1, color);
            spr.fillRoundRect(cx - 6, cy + 2, 10, 3, 1, color);
            spr.fillCircle(cx + 5, cy - 4, 2, rgb565(248, 113, 113));
            break;
        case APP_FLAPPY:
            // Bird silhouette
            spr.fillCircle(cx - 1, cy, 5, color);
            spr.fillCircle(cx + 1, cy - 2, 1, rgb565(12, 16, 28));
            spr.fillTriangle(cx + 3, cy - 1, cx + 7, cy + 1, cx + 3, cy + 2, rgb565(251, 146, 60));
            spr.drawFastHLine(cx - 6, cy, 4, rgb565(250, 204, 21));
            break;
        case APP_MUSIC: {
            // Musical eighth-note + sound wave arc
            spr.fillCircle(cx - 4, cy + 4, 3, color);
            spr.fillCircle(cx + 3, cy + 3, 3, color);
            spr.drawFastVLine(cx - 2, cy - 5, 9, color);
            spr.drawFastVLine(cx + 5, cy - 6, 9, color);
            spr.drawLine(cx - 2, cy - 5, cx + 5, cy - 6, color);
            spr.drawLine(cx - 2, cy - 3, cx + 5, cy - 4, color);
            break;
        }
        case APP_FEEDBACK: {
            // Lightbulb / Feature Ideas emblem
            spr.drawCircle(cx, cy - 2, 5, color);
            spr.fillCircle(cx, cy - 2, 2, rgb565(250, 204, 21));
            spr.drawFastHLine(cx - 2, cy + 4, 5, color);
            spr.drawFastHLine(cx - 1, cy + 6, 3, color);
            break;
        }
        case APP_LUA_RUNNER:
            // Downloaded app tile with script / spark emblem
            spr.drawRoundRect(cx - 6, cy - 6, 12, 12, 3, color);
            spr.drawLine(cx - 3, cy - 2, cx - 1, cy, color);
            spr.drawLine(cx - 1, cy,     cx - 3, cy + 2, color);
            spr.drawFastHLine(cx + 1, cy + 2, 3, rgb565(56, 189, 248));
            break;
        default:
            spr.drawCircle(cx, cy, 6, color);
            break;
    }
}

void drawScrollbar(M5Canvas& spr, int x, int y, int h, int totalItems, int visibleItems, int scrollIdx) {
    if (totalItems <= visibleItems || totalItems <= 0) return;
    const auto& th = activeTheme();
    spr.fillRect(x, y, 3, h, th.surface2);
    int barH = max(8, (h * visibleItems) / totalItems);
    int maxScroll = totalItems - visibleItems;
    int barY = y + ((h - barH) * constrain(scrollIdx, 0, maxScroll)) / max(1, maxScroll);
    spr.fillRect(x, barY, 3, barH, activeAccent());
}

void drawFlashProgress(M5Canvas& spr, const char* appName, uint32_t written, uint32_t total, const char* stage) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();
    spr.fillScreen(th.bg);
    drawStatusBar(spr, "App Installer");

    spr.drawRoundRect(14, 28, SCREEN_W - 28, 88, 6, th.border);
    spr.setTextSize(1);
    spr.setTextColor(acc);
    spr.drawString("INSTALLING TO APP1", 24, 36);

    spr.setTextColor(th.text);
    spr.drawString(appName, 24, 52);

    spr.setTextColor(th.textDim);
    spr.drawString(stage, 24, 68);

    int barX = 24, barY = 84, barW = SCREEN_W - 48, barH = 12;
    spr.drawRoundRect(barX, barY, barW, barH, 3, th.border);
    int pct = total > 0 ? (int)((written * 100ULL) / total) : 0;
    pct = constrain(pct, 0, 100);
    int fw = ((barW - 4) * pct) / 100;
    if (fw > 0) spr.fillRoundRect(barX + 2, barY + 2, fw, barH - 4, 2, acc);

    char pbuf[24];
    snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
    spr.setTextColor(th.text);
    spr.drawString(pbuf, 24, 101);
    spr.pushSprite(0, 0);
}

void drawBootLoadingFrame(M5Canvas& spr, uint32_t frame) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();
    spr.fillScreen(th.bg);

    // Subtle pulsing Cardputer emblem (zero progress bar)
    int cx = SCREEN_W / 2;
    int cy = 40;
    spr.fillRoundRect(cx - 19, cy - 12, 38, 24, 5, th.surface);
    bool pulse = ((frame / 6) % 2) == 0;
    spr.drawRoundRect(cx - 19, cy - 12, 38, 24, 5, pulse ? acc : th.border);
    spr.fillRoundRect(cx - 14, cy - 8, 28, 10, 2, acc);
    for (int col = -1; col <= 1; ++col) {
        for (int row = 0; row < 2; ++row) {
            spr.fillCircle(cx + col * 6, cy + 5 + row * 4, 1, th.textDim);
        }
    }

    spr.setTextDatum(MC_DATUM);
    spr.setTextSize(2);
    spr.setTextColor(th.text);
    spr.drawString("CardOS", cx, 70);

    spr.setTextSize(1);
    spr.setTextColor(th.textDim);
    spr.drawString("Loading", cx, 92);

    // 3 wave-bouncing indicator dots
    for (int i = 0; i < 3; ++i) {
        float wave = sinf(frame * 0.28f - i * 0.9f);
        int dx = cx + (i - 1) * 12;
        int dy = 110 - (int)(wave * 3.5f);
        uint16_t col = (wave > 0.35f) ? acc : th.textDim;
        spr.fillCircle(dx, dy, (wave > 0.55f) ? 3 : 2, col);
    }
    spr.setTextDatum(TL_DATUM);
}

void drawChargingAnimFrame(M5Canvas& spr, uint32_t frame, int battPct) {
    const auto& th = activeTheme();
    uint16_t okCol = th.ok;
    uint16_t boltCol = rgb565(250, 204, 21);

    spr.fillScreen(th.bg);
    int cx = SCREEN_W / 2;
    int bw = 96, bh = 44;
    int bx = cx - bw / 2 - 2;
    int by = 22;

    // Outer glow & battery outline
    spr.drawRoundRect(bx - 2, by - 2, bw + 4, bh + 4, 8, (frame % 10 < 5) ? okCol : th.border);
    spr.fillRoundRect(bx, by, bw, bh, 6, th.surface);
    spr.drawRoundRect(bx, by, bw, bh, 6, th.text);
    spr.fillRoundRect(bx + bw, by + 13, 5, 18, 2, th.text);

    // Rising charge fill animation
    int innerW = bw - 8;
    int innerH = bh - 8;
    int baseFill = (innerW * constrain(battPct, 5, 100)) / 100;
    int pulse = (int)((frame * 3) % max(10, innerW - baseFill + 14));
    int curFill = constrain(max(baseFill / 2, min(innerW, baseFill / 2 + pulse)), 4, innerW);
    spr.fillRoundRect(bx + 4, by + 4, curFill, innerH, 4, okCol);

    // Bold lightning bolt in center
    int bcx = bx + bw / 2;
    int bcy = by + bh / 2;
    spr.fillTriangle(bcx + 2, bcy - 12, bcx - 7, bcy + 1, bcx + 1, bcy + 1, boltCol);
    spr.fillTriangle(bcx - 1, bcy - 1, bcx + 7, bcy - 1, bcx - 2, bcy + 12, boltCol);

    spr.setTextDatum(MC_DATUM);
    spr.setTextSize(2);
    spr.setTextColor(th.text);
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d%%", constrain(battPct, 0, 100));
    spr.drawString(pbuf, cx, 84);

    spr.setTextSize(1);
    spr.setTextColor(okCol);
    spr.drawString("Charging...", cx, 108);
    spr.setTextDatum(TL_DATUM);
}

void drawScreensaverFrame(M5Canvas& spr) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();
    spr.fillScreen(TFT_BLACK);

    struct tm tmInfo;
    char tBuf[16] = "--:--";
    char apBuf[4] = "";
    char dBuf[24] = "No Time Sync";
    int minSeed = 0;
    if (getSafeLocalTime(tmInfo)) {
        minSeed = tmInfo.tm_hour * 60 + tmInfo.tm_min;
        int h = tmInfo.tm_hour;
        if (!g_settings.use24h) {
            strlcpy(apBuf, (h >= 12) ? "PM" : "AM", sizeof(apBuf));
            h = h % 12;
            if (h == 0) h = 12;
        }
        snprintf(tBuf, sizeof(tBuf), "%d:%02d", h, tmInfo.tm_min);
        static const char* WDAYS[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* MONS[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        snprintf(dBuf, sizeof(dBuf), "%s, %s %d",
                 WDAYS[tmInfo.tm_wday % 7], MONS[tmInfo.tm_mon % 12], tmInfo.tm_mday);
    }

    // Subtle minute-based pixel shift to prevent burn-in
    int offX = ((minSeed % 5) - 2) * 3;
    int offY = (((minSeed / 5) % 3) - 1) * 3;
    int cx = SCREEN_W / 2 + offX;
    int cy = 46 + offY;

    spr.setTextDatum(MC_DATUM);
    spr.setTextSize(3);
    spr.setTextColor(TFT_WHITE);
    spr.drawString(tBuf, cx - (apBuf[0] ? 10 : 0), cy);

    if (apBuf[0] != '\0') {
        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString(apBuf, cx + (int)strlen(tBuf) * 9 + 2, cy + 5);
    }

    spr.setTextSize(1);
    spr.setTextColor(rgb565(148, 163, 184));
    spr.drawString(dBuf, cx, cy + 26);

    // Optional configurable Screensaver Widget
    if (g_settings.screensaverWidget != WIDGET_NONE) {
        String wTxt;
        uint16_t wCol = rgb565(125, 211, 252);
        switch (g_settings.screensaverWidget) {
            case WIDGET_WEATHER:
                if (g_sys.weather.valid) {
                    wTxt = String(g_sys.weather.city) + " " + formatTemp(g_sys.weather.tempC, true);
                    wCol = rgb565(56, 189, 248);
                } else {
                    wTxt = "Weather: --";
                    wCol = rgb565(100, 116, 139);
                }
                break;
            case WIDGET_MUSIC:
                if (g_sys.music.trackName[0] != '\0') {
                    wTxt = String(g_sys.music.playing ? (g_sys.music.paused ? "|| " : "> ") : "[] ")
                         + g_sys.music.trackName;
                    if (g_sys.music.shuffle) wTxt += " [S]";
                    wCol = g_sys.music.playing ? rgb565(52, 211, 153) : rgb565(148, 163, 184);
                } else if (g_sys.music.streaming) {
                    wTxt = String("Phone Stream ") + g_sys.wifiIp;
                    wCol = rgb565(167, 139, 250);
                } else {
                    wTxt = "Music: Idle";
                    wCol = rgb565(148, 163, 184);
                }
                break;
            case WIDGET_MINER: {
                char wb[40];
                if (g_sys.minerPool.valid) {
                    snprintf(wb, sizeof(wb), "Miner %d/%d %.1f",
                             g_sys.minerPool.onlineWorkers, g_sys.minerPool.totalWorkers,
                             g_sys.minerPool.balTotal);
                } else {
                    snprintf(wb, sizeof(wb), "Miner: --");
                }
                wTxt = wb;
                wCol = rgb565(250, 204, 21);
                break;
            }
            case WIDGET_SYSINFO: {
                char wb[36];
                snprintf(wb, sizeof(wb), "Heap:%uKB Bat:%d%%",
                         (unsigned)(ESP.getFreeHeap() / 1024),
                         g_sys.batteryPct);
                wTxt = wb;
                wCol = rgb565(74, 222, 128);
                break;
            }
            case WIDGET_CALENDAR: {
                char wb[36];
                snprintf(wb, sizeof(wb), "Today: %d ev  %d notes",
                         g_sys.todayEventsCount, g_sys.notesCount);
                wTxt = wb;
                wCol = rgb565(244, 114, 182);
                break;
            }
            case WIDGET_NOTES: {
                char wb[28];
                snprintf(wb, sizeof(wb), "Notes: %d saved", g_sys.notesCount);
                wTxt = wb;
                wCol = rgb565(251, 191, 36);
                break;
            }
            case WIDGET_SNAKE: {
                char wb[28];
                snprintf(wb, sizeof(wb), "Snake Best: %u", (unsigned)g_settings.snakeHighScore);
                wTxt = wb;
                wCol = rgb565(74, 222, 128);
                break;
            }
            case WIDGET_FLAPPY: {
                char wb[28];
                snprintf(wb, sizeof(wb), "Flappy Best: %u", (unsigned)g_settings.flappyHighScore);
                wTxt = wb;
                wCol = rgb565(250, 204, 21);
                break;
            }
            default:
                break;
        }
        if (wTxt.length() > 0) {
            if (wTxt.length() > 34) wTxt = wTxt.substring(0, 31) + "...";
            spr.setTextColor(wCol);
            spr.drawString(wTxt, cx, SCREEN_H - 36);
        }
    }

    // Bottom info strip: Battery % + Charging bolt | Wi-Fi status
    int by = SCREEN_H - 22;
    spr.setTextDatum(TL_DATUM);
    uint16_t batCol = (g_sys.batteryPct <= 15) ? th.err : (g_sys.batteryPct <= 35 ? th.warn : th.ok);
    char batTxt[24];
    snprintf(batTxt, sizeof(batTxt), "%sBat: %d%%%s",
             g_settings.lowPowerMode ? "ECO  " : "",
             g_sys.batteryPct,
             g_sys.isCharging ? " +" : "");
    spr.setTextColor(g_sys.isCharging ? rgb565(250, 204, 21) : batCol);
    spr.drawString(batTxt, 12, by);

    spr.setTextDatum(TR_DATUM);
    if (g_sys.wifiConnected) {
        String ws = String("WiFi: ") + g_sys.wifiStatus;
        if ((int)ws.length() > 19) ws = ws.substring(0, 18) + "~";
        spr.setTextColor(th.ok);
        spr.drawString(ws, SCREEN_W - 12, by);
    } else {
        spr.setTextColor(rgb565(120, 130, 145));
        spr.drawString("WiFi: Off", SCREEN_W - 12, by);
    }
    spr.setTextDatum(TL_DATUM);

    // Keep live notifications visible on screensaver
    drawToastOverlay(spr);
}

} // namespace ui

