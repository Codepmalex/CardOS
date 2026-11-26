#include "hardware.h"
#include "state.h"
#include "net_weather.h"
#include "ui.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>
#include "tusb.h"

extern void switchApp(AppId id);
extern M5Canvas* getScreenCanvas();

static bool s_btnWasDown = false;
static uint32_t s_btnDownMs = 0;
static bool s_btnLongFired = false;
static uint32_t s_lastBattCheckMs = 0;
static uint32_t s_lastLedUpdateMs = 0;
static uint32_t s_lastTimeCacheMs = 0;
static bool s_ledValid = false;
static uint8_t s_lastR = 0, s_lastG = 0, s_lastB = 0;

static void setLedRgb(uint8_t r, uint8_t g, uint8_t b, bool force = false) {
    if (!force && s_ledValid && r == s_lastR && g == s_lastG && b == s_lastB) return;
    s_ledValid = true;
    s_lastR = r;
    s_lastG = g;
    s_lastB = b;
    neopixelWrite(LED_PIN, r, g, b);
}

void initHardware() {
    setLedRgb(0, 0, 0, true);

    M5Cardputer.Speaker.setVolume(g_settings.volume);
    applyPowerProfile();

    g_sys.batteryPct = constrain((int)M5Cardputer.Power.getBatteryLevel(), 0, 100);
    updateChargingDetector(false);
}

void applyBrightness(uint8_t val) {
    if (!g_sys.screenAsleep && !g_sys.screensaverActive && !g_sys.flashlightOn) {
        uint8_t eff = g_settings.lowPowerMode ? (uint8_t)min((int)val, 80) : val;
        M5Cardputer.Display.setBrightness(eff);
    }
}

void applyPowerProfile() {
    if (g_sys.deepSleepMode || g_sys.screensaverActive) {
        setCpuFrequencyMhz(80);
    } else if (g_settings.lowPowerMode) {
        setCpuFrequencyMhz(80);
    } else {
        setCpuFrequencyMhz(240);
    }
    applyBrightness(g_settings.brightness);
    updateLedEngine();
}

void enterScreensaver() {
    if (g_sys.deepSleepMode || g_sys.screensaverActive) return;
    g_sys.screensaverActive = true;
    g_sys.screenAsleep = false;
    g_sys.screenDimmed = false;
    g_sys.flashlightOn = false;

    uint8_t startBr = g_settings.lowPowerMode ? (uint8_t)min((int)g_settings.brightness, 80) : g_settings.brightness;
    uint8_t targetBr = constrain(g_settings.screensaverBright, 10, 200);
    for (int step = 3; step >= 0; --step) {
        int br = targetBr + ((startBr - targetBr) * step) / 4;
        M5Cardputer.Display.setBrightness((uint8_t)constrain(br, 10, 255));
        delay(12);
    }
    setCpuFrequencyMhz(80);
    setLedRgb(0, 0, 0, true);
}

void exitScreensaver() {
    g_sys.lastActivityMs = millis();
    if (!g_sys.screensaverActive) return;
    g_sys.screensaverActive = false;
    g_sys.screenDimmed = false;
    s_ledValid = false;

    setCpuFrequencyMhz(g_settings.lowPowerMode ? 80 : 240);
    uint8_t targetBr = g_settings.lowPowerMode ? (uint8_t)min((int)g_settings.brightness, 80) : g_settings.brightness;
    uint8_t startBr = constrain(g_settings.screensaverBright, 10, 200);
    for (int step = 1; step <= 4; ++step) {
        int br = startBr + (((int)targetBr - (int)startBr) * step) / 4;
        M5Cardputer.Display.setBrightness((uint8_t)constrain(br, 10, 255));
        delay(10);
    }
    updateLedEngine();
}

void wakeScreen() {
    g_sys.lastActivityMs = millis();
    if (g_sys.screensaverActive) {
        exitScreensaver();
        return;
    }
    if (g_sys.screenAsleep || g_sys.screenDimmed) {
        bool wasAsleep = g_sys.screenAsleep;
        g_sys.screenAsleep = false;
        g_sys.screenDimmed = false;
        s_ledValid = false;
        uint8_t targetBr = g_settings.lowPowerMode ? (uint8_t)min((int)g_settings.brightness, 80) : g_settings.brightness;
        if (wasAsleep) {
            setCpuFrequencyMhz(g_settings.lowPowerMode ? 80 : 240);
            M5Cardputer.Display.wakeup();
            for (int step = 1; step <= 5; ++step) {
                M5Cardputer.Display.setBrightness((targetBr * step) / 5);
                delay(12);
            }
        } else {
            M5Cardputer.Display.setBrightness(targetBr);
        }
    }
}

