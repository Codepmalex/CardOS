#include "state.h"
#include "storage.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

Settings g_settings;
HomeLayout g_layout;
SystemState g_sys;
SemaphoreHandle_t g_sysMutex = nullptr;

static Preferences prefs;

static constexpr const char* SETTINGS_FILE = "/cardos_settings.json";
static constexpr const char* LAYOUT_FILE   = "/cardos_layout.json";
static constexpr const char* CACHE_FILE    = "/cardos_cache.json";

struct AppKeyPair {
    AppId id;
    const char* key;
};

static const AppKeyPair APP_KEYS[] = {
    {APP_NOTES,     "notes"},
    {APP_CALENDAR,  "calendar"},
    {APP_CALC,      "calc"},
    {APP_GYRO,      "gyro"},
    {APP_CONVERTER, "convert"},
    {APP_WEATHER,   "weather"},
    {APP_FOLDERS,   "folders"},
    {APP_USB_MSC,   "usb"},
    {APP_WIFI,      "wifi"},
    {APP_STORE,     "more"},
    {APP_SETTINGS,  "settings"},
    {APP_SLEEP,     "sleep"},
    {APP_MINER,     "miner"},
    {APP_SYSINFO,   "sysinfo"},
    {APP_SNAKE,     "snake"},
    {APP_FLAPPY,    "flappy"},
    {APP_MUSIC,     "music"}
};
static constexpr int APP_KEYS_COUNT = sizeof(APP_KEYS) / sizeof(APP_KEYS[0]);

AppId appIdFromKey(const String& key) {
    for (int i = 0; i < APP_KEYS_COUNT; ++i) {
        if (key == APP_KEYS[i].key) return APP_KEYS[i].id;
    }
    return APP_COUNT;
}

const char* keyFromAppId(AppId id) {
    for (int i = 0; i < APP_KEYS_COUNT; ++i) {
        if (APP_KEYS[i].id == id) return APP_KEYS[i].key;
    }
    return "";
}

bool getSafeLocalTime(struct tm& out) {
    static struct tm s_lastGoodTm;
    static bool s_hasGood = false;
    time_t nowSec = time(nullptr);
    if (nowSec > 1600000000L) {
        struct tm tmp;
        if (localtime_r(&nowSec, &tmp) && tmp.tm_year >= 120) {
            s_lastGoodTm = tmp;
            s_hasGood = true;
            out = tmp;
            return true;
        }
    }
    if (s_hasGood) {
        out = s_lastGoodTm;
        return true;
    }
    return false;
}

const char* findSavedWifiPass(const char* ssid) {
    if (!ssid || strlen(ssid) == 0) return nullptr;
    for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
        if (strlen(g_settings.savedWifis[i].ssid) > 0 && strcmp(g_settings.savedWifis[i].ssid, ssid) == 0) {
            return g_settings.savedWifis[i].pass;
        }
    }
    return nullptr;
}

void upsertSavedWifi(const char* ssid, const char* pass) {
    if (!ssid || strlen(ssid) == 0) return;
    const char* p = pass ? pass : "";
    for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
        if (strcmp(g_settings.savedWifis[i].ssid, ssid) == 0) {
            // Move to front (slot 0 = most recently used)
            SavedWifi cur;
            strlcpy(cur.ssid, ssid, sizeof(cur.ssid));
            strlcpy(cur.pass, p, sizeof(cur.pass));
            for (int j = i; j > 0; --j) g_settings.savedWifis[j] = g_settings.savedWifis[j - 1];
            g_settings.savedWifis[0] = cur;
            return;
        }
    }
    for (int j = MAX_SAVED_WIFIS - 1; j > 0; --j) {
        g_settings.savedWifis[j] = g_settings.savedWifis[j - 1];
    }
    strlcpy(g_settings.savedWifis[0].ssid, ssid, sizeof(g_settings.savedWifis[0].ssid));
    strlcpy(g_settings.savedWifis[0].pass, p, sizeof(g_settings.savedWifis[0].pass));
}

bool removeSavedWifi(const char* ssid) {
    if (!ssid || strlen(ssid) == 0) return false;
    for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
        if (strcmp(g_settings.savedWifis[i].ssid, ssid) == 0) {
            for (int j = i; j < MAX_SAVED_WIFIS - 1; ++j) {
                g_settings.savedWifis[j] = g_settings.savedWifis[j + 1];
            }
            g_settings.savedWifis[MAX_SAVED_WIFIS - 1] = SavedWifi();
            return true;
        }
    }
    return false;
}

bool isStoreAppKey(const String& key) {
    return key == "weather" || key == "miner" || key == "flappy";
}

bool isLuaStoreAppInstalled(const String& key) {
    if (key.isEmpty() || (!g_sys.sdMounted && !g_sys.spiffsMounted)) return false;
    String luaPath = "/store/" + key + ".lua";
    return activeFS().exists(luaPath.c_str());
}

