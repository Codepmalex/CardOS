#pragma once
#include <Arduino.h>
#include <stdint.h>

// Display
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int STATUS_BAR_H = 16;
constexpr const char* CARDOS_VERSION = "2.3";

// Cardputer ADV Hardware Pins
constexpr int SD_SCK_PIN  = 40;
constexpr int SD_MISO_PIN = 39;
constexpr int SD_MOSI_PIN = 14;
constexpr int SD_CS_PIN   = 12;

constexpr int LED_PIN     = 21;
// Note: GPIO 38 is LCD backlight_gpio (M5GFX Light_PWM); never override with pinMode/digitalWrite
constexpr int BTN_G0_PIN  = 0;
constexpr int IR_TX_PIN   = 44;

// App IDs
enum AppId : uint8_t {
    APP_HOME = 0,
    APP_NOTES,
    APP_CALENDAR,
    APP_CALC,
    APP_GYRO,
    APP_CONVERTER,
    APP_WEATHER,
    APP_FOLDERS,
    APP_USB_MSC,
    APP_STORE,
    APP_WIFI,
    APP_SETTINGS,
    APP_SLEEP,
    APP_MINER,
    APP_SYSINFO,
    APP_SNAKE,
    APP_FLAPPY,
    APP_MUSIC,
    APP_RECORDER,
    APP_APOS,
    APP_TERMINAL,
    APP_LUA_RUNNER,
    APP_COUNT
};

// Side Button (G0) Actions
enum BtnAction : uint8_t {
    BTN_ACT_HOME = 0,
    BTN_ACT_SLEEP,
    BTN_ACT_FLASHLIGHT,
    BTN_ACT_BRIGHTNESS,
    BTN_ACT_SEARCH,
    BTN_ACT_NOTES,
    BTN_ACT_CALC,
    BTN_ACT_MUTE,
    BTN_ACT_LED_TOGGLE,
    BTN_ACT_PROFILE,
    BTN_ACT_SCREENSAVER,
    BTN_ACT_COUNT
};

// Device Sound/LED Profiles
enum DeviceProfile : uint8_t {
    PROF_SOUND = 0,
    PROF_SILENT,
    PROF_ANON,
    PROF_COUNT
};

// Notification App Channels
enum NotifApp : uint8_t {
    NOTIF_CALENDAR = 0,
    NOTIF_WEATHER,
    NOTIF_BATTERY,
    NOTIF_SYSTEM,
    NOTIF_MINER,
    NOTIF_UPDATES,
    NOTIF_PHONE,
    NOTIF_APP_COUNT
};

constexpr int MAX_MINER_WORKERS = 6;
constexpr int MAX_SAVED_WIFIS   = 20;
constexpr int MAX_STORE_CATALOG = 64;

enum HomeWidget : uint8_t {
    WIDGET_WEATHER = 0,
    WIDGET_MINER,
    WIDGET_SYSINFO,
    WIDGET_CALENDAR,
    WIDGET_NOTES,
    WIDGET_SNAKE,
    WIDGET_FLAPPY,
    WIDGET_MUSIC,
    WIDGET_NONE,
    WIDGET_COUNT
};

// RGB LED Modes
enum LedMode : uint8_t {
    LED_OFF = 0,
    LED_SOLID,
    LED_BREATHE,
    LED_RAINBOW,
    LED_BATTERY,
    LED_NOTIFY,
    LED_MODE_COUNT
};

struct ThemePalette {
    const char* name;
    uint16_t bg;
    uint16_t surface;
    uint16_t surface2;
    uint16_t accent;
    uint16_t accent2;
    uint16_t text;
    uint16_t textDim;
    uint16_t border;
    uint16_t ok;
    uint16_t warn;
    uint16_t err;
};

// Helper macro to convert RGB888 to RGB565 at compile time
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr int THEME_COUNT = 6;
static const ThemePalette THEMES[THEME_COUNT] = {
    // 0: Midnight
    {"Midnight",   rgb565(12, 16, 28),  rgb565(22, 30, 50),  rgb565(34, 46, 74),  rgb565(56, 189, 248), rgb565(129, 140, 248), rgb565(241, 245, 249), rgb565(148, 163, 184), rgb565(51, 65, 85),   rgb565(52, 211, 153), rgb565(251, 191, 36), rgb565(248, 113, 113)},
    // 1: Cyber Neon
    {"Cyber Neon", rgb565(10, 8, 20),   rgb565(22, 18, 42),  rgb565(38, 28, 68),  rgb565(255, 42, 133), rgb565(0, 245, 212),   rgb565(248, 250, 252), rgb565(167, 139, 250), rgb565(88, 28, 135),  rgb565(0, 245, 212),  rgb565(250, 204, 21), rgb565(255, 75, 110)},
    // 2: Emerald
    {"Emerald",    rgb565(8, 20, 16),   rgb565(16, 36, 28),  rgb565(26, 56, 44),  rgb565(52, 211, 153), rgb565(16, 185, 129),  rgb565(236, 253, 245), rgb565(110, 231, 183), rgb565(30, 75, 58),   rgb565(74, 222, 128), rgb565(251, 191, 36), rgb565(248, 113, 113)},
    // 3: Sunset
    {"Sunset",     rgb565(24, 14, 18),  rgb565(42, 24, 30),  rgb565(64, 36, 44),  rgb565(251, 146, 60), rgb565(244, 63, 94),   rgb565(255, 247, 237), rgb565(253, 186, 116), rgb565(90, 48, 56),   rgb565(74, 222, 128), rgb565(250, 204, 21), rgb565(248, 113, 113)},
    // 4: Monochrome
    {"Monochrome", rgb565(10, 10, 10),  rgb565(26, 26, 26),  rgb565(46, 46, 46),  rgb565(255, 255, 255),rgb565(180, 180, 180), rgb565(255, 255, 255), rgb565(150, 150, 150), rgb565(70, 70, 70),   rgb565(220, 220, 220),rgb565(190, 190, 190),rgb565(255, 100, 100)},
    // 5: Paper Light
    {"PaperLight", rgb565(241, 245, 249),rgb565(255, 255, 255),rgb565(226, 232, 240),rgb565(37, 99, 235),  rgb565(14, 165, 233),  rgb565(15, 23, 42),    rgb565(100, 116, 139), rgb565(203, 213, 225),rgb565(22, 163, 74),  rgb565(217, 119, 6),  rgb565(220, 38, 38)}
};

