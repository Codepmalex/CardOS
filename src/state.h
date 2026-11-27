#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

struct NotifRule {
    bool enabled = true;
    uint8_t volume = 160;      // 0..255
    uint8_t toneIdx = 0;       // 0..3 (Chime, Double, Alert, Soft)
    bool ledEnabled = true;
    uint8_t ledColorIdx = 0;   // 0..6 (LED_PRESETS)
    uint8_t ledPattern = 0;    // 0..2 (Pulse, Fast, Solid)

    NotifRule() = default;
    NotifRule(bool e, uint8_t v, uint8_t t, bool le, uint8_t lc, uint8_t lp)
        : enabled(e), volume(v), toneIdx(t), ledEnabled(le), ledColorIdx(lc), ledPattern(lp) {}
};

struct SavedWifi {
    char ssid[36] = "";
    char pass[64] = "";
};

struct Settings {
    // Appearance
    uint8_t themeIdx = 0;
    uint8_t accentIdx = 0;

    // Display & Power
    uint8_t brightness = 180;        // 30..255
    uint16_t screenTimeoutSec = 120; // 0 = never
    bool dimBeforeSleep = true;
    bool flipScreen = false;         // false = rotation 0, true = rotation 2 (180 deg)
    bool screensaverEnabled = true;
    uint8_t screensaverBright = 35;  // 10..200
    bool lowPowerMode = false;
    uint8_t homeWidget = WIDGET_WEATHER;
    uint8_t screensaverWidget = WIDGET_WEATHER;

    // Buttons & Audio
    uint8_t btnShortAction = BTN_ACT_HOME;
    uint8_t btnLongAction  = BTN_ACT_SLEEP;
    bool keyClick = false;
    uint8_t volume = 90;             // 0..255 (UI/Key clicks; notifications use per-app rule volume)
    uint8_t mediaVolume = 140;       // 0..255 (Master Media output for Music/Voice apps)
    uint8_t micSensitivity = 5;      // 1..10 manual Voice mic gain
    bool micAutoSens = true;         // Auto AGC mic sensitivity in Voice Recorder
    uint8_t deviceProfile = PROF_SILENT;

    // Bluetooth
    bool btEnabled = false;
    char btName[24] = "CardOS";

    // RGB LED
    bool ledEnabled = true;
    uint8_t ledMode = LED_NOTIFY;
    uint32_t ledColor = 0x38BDF8;    // RGB888
    uint8_t ledBrightness = 40;      // 5..255

    // Wi-Fi
    char wifiSsid[64] = "";
    char wifiPass[64] = "";
    bool wifiAutoConnect = true;
    SavedWifi savedWifis[MAX_SAVED_WIFIS];

    // Time & Weather
    bool use24h = false;
    uint8_t tzIdx = 5; // US Eastern default
    bool tempFahrenheit = true;
    char weatherCity[48] = "Auto";
    float weatherLat = 0.0f;
    float weatherLon = 0.0f;
    uint16_t weatherRefreshMin = 30;

    // Notifications
    bool notifEnabled = true;
    bool notifCalendar = true;
    bool notifLowBatt = true;
    bool notifWeather = true;
    bool notifSound = true;
    bool notifLed = true;
    uint8_t rainAlertPct = 50;
    NotifRule notifRules[NOTIF_APP_COUNT];

    // Kryptex Pool Miner Monitor (empty by default; user sets coin & wallet)
    char minerCoin[16] = "xtm-rx";
    char minerWallet[128] = "";

    // Games, App Store & ApOS
    uint16_t snakeHighScore = 0;
    uint16_t flappyHighScore = 0;
    bool autoUpdateApps = false;
    char storeUrl[128] = "https://raw.githubusercontent.com/Codepmalex/CardOS/main/store/catalog.json";
    char apSsid[32] = "CardOS-ApOS";
    char apPass[32] = "cardos1234";

    // Installed third-party app tracking (in app1 partition)
    char installedBinName[64] = "";
    uint32_t installedBinSize = 0;

    Settings() {
        // Sensible per-app defaults
        notifRules[NOTIF_CALENDAR] = NotifRule(true, 160, 0, true, 0, 0); // Sky, Chime, Pulse
        notifRules[NOTIF_WEATHER]  = NotifRule(true, 180, 1, true, 1, 2); // Mint, Double, Fast
        notifRules[NOTIF_BATTERY]  = NotifRule(true, 200, 2, true, 3, 2); // Rose, Alert, Fast
        notifRules[NOTIF_SYSTEM]   = NotifRule(true, 120, 3, true, 4, 0); // Violet, Soft, Pulse
        notifRules[NOTIF_MINER]    = NotifRule(true, 180, 1, true, 5, 1); // Gold, Double, Blink
        notifRules[NOTIF_UPDATES]  = NotifRule(true, 160, 0, true, 0, 0); // Sky, Chime, Pulse
        notifRules[NOTIF_PHONE]    = NotifRule(true, 190, 1, true, 1, 1); // Mint, Double, Blink
    }
};

struct HourForecast {
    char label[6] = "--";
    float tempC = 0.0f;
    int precipProb = 0;
    int weatherCode = 0;
};

struct DayForecast {
    char dayLabel[8] = "---";
    int weatherCode = -1;
    float tempMaxC = 0.0f;
    float tempMinC = 0.0f;
    int precipProbMax = 0;
};