String getStoreAppDisplayName(const String& key) {
    if (g_sys.sdMounted || g_sys.spiffsMounted) {
        String pkg = "/store/" + key + ".json";
        if (activeFS().exists(pkg.c_str())) {
            String raw = readTextFile(pkg.c_str(), 4096);
            JsonDocument doc;
            if (!raw.isEmpty() && deserializeJson(doc, raw) == DeserializationError::Ok) {
                const char* nm = doc["name"] | "";
                if (strlen(nm) > 0) return String(nm);
            }
        }
    }
    return key;
}

bool isStoreAppInstalled(const String& key) {
    for (const auto& k : g_layout.installedStoreApps) {
        if (k == key) return true;
    }
    return false;
}

void installStoreAppKey(const String& key) {
    if (appIdFromKey(key) == APP_COUNT && !isLuaStoreAppInstalled(key)) return;
    if (!isStoreAppInstalled(key)) {
        g_layout.installedStoreApps.push_back(key);
    }
    bool inOrder = false;
    for (const auto& k : g_layout.order) {
        if (k == key) { inOrder = true; break; }
    }
    if (!inOrder) {
        g_layout.order.push_back(key);
    }
    saveHomeLayout();
}

void uninstallStoreAppKey(const String& key) {
    for (size_t i = 0; i < g_layout.installedStoreApps.size(); ++i) {
        if (g_layout.installedStoreApps[i] == key) {
            g_layout.installedStoreApps.erase(g_layout.installedStoreApps.begin() + i);
            break;
        }
    }
    for (size_t i = 0; i < g_layout.order.size(); ++i) {
        if (g_layout.order[i] == key) {
            g_layout.order.erase(g_layout.order.begin() + i);
            break;
        }
    }
    String pkg = "/store/" + key + ".json";
    String lua = "/store/" + key + ".lua";
    if (g_sys.sdMounted || g_sys.spiffsMounted) {
        if (activeFS().exists(pkg.c_str())) {
            String raw = readTextFile(pkg.c_str(), 2048);
            JsonDocument doc;
            if (!raw.isEmpty() && deserializeJson(doc, raw) == DeserializationError::Ok) {
                String dataRel = String(doc["data"] | "");
                if (!dataRel.isEmpty()) {
                    int sl = dataRel.lastIndexOf('/');
                    String baseName = (sl >= 0) ? dataRel.substring(sl + 1) : dataRel;
                    String dataPath = "/store/" + baseName;
                    if (activeFS().exists(dataPath.c_str())) activeFS().remove(dataPath.c_str());
                }
            }
            activeFS().remove(pkg.c_str());
        }
        if (activeFS().exists(lua.c_str())) activeFS().remove(lua.c_str());
    }
    ensureValidHomeWidget();
    saveHomeLayout();
}

bool isWidgetAvailable(uint8_t widgetId) {
    switch (widgetId) {
        case WIDGET_WEATHER: return isStoreAppInstalled("weather");
        case WIDGET_MINER:   return isStoreAppInstalled("miner");
        case WIDGET_FLAPPY:  return isStoreAppInstalled("flappy");
        case WIDGET_SYSINFO:
        case WIDGET_CALENDAR:
        case WIDGET_NOTES:
        case WIDGET_SNAKE:
        case WIDGET_MUSIC:
        case WIDGET_NONE:
            return true;
        default:
            return false;
    }
}

void ensureValidHomeWidget() {
    if (!isWidgetAvailable(g_settings.homeWidget)) {
        g_settings.homeWidget = WIDGET_SYSINFO;
        saveSettings();
    }
}

void initState() {
    if (!g_sysMutex) {
        g_sysMutex = xSemaphoreCreateMutex();
    }
    g_sys.lastActivityMs = millis();
    loadSettings();
}