constexpr int ACCENT_COUNT = 7;
static const uint16_t ACCENT_PRESETS[ACCENT_COUNT] = {
    0, // Theme default
    rgb565(56, 189, 248),  // Sky
    rgb565(52, 211, 153),  // Mint
    rgb565(251, 146, 60),  // Amber
    rgb565(244, 63, 94),   // Rose
    rgb565(167, 139, 250), // Violet
    rgb565(250, 204, 21)   // Gold
};
static const char* ACCENT_NAMES[ACCENT_COUNT] = {
    "Theme", "Sky", "Mint", "Amber", "Rose", "Violet", "Gold"
};

struct TimezoneInfo {
    const char* name;
    const char* utcLabel;
    const char* posixTz;
    int16_t mapX; // x on 240x66 world map (0..239)
    int16_t mapY; // y on 240x66 world map (18..83)
};

constexpr int TZ_COUNT = 18;
static const TimezoneInfo TZ_LIST[TZ_COUNT] = {
    {"Hawaii",         "UTC-10", "HST10",                            16, 48},
    {"Alaska",         "UTC-9",  "AKST9AKDT,M3.2.0,M11.1.0",         24, 28},
    {"US Pacific",     "UTC-8",  "PST8PDT,M3.2.0,M11.1.0",           38, 39},
    {"US Mountain",    "UTC-7",  "MST7MDT,M3.2.0,M11.1.0",           48, 39},
    {"US Central",     "UTC-6",  "CST6CDT,M3.2.0,M11.1.0",           57, 40},
    {"US Eastern",     "UTC-5",  "EST5EDT,M3.2.0,M11.1.0",           68, 40},
    {"Atlantic",       "UTC-4",  "AST4ADT,M3.2.0,M11.1.0",           77, 46},
    {"Brazil (Sao)",   "UTC-3",  "BRT3",                             89, 64},
    {"UTC / GMT",      "UTC+0",  "UTC0",                            116, 35},
    {"UK (London)",    "UTC+0",  "GMT0BST,M3.5.0/1,M10.5.0",        119, 32},
    {"EU Central",     "UTC+1",  "CET-1CEST,M3.5.0,M10.5.0/3",      129, 34},
    {"Cairo / EET",    "UTC+2",  "EET-2EEST,M4.5.5/0,M10.4.4/24",   140, 44},
    {"Moscow / Riyadh","UTC+3",  "MSK-3",                           147, 31},
    {"Dubai (GST)",    "UTC+4",  "GST-4",                           157, 46},
    {"India (IST)",    "UTC+5:30","IST-5:30",                       171, 48},
    {"Bangkok / ICT",  "UTC+7",  "ICT-7",                           186, 51},
    {"China / SG",     "UTC+8",  "CST-8",                           196, 44},
    {"Japan / Sydney", "UTC+9",  "JST-9",                           212, 39}
};

static const char* BTN_ACTION_NAMES[BTN_ACT_COUNT] = {
    "Go Home",
    "Sleep / Wake",
    "Flashlight",
    "Cycle Bright",
    "Focus Search",
    "Open Notes",
    "Open Calc",
    "Mute Audio",
    "Toggle LED",
    "Cycle Mode",
    "Screensaver"
};

static const char* LED_MODE_NAMES[LED_MODE_COUNT] = {
    "Off", "Solid", "Breathe", "Rainbow", "Battery", "Notify"
};

static const char* PROFILE_NAMES[PROF_COUNT] = {
    "Sound", "Silent", "Anonymous"
};

static const char* NOTIF_APP_NAMES[NOTIF_APP_COUNT] = {
    "Calendar", "Weather/Rain", "Battery", "System", "Miner", "Updates", "Phone"
};

static const char* NOTIF_TONE_NAMES[4] = {
    "Chime", "Double", "Alert", "Soft"
};

static const char* NOTIF_LED_PAT_NAMES[4] = {
    "Pulse", "Blink", "Fast", "Solid"
};

static const uint32_t LED_PRESETS[7] = {
    0x38BDF8, 0x34D399, 0xFB923C, 0xF43F5E, 0xA78BFA, 0xFACC15, 0xFFFFFF
};
static const char* LED_COLOR_NAMES[7] = {
    "Sky", "Mint", "Amber", "Rose", "Violet", "Gold", "White"
};

static const char* HOME_WIDGET_NAMES[WIDGET_COUNT] = {
    "Weather", "Miner", "SysInfo", "Calendar", "Notes", "Snake", "Flappy", "Music", "Off"
};

