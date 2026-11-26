#include <M5Cardputer.h>
#include <SD.h>
#include "config.h"
#include "state.h"
#include "input.h"
#include "storage.h"
#include "hardware.h"
#include "net_weather.h"
#include "serial_ctl.h"
#include "app_installer.h"
#include "usb_msc_mode.h"
#include "ir_tx.h"
#include "ui.h"
#include "apps/apps_registry.h"

static M5Canvas s_spr(&M5Cardputer.Display);
static App* s_apps[APP_COUNT] = {nullptr};
static bool s_uiReady = false;
static volatile bool s_bootAnimStop = false;
static volatile bool s_bootAnimDone = false;

static void bootAnimTask(void* param) {
    (void)param;
    uint32_t startMs = millis();
    uint32_t frame = 0;
    while (!s_bootAnimStop || (millis() - startMs < 750)) {
        ui::drawBootLoadingFrame(s_spr, frame++);
        s_spr.pushSprite(0, 0);
        delay(45);
    }
    s_bootAnimDone = true;
    vTaskDelete(nullptr);
}

M5Canvas* getScreenCanvas() { return &s_spr; }

const char* getAppName(AppId id) {
    if (id < APP_COUNT && s_apps[id]) return s_apps[id]->name();
    return "Unknown";
}

void switchApp(AppId id) {
    if (id >= APP_COUNT || !s_apps[id]) return;
    AppId prev = g_sys.activeApp;
    if (prev < APP_COUNT && s_apps[prev]) {
        s_apps[prev]->onExit();
    }
    g_sys.activeApp = id;
    s_apps[id]->onEnter();

    if (s_uiReady && !g_sys.screenAsleep && !g_sys.deepSleepMode && prev != id) {
        s_apps[id]->render(s_spr);
        uint16_t acc = activeAccent();
        if (prev == APP_HOME && id != APP_HOME) {
            // Opening animation: 5-frame smooth center zoom-in
            for (int f = 1; f <= 4; ++f) {
                int w = (SCREEN_W * (f + 1)) / 5;
                int h = (SCREEN_H * (f + 1)) / 5;
                int x = (SCREEN_W - w) / 2;
                int y = (SCREEN_H - h) / 2;
                M5Cardputer.Display.setClipRect(x, y, w, h);
                s_spr.pushSprite(0, 0);
                M5Cardputer.Display.clearClipRect();
                M5Cardputer.Display.drawRect(x, y, w, h, acc);
                delay(14);
            }
        } else if (prev != APP_HOME && id == APP_HOME) {
            // Closing animation: 4-frame clean top-down reveal back to Home
            for (int f = 1; f <= 3; ++f) {
                int h = (SCREEN_H * f) / 4;
                M5Cardputer.Display.setClipRect(0, 0, SCREEN_W, h);
                s_spr.pushSprite(0, 0);
                M5Cardputer.Display.clearClipRect();
                M5Cardputer.Display.drawFastHLine(0, h, SCREEN_W, acc);
                delay(14);
            }
        } else {
            // App-to-app switch: 4-frame horizontal slide-in
            for (int f = 1; f <= 3; ++f) {
                int x = SCREEN_W - (SCREEN_W * f) / 4;
                M5Cardputer.Display.setClipRect(x, 0, SCREEN_W - x, SCREEN_H);
                s_spr.pushSprite(0, 0);
                M5Cardputer.Display.clearClipRect();
                M5Cardputer.Display.drawFastVLine(x, 0, SCREEN_H, acc);
                delay(14);
            }
        }
        M5Cardputer.Display.clearClipRect();
        s_spr.pushSprite(0, 0);
    }
}