static void saveSettingsToNvs() {
    if (!prefs.begin("cardos", false)) return;

    prefs.putUChar("theme", g_settings.themeIdx);
    prefs.putUChar("accent", g_settings.accentIdx);
    prefs.putUChar("bright", g_settings.brightness);
    prefs.putUShort("timeout", g_settings.screenTimeoutSec);
    prefs.putBool("dim", g_settings.dimBeforeSleep);
    prefs.putBool("flip", g_settings.flipScreen);
    prefs.putBool("ssEn", g_settings.screensaverEnabled);
    prefs.putUChar("ssBr", g_settings.screensaverBright);
    prefs.putBool("lowP", g_settings.lowPowerMode);
    prefs.putUChar("hWid", g_settings.homeWidget);
    prefs.putUChar("ssWid", g_settings.screensaverWidget);
    prefs.putUChar("btnS", g_settings.btnShortAction);
    prefs.putUChar("btnL", g_settings.btnLongAction);
    prefs.putBool("kclick", g_settings.keyClick);
    prefs.putUChar("vol", g_settings.volume);
    prefs.putUChar("prof", g_settings.deviceProfile);
    prefs.putBool("ledOn", g_settings.ledEnabled);
    prefs.putUChar("ledMode", g_settings.ledMode);
    prefs.putUInt("ledCol", g_settings.ledColor);
    prefs.putUChar("ledBr", g_settings.ledBrightness);

    prefs.putString("wSsid", g_settings.wifiSsid);
    prefs.putString("wPass", g_settings.wifiPass);
    prefs.putBool("wAuto", g_settings.wifiAutoConnect);

    prefs.putBool("use24h", g_settings.use24h);
    prefs.putUChar("tzIdx", g_settings.tzIdx);
    prefs.putBool("tempF", g_settings.tempFahrenheit);
    prefs.putString("wCity", g_settings.weatherCity);
    prefs.putFloat("wLat", g_settings.weatherLat);
    prefs.putFloat("wLon", g_settings.weatherLon);
    prefs.putUShort("wRef", g_settings.weatherRefreshMin);

    prefs.putBool("nEn", g_settings.notifEnabled);
    prefs.putBool("nCal", g_settings.notifCalendar);
    prefs.putBool("nBat", g_settings.notifLowBatt);
    prefs.putBool("nWth", g_settings.notifWeather);
    prefs.putBool("nSnd", g_settings.notifSound);
    prefs.putBool("nLed", g_settings.notifLed);
    prefs.putUChar("rainPct", g_settings.rainAlertPct);
    prefs.putBytes("nRules", g_settings.notifRules, sizeof(g_settings.notifRules));
    prefs.putString("mCoin", g_settings.minerCoin);
    prefs.putString("mWal", g_settings.minerWallet);
    prefs.putBytes("sWifis", g_settings.savedWifis, sizeof(g_settings.savedWifis));
    prefs.putUShort("snkHi", g_settings.snakeHighScore);
    prefs.putUShort("flpHi", g_settings.flappyHighScore);
    prefs.putBool("autoUpd", g_settings.autoUpdateApps);
    prefs.putString("stUrl", g_settings.storeUrl);

    prefs.putString("instBin", g_settings.installedBinName);
    prefs.putUInt("instSz", g_settings.installedBinSize);

    prefs.end();
}

static void saveSettingsToFile() {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    JsonDocument doc;
    doc["theme"] = g_settings.themeIdx;
    doc["accent"] = g_settings.accentIdx;
    doc["brightness"] = g_settings.brightness;
    doc["timeout"] = g_settings.screenTimeoutSec;
    doc["dim"] = g_settings.dimBeforeSleep;
    doc["flip"] = g_settings.flipScreen;
    doc["ssEnabled"] = g_settings.screensaverEnabled;
    doc["ssBright"] = g_settings.screensaverBright;
    doc["lowPower"] = g_settings.lowPowerMode;
    doc["homeWidget"] = g_settings.homeWidget;
    doc["ssWidget"] = g_settings.screensaverWidget;
    doc["btnShort"] = g_settings.btnShortAction;
    doc["btnLong"] = g_settings.btnLongAction;
    doc["keyClick"] = g_settings.keyClick;
    doc["volume"] = g_settings.volume;
    doc["profile"] = g_settings.deviceProfile;
    doc["ledEnabled"] = g_settings.ledEnabled;
    doc["ledMode"] = g_settings.ledMode;
    doc["ledColor"] = g_settings.ledColor;
    doc["ledBrightness"] = g_settings.ledBrightness;
    doc["wifiSsid"] = g_settings.wifiSsid;
    doc["wifiPass"] = g_settings.wifiPass;
    doc["wifiAuto"] = g_settings.wifiAutoConnect;
    doc["use24h"] = g_settings.use24h;
    doc["tzIdx"] = g_settings.tzIdx;
    doc["tempF"] = g_settings.tempFahrenheit;
    doc["weatherCity"] = g_settings.weatherCity;
    doc["weatherLat"] = g_settings.weatherLat;
    doc["weatherLon"] = g_settings.weatherLon;
    doc["weatherRefresh"] = g_settings.weatherRefreshMin;
    doc["notifEnabled"] = g_settings.notifEnabled;
    doc["rainAlertPct"] = g_settings.rainAlertPct;

    JsonArray rules = doc["notifRules"].to<JsonArray>();
    for (int i = 0; i < NOTIF_APP_COUNT; ++i) {
        JsonObject r = rules.add<JsonObject>();
        const auto& nr = g_settings.notifRules[i];
        r["en"] = nr.enabled;
        r["vol"] = nr.volume;
        r["tone"] = nr.toneIdx;
        r["led"] = nr.ledEnabled;
        r["col"] = nr.ledColorIdx;
        r["pat"] = nr.ledPattern;
    }

    doc["minerCoin"] = g_settings.minerCoin;
    doc["minerWallet"] = g_settings.minerWallet;
    doc["snakeHigh"] = g_settings.snakeHighScore;
    doc["flappyHigh"] = g_settings.flappyHighScore;
    doc["autoUpdate"] = g_settings.autoUpdateApps;
    doc["storeUrl"] = g_settings.storeUrl;

    JsonArray sw = doc["savedWifis"].to<JsonArray>();
    for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
        if (strlen(g_settings.savedWifis[i].ssid) == 0) continue;
        JsonObject o = sw.add<JsonObject>();
        o["ssid"] = g_settings.savedWifis[i].ssid;
        o["pass"] = g_settings.savedWifis[i].pass;
    }

    doc["installedBin"] = g_settings.installedBinName;
    doc["installedSize"] = g_settings.installedBinSize;

    String out;
    serializeJsonPretty(doc, out);
    writeTextFile(SETTINGS_FILE, out);
}

