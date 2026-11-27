#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include "config.h"

namespace ui {

inline float easeOutCubic(float t) {
    t = constrain(t, 0.0f, 1.0f);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

inline float smoothMove(float cur, float target, float k = 0.45f) {
    float diff = target - cur;
    return (fabsf(diff) < 0.6f) ? target : (cur + diff * k);
}

int drawStatusIcons(M5Canvas& spr, bool showTime = true);
void drawStatusBar(M5Canvas& spr, const char* title, bool showTime = true);
void drawToastOverlay(M5Canvas& spr);
void drawWeatherIcon(M5Canvas& spr, int cx, int cy, int r, int wmoCode, bool isDay);
void drawAppIcon(M5Canvas& spr, int cx, int cy, AppId id, uint16_t color, bool isBinApp = false);
void drawScrollbar(M5Canvas& spr, int x, int y, int h, int totalItems, int visibleItems, int scrollIdx);

// Helper to format temperature according to g_settings.tempFahrenheit
String formatTemp(float tempC, bool withUnit = true);

// Draw a centered progress modal directly onto display (used during flash)
void drawFlashProgress(M5Canvas& spr, const char* appName, uint32_t written, uint32_t total, const char* stage);

// Animated CardOS boot loading screen (no progress bar)
void drawBootLoadingFrame(M5Canvas& spr, uint32_t frame);

// Fullscreen 2s charging animation overlay
void drawChargingAnimFrame(M5Canvas& spr, uint32_t frame, int battPct);

// Ultra-low-power screensaver clock/battery/Wi-Fi overlay
void drawScreensaverFrame(M5Canvas& spr);

// Clean word-wrapping text renderer & scrollable full-description modal
int drawWrappedText(M5Canvas& spr, const String& text, int x, int y, int maxW, int maxH,
                    int lineH = 10, int scrollLines = 0, int* totalLinesOut = nullptr);
void drawTextModal(M5Canvas& spr, const char* title, const String& body,
                   int scrollLine, int* totalLinesOut = nullptr);

} // namespace ui