struct WeatherData {
    bool valid = false;
    bool fetching = false;
    float tempC = 0.0f;
    float feelsLikeC = 0.0f;
    int humidity = 0;
    float windKmh = 0.0f;
    int windDirDeg = 0;
    float pressureHpa = 0.0f;
    float precipMm = 0.0f;
    int precipProbNow = 0;
    float uvMax = 0.0f;
    char sunrise[8] = "--:--";
    char sunset[8] = "--:--";
    int weatherCode = -1;
    bool isDay = true;
    char city[48] = "Offline";
    char errorMsg[48] = "";
    uint32_t lastFetchMs = 0;
    int hourlyCount = 0;
    HourForecast hourly[12];
    DayForecast forecast[5];
};

struct MinerWorkerLive {
    char name[20] = "";
    char scheme[8] = "";
    bool online = false;
    float hash30m = 0.0f;
    float hash3h = 0.0f;
    float hash24h = 0.0f;
    uint32_t valid = 0;
    uint32_t stale = 0;
    uint32_t invalid = 0;
};

struct MinerPoolLive {
    bool valid = false;
    bool everChecked = false;
    bool fetching = false;
    char errorMsg[40] = "";
    float totalHash30m = 0.0f;
    float totalHash3h = 0.0f;
    float totalHash24h = 0.0f;
    int onlineWorkers = 0;
    int totalWorkers = 0;
    float balTotal = 0.0f;
    float balConfirmed = 0.0f;
    float balUnconfirmed = 0.0f;
    float threshold = 0.0f;
    float reachedPct = 0.0f;
    uint32_t totalBadShares = 0;
    uint32_t lastFetchMs = 0;
    int workerCount = 0;
    MinerWorkerLive workers[MAX_MINER_WORKERS];
};

struct SystemState {
    bool sdMounted = false;
    uint32_t sdTotalMB = 0;
    uint32_t sdFreeMB = 0;
    bool spiffsMounted = false;
    bool imuReady = false;

    bool wifiConnected = false;
    bool wifiConnecting = false;
    char wifiStatus[32] = "Off";
    char wifiIp[20] = "0.0.0.0";
    int wifiRssi = 0;
    bool ntpSynced = false;
    bool btEnabled = false;
    bool btConnected = false;

    int batteryPct = 100;
    int batteryMv = 4000;
    bool isCharging = false;

    AppId activeApp = APP_HOME;
    AppId returnApp = APP_HOME;
    volatile bool usbMscActive = false;
    volatile bool deepSleepMode = false;
    volatile bool screensaverActive = false;
    uint32_t sleepEnteredMs = 0;
    bool flashlightOn = false;
    bool screenAsleep = false;
    bool screenDimmed = false;
    uint32_t lastActivityMs = 0;

    // Cross-app launch payloads
    char pendingNotePath[96] = "";
    char pendingFolderPath[96] = "";
    bool focusSearchOnHome = false;

    // Notification / Toast
    char toastMsg[64] = "";
    uint32_t toastStartMs = 0;
    uint32_t toastUntilMs = 0;
    uint32_t ledNotifyUntilMs = 0;
    uint32_t ledNotifyColor = 0x38BDF8;
    uint8_t ledNotifyPattern = 0;
    int todayEventsCount = 0;
    int notesCount = 0;
    bool lowBattNotified = false;
    bool weatherNotified = false;
    bool rainNotified = false;
    uint8_t unreadNotifCount = 0;
    char latestOsVer[16] = "";
    int pendingAppUpdates = 0;

struct MusicLiveState {
    bool playing = false;
    bool paused = false;
    bool shuffle = false;
    bool streaming = false;
    char trackName[48] = "";
    char playlist[32] = "";
};

    WeatherData weather;
    MinerPoolLive minerPool;
    MusicLiveState music;
};

struct HomeLayout {
    std::vector<String> order;              // Ordered stock + installed Store app keys
    std::vector<String> pinnedBins;         // Pinned SD .bin paths shown directly on Homescreen
    std::vector<String> installedStoreApps; // Downloaded Store app keys ("weather", "miner", "flappy")
};

extern Settings g_settings;
extern HomeLayout g_layout;
extern SystemState g_sys;
extern SemaphoreHandle_t g_sysMutex;
extern SemaphoreHandle_t g_tlsMutex;

void initState();
void loadSettings();
void saveSettings();
void factoryResetSettings();

// SD/SPIFFS JSON persistence (/cardos_settings.json, /cardos_layout.json, /cardos_cache.json)
void syncSettingsWithStorage();
void loadHomeLayout();
void saveHomeLayout();
void saveWeatherAndTimeCache();
void loadWeatherAndTimeCache();
bool isBinPinned(const String& path);
bool toggleBinPin(const String& path);
AppId appIdFromKey(const String& key);
const char* keyFromAppId(AppId id);

// Flicker-free local time reader
bool getSafeLocalTime(struct tm& out);

// Multi-Wi-Fi saved credentials
void upsertSavedWifi(const char* ssid, const char* pass);
bool removeSavedWifi(const char* ssid);
const char* findSavedWifiPass(const char* ssid);

// GitHub App Store installation helpers (downloadable apps start uninstalled)
bool isStoreAppKey(const String& key);
bool isStoreAppInstalled(const String& key);
bool isLuaStoreAppInstalled(const String& key);
String getStoreAppDisplayName(const String& key);
void installStoreAppKey(const String& key);
void uninstallStoreAppKey(const String& key);
bool isWidgetAvailable(uint8_t widgetId);
void ensureValidHomeWidget();

bool getMscBootFlag();
void setMscBootFlag(bool enable);

const ThemePalette& activeTheme();
uint16_t activeAccent();
void showToast(const char* msg, uint32_t durationMs = 2400, bool notifyLed = false);