void loadSettings() {
    if (!prefs.begin("cardos", true)) return;

    g_settings.themeIdx          = prefs.getUChar("theme", 0) % THEME_COUNT;
    g_settings.accentIdx         = prefs.getUChar("accent", 0) % ACCENT_COUNT;
    g_settings.brightness        = prefs.getUChar("bright", 180);
    g_settings.screenTimeoutSec  = prefs.getUShort("timeout", 120);
    g_settings.dimBeforeSleep    = prefs.getBool("dim", true);
    g_settings.flipScreen        = prefs.getBool("flip", false);
    g_settings.screensaverEnabled = prefs.getBool("ssEn", true);
    g_settings.screensaverBright = prefs.getUChar("ssBr", 35);
    g_settings.lowPowerMode      = prefs.getBool("lowP", false);
    g_settings.homeWidget        = prefs.getUChar("hWid", WIDGET_WEATHER) % WIDGET_COUNT;
    g_settings.screensaverWidget = prefs.getUChar("ssWid", WIDGET_WEATHER) % WIDGET_COUNT;
    g_settings.btnShortAction    = prefs.getUChar("btnS", BTN_ACT_HOME) % BTN_ACT_COUNT;
    g_settings.btnLongAction     = prefs.getUChar("btnL", BTN_ACT_SLEEP) % BTN_ACT_COUNT;
    g_settings.keyClick          = prefs.getBool("kclick", false);
    g_settings.volume            = prefs.getUChar("vol", 90);
    g_settings.deviceProfile     = prefs.getUChar("prof", PROF_SILENT) % PROF_COUNT;
    g_settings.ledEnabled        = prefs.getBool("ledOn", true);
    g_settings.ledMode           = prefs.getUChar("ledMode", LED_BATTERY) % LED_MODE_COUNT;
    g_settings.ledColor          = prefs.getUInt("ledCol", 0x38BDF8);
    g_settings.ledBrightness     = prefs.getUChar("ledBr", 40);

    String ssid = prefs.getString("wSsid", "");
    String pass = prefs.getString("wPass", "");
    strlcpy(g_settings.wifiSsid, ssid.c_str(), sizeof(g_settings.wifiSsid));
    strlcpy(g_settings.wifiPass, pass.c_str(), sizeof(g_settings.wifiPass));
    g_settings.wifiAutoConnect   = prefs.getBool("wAuto", true);

    g_settings.use24h            = prefs.getBool("use24h", false);
    g_settings.tzIdx             = prefs.getUChar("tzIdx", 5) % TZ_COUNT;
    g_settings.tempFahrenheit    = prefs.getBool("tempF", true);
    String wcity = prefs.getString("wCity", "Auto");
    strlcpy(g_settings.weatherCity, wcity.c_str(), sizeof(g_settings.weatherCity));
    g_settings.weatherLat        = prefs.getFloat("wLat", 0.0f);
    g_settings.weatherLon        = prefs.getFloat("wLon", 0.0f);
    g_settings.weatherRefreshMin = prefs.getUShort("wRef", 30);

    g_settings.notifEnabled      = prefs.getBool("nEn", true);
    g_settings.notifCalendar     = prefs.getBool("nCal", true);
    g_settings.notifLowBatt      = prefs.getBool("nBat", true);
    g_settings.notifWeather      = prefs.getBool("nWth", true);
    g_settings.notifSound        = prefs.getBool("nSnd", true);
    g_settings.notifLed          = prefs.getBool("nLed", true);
    g_settings.rainAlertPct      = prefs.getUChar("rainPct", 50);
    if (prefs.getBytesLength("nRules") == sizeof(g_settings.notifRules)) {
        prefs.getBytes("nRules", g_settings.notifRules, sizeof(g_settings.notifRules));
    }
    String mc = prefs.getString("mCoin", "xtm-rx");
    String mw = prefs.getString("mWal", "");
    strlcpy(g_settings.minerCoin, mc.c_str(), sizeof(g_settings.minerCoin));
    strlcpy(g_settings.minerWallet, mw.c_str(), sizeof(g_settings.minerWallet));
    size_t swLen = prefs.getBytesLength("sWifis");
    if (swLen > 0 && swLen <= sizeof(g_settings.savedWifis) && (swLen % sizeof(SavedWifi)) == 0) {
        prefs.getBytes("sWifis", g_settings.savedWifis, swLen);
    }
    if (strlen(g_settings.wifiSsid) > 0 && findSavedWifiPass(g_settings.wifiSsid) == nullptr) {
        upsertSavedWifi(g_settings.wifiSsid, g_settings.wifiPass);
    }
    g_settings.snakeHighScore  = prefs.getUShort("snkHi", 0);
    g_settings.flappyHighScore = prefs.getUShort("flpHi", 0);
    g_settings.autoUpdateApps  = prefs.getBool("autoUpd", false);
    String stUrl = prefs.getString("stUrl", "https://raw.githubusercontent.com/Codepmalex/CardOS/main/store/catalog.json");
    if (stUrl.indexOf("alexlaspro/CardOS-Store") >= 0) {
        stUrl = "https://raw.githubusercontent.com/Codepmalex/CardOS/main/store/catalog.json";
    }
    strlcpy(g_settings.storeUrl, stUrl.c_str(), sizeof(g_settings.storeUrl));

    String instName = prefs.getString("instBin", "");
    strlcpy(g_settings.installedBinName, instName.c_str(), sizeof(g_settings.installedBinName));
    g_settings.installedBinSize  = prefs.getUInt("instSz", 0);

    prefs.end();
}