bool openAppByName(const String& name) {
    String q = name;
    q.trim();
    q.toLowerCase();
    for (int i = 0; i < APP_COUNT; ++i) {
        if (!s_apps[i]) continue;
        String nm = s_apps[i]->name();
        nm.toLowerCase();
        if (nm == q || nm.startsWith(q)) {
            switchApp((AppId)i);
            return true;
        }
    }
    if (q == "usbdisk" || q == "usb" || q == "msc") { switchApp(APP_USB_MSC); return true; }
    if (q == "unit" || q == "converter") { switchApp(APP_CONVERTER); return true; }
    if (q == "calculator") { switchApp(APP_CALC); return true; }
    if (q == "compass") { switchApp(APP_GYRO); return true; }
    if (q == "files" || q == "explorer") { switchApp(APP_FOLDERS); return true; }
    if (q == "store" || q == "installer" || q == "more" || q == "moreapps") { switchApp(APP_STORE); return true; }
    if (q == "wifi" || q == "network") { switchApp(APP_WIFI); return true; }
    if (q == "sleep" || q == "power") { switchApp(APP_SLEEP); return true; }
    if (q == "miner" || q == "mining" || q == "rig" || q == "rigs") { switchApp(APP_MINER); return true; }
    if (q == "sysinfo" || q == "stats" || q == "system") { switchApp(APP_SYSINFO); return true; }
    if (q == "snake") { switchApp(APP_SNAKE); return true; }
    if (q == "flappy" || q == "bird") { switchApp(APP_FLAPPY); return true; }
    return false;
}

bool installAndBootBinInteractive(const String& path, const String& displayName) {
    if (!g_sys.sdMounted) {
        showToast("No SD card!", 1800);
        return false;
    }
    File f = SD.open(path.c_str(), FILE_READ);
    uint32_t fsize = f ? (uint32_t)f.size() : 0;
    if (f) f.close();
    if (fsize == 0) {
        showToast("File not found", 1800);
        return false;
    }

    String err;
    if (!isBinAlreadyInstalled(path.c_str(), fsize)) {
        bool ok = installBinToApp1(path.c_str(), [&](uint32_t written, uint32_t total, const char* stage) {
            ui::drawFlashProgress(s_spr, displayName.c_str(), written, total, stage);
        }, err);
        if (!ok) {
            showToast(err.c_str(), 2600);
            return false;
        }
    } else {
        ui::drawFlashProgress(s_spr, displayName.c_str(), 100, 100, "Already in app1, booting...");
        delay(250);
    }

    ui::drawFlashProgress(s_spr, displayName.c_str(), 100, 100, "Launching! RST=CardOS");
    delay(350);
    if (!launchInstalledApp1(err)) {
        showToast(err.c_str(), 2600);
        return false;
    }
    return true;
}

void setup() {
    initState();

    // Clear any leftover USB MSC boot flag so boots always land on CardOS
    if (getMscBootFlag()) {
        setMscBootFlag(false);
    }

    // Ensure soft resets stay in CardOS unless launching app1
    ensureBootToCardOS();

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(g_settings.flipScreen ? 2 : 0);
    M5Cardputer.Display.setBrightness(g_settings.brightness);
    s_spr.setColorDepth(16);
    s_spr.createSprite(SCREEN_W, SCREEN_H);

    // Show animated loading screen immediately (no progress bar) while init runs
    s_bootAnimStop = false;
    s_bootAnimDone = false;
    xTaskCreatePinnedToCore(bootAnimTask, "boot_anim", 4096, nullptr, 1, nullptr, 0);

    g_sys.imuReady = M5.Imu.isEnabled();

    initInput();
    initSerialCtl();
    initStorage();
    syncSettingsWithStorage();
    loadHomeLayout();
    loadWeatherAndTimeCache();

    s_bootAnimStop = true;
    while (!s_bootAnimDone) {
        delay(10);
    }

    M5Cardputer.Display.setRotation(g_settings.flipScreen ? 2 : 0);
    initHardware();
    initIrTx();
    applyTimezoneConfig();
    initNetWeather();

    s_apps[APP_HOME]      = createHomeApp();
    s_apps[APP_NOTES]     = createNotesApp();
    s_apps[APP_CALENDAR]  = createCalendarApp();
    s_apps[APP_CALC]      = createCalcApp();
    s_apps[APP_GYRO]      = createGyroApp();
    s_apps[APP_CONVERTER] = createConverterApp();
    s_apps[APP_WEATHER]   = createWeatherApp();
    s_apps[APP_FOLDERS]   = createFoldersApp();
    s_apps[APP_USB_MSC]   = createUsbMscApp();
    s_apps[APP_STORE]     = createStoreApp();
    s_apps[APP_WIFI]      = createWifiApp();
    s_apps[APP_SETTINGS]  = createSettingsApp();
    s_apps[APP_SLEEP]     = createSleepApp();
    s_apps[APP_MINER]     = createMinerApp();
    s_apps[APP_SYSINFO]   = createSysInfoApp();
    s_apps[APP_SNAKE]      = createSnakeApp();
    s_apps[APP_FLAPPY]     = createFlappyApp();
    s_apps[APP_LUA_RUNNER] = createLuaRunnerApp();

    // Pre-initialize Calendar & Notes counts for widgets/badges
    s_apps[APP_CALENDAR]->onEnter();
    s_apps[APP_CALENDAR]->onExit();
    s_apps[APP_NOTES]->onEnter();
    s_apps[APP_NOTES]->onExit();

    s_uiReady = true;
    switchApp(APP_HOME);
    Serial.println("CARDOS_READY");
}