void sleepScreen() {
    g_sys.screenAsleep = true;
    g_sys.screensaverActive = false;
    g_sys.screenDimmed = false;
    for (int step = 4; step >= 0; --step) {
        M5Cardputer.Display.setBrightness((g_settings.brightness * step) / 5);
        delay(12);
    }
    M5Cardputer.Display.sleep();
    setCpuFrequencyMhz(80);
    setLedRgb(0, 0, 0, true);
}

void enterChargeSleepMode() {
    saveWeatherAndTimeCache();
    g_sys.deepSleepMode = true;
    g_sys.screenAsleep = true;
    g_sys.screenDimmed = false;
    g_sys.flashlightOn = false;
    g_sys.toastUntilMs = 0;
    g_sys.ledNotifyUntilMs = 0;

    for (int step = 4; step >= 0; --step) {
        M5Cardputer.Display.setBrightness((g_settings.brightness * step) / 5);
        delay(12);
    }
    M5Cardputer.Display.sleep();
    setLedRgb(0, 0, 0, true);

    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    g_sys.wifiConnected = false;
    g_sys.wifiConnecting = false;
    strlcpy(g_sys.wifiStatus, "Off", sizeof(g_sys.wifiStatus));

    setCpuFrequencyMhz(80);

    // Wait for triggering key / G0 button release before arming wake detection
    uint32_t waitStart = millis();
    while (millis() - waitStart < 1000) {
        M5Cardputer.update();
        if (!M5Cardputer.BtnA.isPressed() && !M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        delay(20);
    }
    delay(180);
    M5Cardputer.update();
    g_sys.sleepEnteredMs = millis();
}

void exitChargeSleepMode() {
    setCpuFrequencyMhz(240);
    g_sys.deepSleepMode = false;
    g_sys.lastActivityMs = millis();

    wakeScreen();

    WiFi.mode(WIFI_STA);
    if (g_settings.wifiAutoConnect && strlen(g_settings.wifiSsid) > 0) {
        requestWifiConnect();
    }

    s_ledValid = false;
    updateLedEngine();
}

void setFlashlight(bool on) {
    g_sys.flashlightOn = on;
    if (on) {
        wakeScreen();
        M5Cardputer.Display.setBrightness(255);
    } else {
        M5Cardputer.Display.setBrightness(g_settings.brightness);
    }
}

void playKeyClick() {
    if (g_sys.deepSleepMode) return;
    if (g_settings.deviceProfile != PROF_SOUND) return;
    if (g_settings.keyClick && g_settings.volume > 0) {
        M5Cardputer.Speaker.setVolume(g_settings.volume);
        M5Cardputer.Speaker.tone(2400, 12);
    }
}

static void playTonePattern(uint8_t toneIdx, uint8_t vol) {
    if (vol == 0) return;
    M5Cardputer.Speaker.setVolume(vol);
    switch (toneIdx % 4) {
        case 0: // Chime
            M5Cardputer.Speaker.tone(1046, 70);
            break;
        case 1: // Double
            M5Cardputer.Speaker.tone(1318, 55);
            delay(75);
            M5Cardputer.Speaker.tone(1568, 70);
            break;
        case 2: // Alert
            M5Cardputer.Speaker.tone(1800, 90);
            delay(110);
            M5Cardputer.Speaker.tone(1800, 90);
            break;
        case 3: // Soft
        default:
            M5Cardputer.Speaker.tone(784, 60);
            break;
    }
}

void playNotifyChime() {
    if (g_sys.deepSleepMode) return;
    if (g_settings.deviceProfile != PROF_SOUND) return;
    if (g_settings.notifEnabled && g_settings.volume > 0) {
        playTonePattern(0, g_settings.volume);
    }
}

void triggerNotification(NotifApp app, const char* msg) {
    if (g_sys.deepSleepMode) return;
    if (!g_settings.notifEnabled || app >= NOTIF_APP_COUNT) return;
    const auto& rule = g_settings.notifRules[app];
    if (!rule.enabled) return;

    showToast(msg, 2800, false);

    // Anonymous mode suppresses both LEDs and sounds
    if (g_settings.deviceProfile == PROF_ANON) return;

    if (rule.ledEnabled) {
        g_sys.ledNotifyColor = LED_PRESETS[rule.ledColorIdx % 7];
        g_sys.ledNotifyPattern = rule.ledPattern % 4;
        g_sys.ledNotifyUntilMs = millis() + 6500;
    }

    // Silent mode keeps LEDs active but mutes all sound
    if (g_settings.deviceProfile == PROF_SOUND && rule.volume > 0) {
        playTonePattern(rule.toneIdx, rule.volume);
    }
}

void executeBtnAction(BtnAction act) {
    switch (act) {
        case BTN_ACT_HOME:
            if (g_sys.flashlightOn) setFlashlight(false);
            switchApp(APP_HOME);
            break;
        case BTN_ACT_SLEEP:
            if (g_sys.screenAsleep) wakeScreen();
            else sleepScreen();
            break;
        case BTN_ACT_FLASHLIGHT:
            setFlashlight(!g_sys.flashlightOn);
            break;
        case BTN_ACT_BRIGHTNESS: {
            const uint8_t levels[4] = {60, 120, 180, 255};
            int nextIdx = 0;
            for (int i = 0; i < 4; ++i) {
                if (g_settings.brightness < levels[i] - 10) { nextIdx = i; break; }
            }
            g_settings.brightness = levels[nextIdx];
            saveSettings();
            applyBrightness(g_settings.brightness);
            char msg[32];
            snprintf(msg, sizeof(msg), "Brightness: %d%%", (g_settings.brightness * 100) / 255);
            showToast(msg, 1200);
            break;
        }
        case BTN_ACT_SEARCH:
            g_sys.focusSearchOnHome = true;
            switchApp(APP_HOME);
            break;
        case BTN_ACT_NOTES:
            switchApp(APP_NOTES);
            break;
        case BTN_ACT_CALC:
            switchApp(APP_CALC);
            break;
        case BTN_ACT_MUTE:
            g_settings.keyClick = !g_settings.keyClick;
            saveSettings();
            showToast(g_settings.keyClick ? "Audio Click: ON" : "Audio Click: OFF", 1400);
            break;
        case BTN_ACT_LED_TOGGLE:
            g_settings.ledEnabled = !g_settings.ledEnabled;
            saveSettings();
            updateLedEngine();
            showToast(g_settings.ledEnabled ? "RGB LED: ON" : "RGB LED: OFF", 1400);
            break;
        case BTN_ACT_PROFILE:
            g_settings.deviceProfile = (g_settings.deviceProfile + 1) % PROF_COUNT;
            saveSettings();
            updateLedEngine();
            showToast((String("Mode: ") + PROFILE_NAMES[g_settings.deviceProfile]).c_str(), 1400);
            break;
        case BTN_ACT_SCREENSAVER:
            if (g_sys.screensaverActive) exitScreensaver();
            else enterScreensaver();
            break;
        default:
            break;
    }
}

static void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint8_t region = h / 43;
    uint8_t rem = (h - (region * 43)) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

void updateLedEngine() {
    if (g_sys.deepSleepMode) {
        setLedRgb(0, 0, 0);
        return;
    }
    uint32_t now = millis();
    if (now - s_lastLedUpdateMs < 50) return;
    s_lastLedUpdateMs = now;

    // Anonymous mode forces ALL LEDs OFF
    if (g_settings.deviceProfile == PROF_ANON) {
        setLedRgb(0, 0, 0);
        return;
    }

    // Notification LED alert (active in Sound and Silent modes)
    if (now < g_sys.ledNotifyUntilMs && g_settings.notifEnabled) {
        uint8_t br = g_settings.ledBrightness;
        uint8_t nr = (g_sys.ledNotifyColor >> 16) & 0xFF;
        uint8_t ng = (g_sys.ledNotifyColor >> 8) & 0xFF;
        uint8_t nb = g_sys.ledNotifyColor & 0xFF;
        float mult = 1.0f;
        if (g_sys.ledNotifyPattern == 0) {
            // Smooth sine-wave pulse
            mult = 0.08f + 0.92f * (sinf((now % 1400) * 2.0f * PI / 1400.0f) + 1.0f) * 0.5f;
        } else if (g_sys.ledNotifyPattern == 1) {
            // Standard Blink
            mult = (((now / 350) % 2) == 0) ? 1.0f : 0.0f;
        } else if (g_sys.ledNotifyPattern == 2) {
            // Fast Flash
            mult = (((now / 110) % 2) == 0) ? 1.0f : 0.0f;
        } else {
            // Solid
            mult = 1.0f;
        }
        uint8_t eff = (uint8_t)(br * mult);
        setLedRgb((nr * eff) / 255, (ng * eff) / 255, (nb * eff) / 255);
        return;
    }

    if (!g_settings.ledEnabled || g_settings.ledMode == LED_OFF || g_sys.screenAsleep || g_sys.screensaverActive || g_settings.lowPowerMode) {
        setLedRgb(0, 0, 0);
        return;
    }

    uint8_t br = g_settings.ledBrightness;
    uint8_t baseR = (g_settings.ledColor >> 16) & 0xFF;
    uint8_t baseG = (g_settings.ledColor >> 8) & 0xFF;
    uint8_t baseB = g_settings.ledColor & 0xFF;

    uint8_t r = 0, g = 0, b = 0;
    switch (g_settings.ledMode) {
        case LED_SOLID:
            r = (baseR * br) / 255;
            g = (baseG * br) / 255;
            b = (baseB * br) / 255;
            break;
        case LED_BREATHE: {
            float phase = (sinf((now % 3000) * 2.0f * PI / 3000.0f) + 1.0f) * 0.5f;
            uint8_t scale = (uint8_t)(br * (0.1f + 0.9f * phase));
            r = (baseR * scale) / 255;
            g = (baseG * scale) / 255;
            b = (baseB * scale) / 255;
            break;
        }
        case LED_RAINBOW: {
            uint8_t hue = (uint8_t)((now / 20) & 0xFF);
            hsvToRgb(hue, 255, br, r, g, b);
            break;
        }
        case LED_BATTERY: {
            if (g_sys.batteryPct >= 60) { r = 0; g = br; b = br / 4; }
            else if (g_sys.batteryPct >= 25) { r = br; g = br / 2; b = 0; }
            else { r = br; g = 0; b = 0; }
            break;
        }
        case LED_NOTIFY:
        default:
            r = 0; g = 0; b = 0;
            break;
    }
    setLedRgb(r, g, b);
}

void tickHardware(bool hadInputEvent) {
    if (g_sys.deepSleepMode) return;
    if (hadInputEvent) {
        wakeScreen();
    }

    uint32_t now = millis();

    // Side/Top button G0 handling (short vs long press)
    bool btnDown = M5Cardputer.BtnA.isPressed();
    if (btnDown && !s_btnWasDown) {
        s_btnWasDown = true;
        s_btnDownMs = now;
        s_btnLongFired = false;
    } else if (btnDown && s_btnWasDown && !s_btnLongFired && (now - s_btnDownMs > 650)) {
        s_btnLongFired = true;
        wakeScreen();
        executeBtnAction((BtnAction)g_settings.btnLongAction);
    } else if (!btnDown && s_btnWasDown) {
        if (!s_btnLongFired && (now - s_btnDownMs > 25)) {
            if (g_sys.screensaverActive) {
                exitScreensaver();
            } else if (g_sys.screenAsleep) {
                wakeScreen();
            } else {
                executeBtnAction((BtnAction)g_settings.btnShortAction);
            }
        }
        s_btnWasDown = false;
    }

    // Screen timeout / dim management (triggers Screensaver if enabled, else shuts down screen)
    now = millis();
    uint16_t effTimeout = g_settings.screenTimeoutSec;
    if (g_settings.lowPowerMode && (effTimeout == 0 || effTimeout > 30)) {
        effTimeout = 30;
    }
    if (!g_sys.flashlightOn && !g_sys.screenAsleep && !g_sys.screensaverActive && effTimeout > 0 && now >= g_sys.lastActivityMs) {
        uint32_t idleMs = now - g_sys.lastActivityMs;
        uint32_t timeoutMs = (uint32_t)effTimeout * 1000UL;
        if (idleMs >= timeoutMs) {
            if (g_settings.screensaverEnabled) {
                enterScreensaver();
            } else {
                sleepScreen();
            }
        } else if (g_settings.dimBeforeSleep && !g_sys.screenDimmed && idleMs >= (timeoutMs * 3) / 4) {
            g_sys.screenDimmed = true;
            M5Cardputer.Display.setBrightness(max(20, g_settings.brightness / 4));
        }
    }

    // Battery & charging monitor (every 500ms) with heavy EMA + hysteresis smoother
    static float s_smoothBatt = -1.0f;
    if (now - s_lastBattCheckMs > 500) {
        s_lastBattCheckMs = now;
        int rawPct = constrain((int)M5Cardputer.Power.getBatteryLevel(), 0, 100);
        if (s_smoothBatt < 0.0f) {
            s_smoothBatt = (float)rawPct;
            g_sys.batteryPct = rawPct;
        } else {
            s_smoothBatt = s_smoothBatt * 0.90f + (float)rawPct * 0.10f;
            if (rawPct == 0 || rawPct == 100 || fabsf(s_smoothBatt - (float)g_sys.batteryPct) >= 1.6f) {
                g_sys.batteryPct = constrain((int)roundf(s_smoothBatt), 0, 100);
            }
        }
        updateChargingDetector(true);
        if (g_sys.batteryPct <= 15 && !g_sys.isCharging) {
            if (!g_sys.lowBattNotified) {
                g_sys.lowBattNotified = true;
                triggerNotification(NOTIF_BATTERY, "Low Battery (<15%)");
            }
        } else if (g_sys.batteryPct > 20) {
            g_sys.lowBattNotified = false;
        }
    }

    // Periodically cache time + weather to SD every 60s so reboots preserve time
    if (now - s_lastTimeCacheMs > 60000) {
        s_lastTimeCacheMs = now;
        if (time(nullptr) > 1700000000) {
            saveWeatherAndTimeCache();
        }
    }

    updateLedEngine();
}

void playChargingAnimation(bool fromSleep) {
    M5Canvas* sprPtr = getScreenCanvas();
    if (!sprPtr) return;
    M5Canvas& spr = *sprPtr;

    if (fromSleep) {
        M5Cardputer.Display.wakeup();
        for (int step = 1; step <= 4; ++step) {
            M5Cardputer.Display.setBrightness((g_settings.brightness * step) / 4);
            delay(10);
        }
    } else {
        M5Cardputer.Display.setBrightness(g_settings.brightness);
    }

    uint32_t startMs = millis();
    uint32_t frame = 0;
    while (millis() - startMs < 2000) {
        ui::drawChargingAnimFrame(spr, frame++, g_sys.batteryPct);
        spr.pushSprite(0, 0);
        if (!fromSleep) {
            M5Cardputer.update();
            if (M5Cardputer.BtnA.wasPressed() || (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) {
                break;
            }
        }
        delay(33);
    }

    if (fromSleep) {
        for (int step = 3; step >= 0; --step) {
            M5Cardputer.Display.setBrightness((g_settings.brightness * step) / 4);
            delay(10);
        }
        M5Cardputer.Display.sleep();
        g_sys.screenAsleep = true;
    } else {
        g_sys.lastActivityMs = millis();
    }
}

void updateChargingDetector(bool allowAnim) {
    static bool s_inited = false;
    static bool s_prevCharging = false;
    static bool s_wallCharging = false;
    static int  s_avgMv = 0;
    static uint32_t s_lastPlugAnimMs = 0;

    int mv = (int)M5Cardputer.Power.getBatteryVoltage();
    if (mv > 2500 && mv < 4600) {
        g_sys.batteryMv = mv;
        if (s_avgMv == 0) s_avgMv = mv;
    }

    bool usbPlugged = tud_connected();
    if (mv > 2500 && mv < 4600) {
        if (!usbPlugged) {
            // Detect wall-only charger insertion/removal via VBAT jump/high-float
            if (mv >= 4195 || (mv - s_avgMv) >= 55) {
                s_wallCharging = true;
            } else if ((s_avgMv - mv) >= 55 || mv < 4110) {
                s_wallCharging = false;
            }
        } else {
            s_wallCharging = false;
        }
        s_avgMv = (s_avgMv * 7 + mv) / 8;
    }

    bool nowCharging = usbPlugged || s_wallCharging;
    g_sys.isCharging = nowCharging;

    if (!s_inited) {
        s_inited = true;
        s_prevCharging = nowCharging;
        return;
    }

    if (nowCharging && !s_prevCharging && allowAnim) {
        if (millis() - s_lastPlugAnimMs > 3000) {
            s_lastPlugAnimMs = millis();
            bool asleepOrSaver = g_sys.deepSleepMode || g_sys.screenAsleep || g_sys.screensaverActive;
            playChargingAnimation(asleepOrSaver && !g_sys.screensaverActive);
            if (g_sys.screensaverActive) {
                M5Cardputer.Display.setBrightness(constrain(g_settings.screensaverBright, 10, 200));
            }
        }
    }
    s_prevCharging = nowCharging;
}