void syncSettingsWithStorage() {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    if (!activeFS().exists(SETTINGS_FILE)) {
        saveSettingsToFile();
        return;
    }
    String raw = readTextFile(SETTINGS_FILE, 12288);
    if (raw.isEmpty()) return;
    JsonDocument doc;
    if (deserializeJson(doc, raw) != DeserializationError::Ok) return;

    if (doc["theme"].is<int>())          g_settings.themeIdx = doc["theme"].as<int>() % THEME_COUNT;
    if (doc["accent"].is<int>())         g_settings.accentIdx = doc["accent"].as<int>() % ACCENT_COUNT;
    if (doc["brightness"].is<int>())     g_settings.brightness = constrain(doc["brightness"].as<int>(), 30, 255);
    if (doc["timeout"].is<int>())        g_settings.screenTimeoutSec = doc["timeout"].as<int>();
    if (doc["dim"].is<bool>())           g_settings.dimBeforeSleep = doc["dim"].as<bool>();
    if (doc["flip"].is<bool>())          g_settings.flipScreen = doc["flip"].as<bool>();
    if (doc["ssEnabled"].is<bool>())     g_settings.screensaverEnabled = doc["ssEnabled"].as<bool>();
    if (doc["ssBright"].is<int>())       g_settings.screensaverBright = constrain(doc["ssBright"].as<int>(), 10, 200);
    if (doc["lowPower"].is<bool>())      g_settings.lowPowerMode = doc["lowPower"].as<bool>();
    if (doc["homeWidget"].is<int>())     g_settings.homeWidget = doc["homeWidget"].as<int>() % WIDGET_COUNT;
    if (doc["ssWidget"].is<int>())       g_settings.screensaverWidget = doc["ssWidget"].as<int>() % WIDGET_COUNT;
    if (doc["btnShort"].is<int>())       g_settings.btnShortAction = doc["btnShort"].as<int>() % BTN_ACT_COUNT;
    if (doc["btnLong"].is<int>())        g_settings.btnLongAction = doc["btnLong"].as<int>() % BTN_ACT_COUNT;
    if (doc["keyClick"].is<bool>())      g_settings.keyClick = doc["keyClick"].as<bool>();
    if (doc["volume"].is<int>())         g_settings.volume = constrain(doc["volume"].as<int>(), 0, 255);
    if (doc["profile"].is<int>())        g_settings.deviceProfile = doc["profile"].as<int>() % PROF_COUNT;
    if (doc["ledEnabled"].is<bool>())    g_settings.ledEnabled = doc["ledEnabled"].as<bool>();
    if (doc["ledMode"].is<int>())        g_settings.ledMode = doc["ledMode"].as<int>() % LED_MODE_COUNT;
    if (doc["ledColor"].is<uint32_t>())  g_settings.ledColor = doc["ledColor"].as<uint32_t>();
    if (doc["ledBrightness"].is<int>())  g_settings.ledBrightness = constrain(doc["ledBrightness"].as<int>(), 5, 255);

    if (doc["wifiSsid"].is<const char*>()) strlcpy(g_settings.wifiSsid, doc["wifiSsid"].as<const char*>(), sizeof(g_settings.wifiSsid));
    if (doc["wifiPass"].is<const char*>()) strlcpy(g_settings.wifiPass, doc["wifiPass"].as<const char*>(), sizeof(g_settings.wifiPass));
    if (doc["wifiAuto"].is<bool>())        g_settings.wifiAutoConnect = doc["wifiAuto"].as<bool>();

    if (doc["use24h"].is<bool>())          g_settings.use24h = doc["use24h"].as<bool>();
    if (doc["tzIdx"].is<int>())            g_settings.tzIdx = doc["tzIdx"].as<int>() % TZ_COUNT;
    if (doc["tempF"].is<bool>())           g_settings.tempFahrenheit = doc["tempF"].as<bool>();
    if (doc["weatherCity"].is<const char*>()) strlcpy(g_settings.weatherCity, doc["weatherCity"].as<const char*>(), sizeof(g_settings.weatherCity));
    if (doc["weatherLat"].is<float>())     g_settings.weatherLat = doc["weatherLat"].as<float>();
    if (doc["weatherLon"].is<float>())     g_settings.weatherLon = doc["weatherLon"].as<float>();
    if (doc["weatherRefresh"].is<int>())   g_settings.weatherRefreshMin = doc["weatherRefresh"].as<int>();

    if (doc["notifEnabled"].is<bool>())    g_settings.notifEnabled = doc["notifEnabled"].as<bool>();
    if (doc["rainAlertPct"].is<int>())     g_settings.rainAlertPct = constrain(doc["rainAlertPct"].as<int>(), 10, 100);

    if (doc["notifRules"].is<JsonArray>()) {
        JsonArray rules = doc["notifRules"].as<JsonArray>();
        for (int i = 0; i < NOTIF_APP_COUNT && i < (int)rules.size(); ++i) {
            JsonObject r = rules[i];
            if (r["en"].is<bool>())   g_settings.notifRules[i].enabled = r["en"].as<bool>();
            if (r["vol"].is<int>())   g_settings.notifRules[i].volume = constrain(r["vol"].as<int>(), 0, 255);
            if (r["tone"].is<int>())  g_settings.notifRules[i].toneIdx = r["tone"].as<int>() % 4;
            if (r["led"].is<bool>())  g_settings.notifRules[i].ledEnabled = r["led"].as<bool>();
            if (r["col"].is<int>())   g_settings.notifRules[i].ledColorIdx = r["col"].as<int>() % 7;
            if (r["pat"].is<int>())   g_settings.notifRules[i].ledPattern = r["pat"].as<int>() % 3;
        }
    }

    if (doc["minerCoin"].is<const char*>())   strlcpy(g_settings.minerCoin, doc["minerCoin"].as<const char*>(), sizeof(g_settings.minerCoin));
    if (doc["minerWallet"].is<const char*>()) strlcpy(g_settings.minerWallet, doc["minerWallet"].as<const char*>(), sizeof(g_settings.minerWallet));
    if (doc["snakeHigh"].is<int>())           g_settings.snakeHighScore = (uint16_t)max(0, doc["snakeHigh"].as<int>());
    if (doc["flappyHigh"].is<int>())          g_settings.flappyHighScore = (uint16_t)max(0, doc["flappyHigh"].as<int>());
    if (doc["autoUpdate"].is<bool>())         g_settings.autoUpdateApps = doc["autoUpdate"].as<bool>();
    if (doc["storeUrl"].is<const char*>()) {
        String u = doc["storeUrl"].as<const char*>();
        if (u.indexOf("alexlaspro/CardOS-Store") >= 0) {
            u = "https://raw.githubusercontent.com/Codepmalex/CardOS/main/store/catalog.json";
        }
        strlcpy(g_settings.storeUrl, u.c_str(), sizeof(g_settings.storeUrl));
    }

    if (doc["savedWifis"].is<JsonArray>()) {
        for (int i = 0; i < MAX_SAVED_WIFIS; ++i) g_settings.savedWifis[i] = SavedWifi();
        JsonArray sw = doc["savedWifis"].as<JsonArray>();
        int idx = 0;
        for (JsonObject o : sw) {
            if (idx >= MAX_SAVED_WIFIS) break;
            const char* s = o["ssid"] | "";
            if (strlen(s) > 0) {
                strlcpy(g_settings.savedWifis[idx].ssid, s, sizeof(g_settings.savedWifis[idx].ssid));
                strlcpy(g_settings.savedWifis[idx].pass, o["pass"] | "", sizeof(g_settings.savedWifis[idx].pass));
                idx++;
            }
        }
    }
    if (strlen(g_settings.wifiSsid) > 0 && findSavedWifiPass(g_settings.wifiSsid) == nullptr) {
        upsertSavedWifi(g_settings.wifiSsid, g_settings.wifiPass);
    }

    if (doc["installedBin"].is<const char*>()) strlcpy(g_settings.installedBinName, doc["installedBin"].as<const char*>(), sizeof(g_settings.installedBinName));
    if (doc["installedSize"].is<uint32_t>())   g_settings.installedBinSize = doc["installedSize"].as<uint32_t>();

    saveSettingsToNvs();
}