void loop() {
    static bool s_prevWakeKeyDown = false;
    static uint32_t s_lastSleepChargeMs = 0;
    if (g_sys.deepSleepMode) {
        M5Cardputer.update();
        if (millis() - s_lastSleepChargeMs >= 500) {
            s_lastSleepChargeMs = millis();
            g_sys.batteryPct = constrain((int)M5Cardputer.Power.getBatteryLevel(), 0, 100);
            updateChargingDetector(true);
        }
        bool nowKeyDown = M5Cardputer.Keyboard.isPressed();
        bool wakeReq = false;
        if (millis() - g_sys.sleepEnteredMs > 300) {
            if (M5Cardputer.BtnA.wasPressed() || (!s_prevWakeKeyDown && nowKeyDown)) {
                wakeReq = true;
            }
        }
        s_prevWakeKeyDown = nowKeyDown;
        if (wakeReq) {
            s_prevWakeKeyDown = false;
            exitChargeSleepMode();
            InputEvent dummy;
            while (popInputEvent(dummy)) {}
            switchApp(APP_HOME);
        } else {
            delay(50);
        }
        return;
    }
    s_prevWakeKeyDown = false;

    M5Cardputer.update();
    pollKeyboardInput();
    pollSerialCtl();

    bool hadInput = false;
    InputEvent ev;
    while (popInputEvent(ev)) {
        hadInput = true;
        if (g_sys.screensaverActive) {
            exitScreensaver();
            continue;
        }
        if (g_sys.screenAsleep) {
            wakeScreen();
            continue;
        }
        if (g_sys.flashlightOn) {
            setFlashlight(false);
            continue;
        }
        playKeyClick();
        if (s_apps[g_sys.activeApp]) {
            s_apps[g_sys.activeApp]->onInput(ev);
        }
        if (g_sys.deepSleepMode) return;
    }

    tickHardware(hadInput);

    static uint32_t s_lastSaverDrawMs = 0;
    if (g_sys.screensaverActive) {
        uint32_t now = millis();
        bool toastOn = (now < g_sys.toastUntilMs);
        if (now - s_lastSaverDrawMs >= (toastOn ? 120UL : 1000UL)) {
            s_lastSaverDrawMs = now;
            ui::drawScreensaverFrame(s_spr);
            s_spr.pushSprite(0, 0);
        }
        delay(80);
        return;
    }

    if (!g_sys.screenAsleep) {
        if (g_sys.flashlightOn) {
            s_spr.fillScreen(TFT_WHITE);
            s_spr.setTextColor(TFT_BLACK);
            s_spr.setTextSize(1);
            s_spr.setTextDatum(MC_DATUM);
            s_spr.drawString("FLASHLIGHT - Any key to exit", SCREEN_W / 2, SCREEN_H / 2);
            s_spr.setTextDatum(TL_DATUM);
            s_spr.pushSprite(0, 0);
        } else if (s_apps[g_sys.activeApp]) {
            s_apps[g_sys.activeApp]->onTick();
            s_apps[g_sys.activeApp]->render(s_spr);
            s_spr.pushSprite(0, 0);
        }
    }

    delay(g_settings.lowPowerMode ? 35 : 16);
}