void saveSettings() {
    saveSettingsToNvs();
    saveSettingsToFile();
}

void factoryResetSettings() {
    if (prefs.begin("cardos", false)) {
        prefs.clear();
        prefs.end();
    }
    g_settings = Settings();
    saveSettings();
}

void saveWeatherAndTimeCache() {
    if (g_sys.usbMscActive) return;
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    JsonDocument doc;
    time_t nowSec = time(nullptr);
    if (nowSec > 1700000000) {
        doc["epoch"] = (uint32_t)nowSec;
    }

    WeatherData w;
    if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
        w = g_sys.weather;
        xSemaphoreGive(g_sysMutex);
    } else {
        w = g_sys.weather;
    }

    if (w.valid) {
        JsonObject wo = doc["weather"].to<JsonObject>();
        wo["tempC"] = w.tempC;
        wo["feelsC"] = w.feelsLikeC;
        wo["hum"] = w.humidity;
        wo["wind"] = w.windKmh;
        wo["wdir"] = w.windDirDeg;
        wo["pres"] = w.pressureHpa;
        wo["precMm"] = w.precipMm;
        wo["precPct"] = w.precipProbNow;
        wo["uv"] = w.uvMax;
        wo["sr"] = w.sunrise;
        wo["ss"] = w.sunset;
        wo["code"] = w.weatherCode;
        wo["day"] = w.isDay;
        wo["city"] = w.city;

        JsonArray hr = wo["hourly"].to<JsonArray>();
        for (int i = 0; i < w.hourlyCount && i < 12; ++i) {
            JsonObject h = hr.add<JsonObject>();
            h["l"] = w.hourly[i].label;
            h["t"] = w.hourly[i].tempC;
            h["p"] = w.hourly[i].precipProb;
            h["c"] = w.hourly[i].weatherCode;
        }

        JsonArray dy = wo["daily"].to<JsonArray>();
        for (int i = 0; i < 5; ++i) {
            JsonObject d = dy.add<JsonObject>();
            d["l"] = w.forecast[i].dayLabel;
            d["c"] = w.forecast[i].weatherCode;
            d["hi"] = w.forecast[i].tempMaxC;
            d["lo"] = w.forecast[i].tempMinC;
            d["p"] = w.forecast[i].precipProbMax;
        }
    }

    String out;
    serializeJson(doc, out);
    writeTextFile(CACHE_FILE, out);
}

void loadWeatherAndTimeCache() {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    if (!activeFS().exists(CACHE_FILE)) return;
    String raw = readTextFile(CACHE_FILE, 12288);
    if (raw.isEmpty()) return;

    JsonDocument doc;
    if (deserializeJson(doc, raw) != DeserializationError::Ok) return;

    uint32_t cachedEpoch = doc["epoch"] | 0;
    if (cachedEpoch > 1700000000 && time(nullptr) < 1700000000) {
        struct timeval tv = { (time_t)cachedEpoch, 0 };
        settimeofday(&tv, nullptr);
    }

    if (doc["weather"].is<JsonObject>()) {
        JsonObject wo = doc["weather"];
        WeatherData w;
        w.valid = true;
        w.tempC = wo["tempC"] | 0.0f;
        w.feelsLikeC = wo["feelsC"] | w.tempC;
        w.humidity = wo["hum"] | 0;
        w.windKmh = wo["wind"] | 0.0f;
        w.windDirDeg = wo["wdir"] | 0;
        w.pressureHpa = wo["pres"] | 0.0f;
        w.precipMm = wo["precMm"] | 0.0f;
        w.precipProbNow = wo["precPct"] | 0;
        w.uvMax = wo["uv"] | 0.0f;
        strlcpy(w.sunrise, wo["sr"] | "--:--", sizeof(w.sunrise));
        strlcpy(w.sunset, wo["ss"] | "--:--", sizeof(w.sunset));
        w.weatherCode = wo["code"] | 0;
        w.isDay = wo["day"] | true;
        strlcpy(w.city, wo["city"] | "Cached", sizeof(w.city));

        JsonArray hr = wo["hourly"];
        w.hourlyCount = min(12, (int)hr.size());
        for (int i = 0; i < w.hourlyCount; ++i) {
            strlcpy(w.hourly[i].label, hr[i]["l"] | "--", sizeof(w.hourly[i].label));
            w.hourly[i].tempC = hr[i]["t"] | 0.0f;
            w.hourly[i].precipProb = hr[i]["p"] | 0;
            w.hourly[i].weatherCode = hr[i]["c"] | 0;
        }

        JsonArray dy = wo["daily"];
        for (int i = 0; i < 5 && i < (int)dy.size(); ++i) {
            strlcpy(w.forecast[i].dayLabel, dy[i]["l"] | "---", sizeof(w.forecast[i].dayLabel));
            w.forecast[i].weatherCode = dy[i]["c"] | 0;
            w.forecast[i].tempMaxC = dy[i]["hi"] | 0.0f;
            w.forecast[i].tempMinC = dy[i]["lo"] | 0.0f;
            w.forecast[i].precipProbMax = dy[i]["p"] | 0;
        }

        if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
            g_sys.weather = w;
            xSemaphoreGive(g_sysMutex);
        } else {
            g_sys.weather = w;
        }
    }
}

void loadHomeLayout() {
    g_layout.order.clear();
    g_layout.pinnedBins.clear();
    g_layout.installedStoreApps.clear();

    if ((g_sys.sdMounted || g_sys.spiffsMounted) && activeFS().exists(LAYOUT_FILE)) {
        String raw = readTextFile(LAYOUT_FILE, 8192);
        JsonDocument doc;
        if (!raw.isEmpty() && deserializeJson(doc, raw) == DeserializationError::Ok) {
            for (JsonVariant v : doc["installedStore"].as<JsonArray>()) {
                String k = v.as<String>();
                if ((isStoreAppKey(k) || isLuaStoreAppInstalled(k)) && !isStoreAppInstalled(k)) {
                    g_layout.installedStoreApps.push_back(k);
                }
            }
            for (JsonVariant v : doc["order"].as<JsonArray>()) {
                String k = v.as<String>();
                if (appIdFromKey(k) != APP_COUNT || isLuaStoreAppInstalled(k)) {
                    bool dup = false;
                    for (auto& ex : g_layout.order) if (ex == k) { dup = true; break; }
                    if (!dup) g_layout.order.push_back(k);
                }
            }
            for (JsonVariant v : doc["pinned"].as<JsonArray>()) {
                String p = v.as<String>();
                if (!p.isEmpty()) g_layout.pinnedBins.push_back(p);
            }
        }
    }

    bool changed = !activeFS().exists(LAYOUT_FILE);

    // Ensure downloadable Store apps only stay on Home if explicitly installed from Store
    for (int i = (int)g_layout.order.size() - 1; i >= 0; --i) {
        const String& k = g_layout.order[i];
        if (isStoreAppKey(k) && !isStoreAppInstalled(k)) {
            g_layout.order.erase(g_layout.order.begin() + i);
            changed = true;
        }
    }

    // Add any missing stock apps (or installed Store apps)
    for (int i = 0; i < APP_KEYS_COUNT; ++i) {
        String k = APP_KEYS[i].key;
        if (isStoreAppKey(k) && !isStoreAppInstalled(k)) continue;
        bool found = false;
        for (auto& ex : g_layout.order) if (ex == k) { found = true; break; }
        if (!found) {
            g_layout.order.push_back(k);
            changed = true;
        }
    }
    // Add any installed downloadable Lua apps missing from order
    for (const auto& k : g_layout.installedStoreApps) {
        if (!isLuaStoreAppInstalled(k)) continue;
        bool found = false;
        for (auto& ex : g_layout.order) if (ex == k) { found = true; break; }
        if (!found) {
            g_layout.order.push_back(k);
            changed = true;
        }
    }
    if (changed) {
        saveHomeLayout();
    }
    ensureValidHomeWidget();
}

void saveHomeLayout() {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    JsonDocument doc;
    JsonArray ord = doc["order"].to<JsonArray>();
    for (auto& k : g_layout.order) ord.add(k);
    JsonArray pin = doc["pinned"].to<JsonArray>();
    for (auto& p : g_layout.pinnedBins) pin.add(p);
    JsonArray inst = doc["installedStore"].to<JsonArray>();
    for (auto& s : g_layout.installedStoreApps) inst.add(s);

    String out;
    serializeJsonPretty(doc, out);
    writeTextFile(LAYOUT_FILE, out);
}

bool isBinPinned(const String& path) {
    for (auto& p : g_layout.pinnedBins) {
        if (p == path) return true;
    }
    return false;
}

bool toggleBinPin(const String& path) {
    for (size_t i = 0; i < g_layout.pinnedBins.size(); ++i) {
        if (g_layout.pinnedBins[i] == path) {
            g_layout.pinnedBins.erase(g_layout.pinnedBins.begin() + i);
            saveHomeLayout();
            return false;
        }
    }
    g_layout.pinnedBins.push_back(path);
    saveHomeLayout();
    return true;
}

bool getMscBootFlag() {
    bool val = false;
    if (prefs.begin("cardos_boot", true)) {
        val = prefs.getBool("msc", false);
        prefs.end();
    }
    return val;
}

void setMscBootFlag(bool enable) {
    if (prefs.begin("cardos_boot", false)) {
        prefs.putBool("msc", enable);
        prefs.end();
    }
}

const ThemePalette& activeTheme() {
    return THEMES[g_settings.themeIdx % THEME_COUNT];
}

uint16_t activeAccent() {
    if (g_settings.accentIdx == 0 || g_settings.accentIdx >= ACCENT_COUNT) {
        return activeTheme().accent;
    }
    return ACCENT_PRESETS[g_settings.accentIdx];
}

void showToast(const char* msg, uint32_t durationMs, bool notifyLed) {
    if (g_sys.deepSleepMode) return;
    strlcpy(g_sys.toastMsg, msg, sizeof(g_sys.toastMsg));
    g_sys.toastStartMs = millis();
    g_sys.toastUntilMs = g_sys.toastStartMs + durationMs;
    if (notifyLed && g_settings.notifEnabled && g_settings.deviceProfile != PROF_ANON) {
        g_sys.ledNotifyUntilMs = g_sys.toastStartMs + 2000;
    }
}
