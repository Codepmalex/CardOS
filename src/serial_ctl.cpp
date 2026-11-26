#include "serial_ctl.h"
#include "config.h"
#include "state.h"
#include "input.h"
#include "storage.h"
#include "net_weather.h"
#include "hardware.h"
#include "app_installer.h"
#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <time.h>

extern void switchApp(AppId id);
extern const char* getAppName(AppId id);
extern bool openAppByName(const String& name);
extern void triggerHomeSearch(const String& q);
extern bool installAndBootBinInteractive(const String& path, const String& displayName);
extern M5Canvas* getScreenCanvas();
extern void openTimezoneMap();

static String s_cmdBuf;

void initSerialCtl() {
    Serial.begin(115200);
    s_cmdBuf.reserve(192);
}

static void handleLine(String line) {
    line.trim();
    if (line.isEmpty()) return;

    int sp = line.indexOf(' ');
    String cmd = (sp < 0) ? line : line.substring(0, sp);
    String arg = (sp < 0) ? "" : line.substring(sp + 1);
    cmd.toLowerCase();
    arg.trim();

    if (cmd == "ping") {
        Serial.println("PONG CardOS");
        return;
    }

    if (cmd == "status") {
        JsonDocument doc;
        doc["os"] = "CardOS";
        doc["app"] = getAppName(g_sys.activeApp);
        doc["appId"] = (int)g_sys.activeApp;
        doc["battery"] = g_sys.batteryPct;
        doc["batteryMv"] = g_sys.batteryMv;
        doc["charging"] = g_sys.isCharging;
        doc["sd"] = g_sys.sdMounted;
        doc["sdTotalMB"] = g_sys.sdTotalMB;
        doc["sdFreeMB"] = g_sys.sdFreeMB;
        doc["spiffs"] = g_sys.spiffsMounted;
        doc["imu"] = g_sys.imuReady;
        doc["wifi"] = g_sys.wifiConnected;
        doc["wifiStatus"] = g_sys.wifiStatus;
        doc["ip"] = g_sys.wifiIp;
        doc["ntp"] = g_sys.ntpSynced;
        doc["heapFree"] = ESP.getFreeHeap();
        doc["installedBin"] = g_settings.installedBinName;
        doc["theme"] = activeTheme().name;
        doc["brightness"] = g_settings.brightness;
        doc["profile"] = PROFILE_NAMES[g_settings.deviceProfile % PROF_COUNT];
        doc["dispW"] = M5Cardputer.Display.width();
        doc["dispH"] = M5Cardputer.Display.height();
        doc["rot"] = M5Cardputer.Display.getRotation();
        doc["keyClick"] = g_settings.keyClick;
        doc["asleep"] = g_sys.screenAsleep;

        struct tm tmInfo;
        if (getSafeLocalTime(tmInfo)) {
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tmInfo);
            doc["time"] = tbuf;
        }
        if (g_sys.weather.valid) {
            doc["weatherTempC"] = g_sys.weather.tempC;
            doc["weatherCode"] = g_sys.weather.weatherCode;
            doc["weatherCity"] = g_sys.weather.city;
        }

        String out;
        serializeJson(doc, out);
        Serial.println(out);
        return;
    }

    if (cmd == "apps") {
        JsonDocument doc;
        JsonArray stock = doc["stock"].to<JsonArray>();
        for (int i = 0; i < APP_COUNT; ++i) {
            stock.add(getAppName((AppId)i));
        }
        JsonArray sdbins = doc["sd_bins"].to<JsonArray>();
        auto bins = listSdBinApps();
        for (auto& b : bins) {
            JsonObject o = sdbins.add<JsonObject>();
            o["name"] = b.name;
            o["path"] = b.path;
            o["size"] = b.size;
        }
        String out;
        serializeJson(doc, out);
        Serial.println(out);
        return;
    }

    if (cmd == "ls") {
        String path = arg.isEmpty() ? "/" : arg;
        auto entries = listDir(path.c_str());
        JsonDocument doc;
        doc["path"] = path;
        JsonArray arr = doc["entries"].to<JsonArray>();
        for (auto& e : entries) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = e.name;
            o["path"] = e.path;
            o["dir"] = e.isDir;
            o["size"] = e.size;
        }
        String out;
        serializeJson(doc, out);
        Serial.println(out);
        return;
    }

    if (cmd == "home") {
        switchApp(APP_HOME);
        Serial.println("OK home");
        return;
    }

    if (cmd == "open") {
        if (openAppByName(arg)) {
            Serial.println("OK open " + arg);
        } else {
            Serial.println("ERR unknown app: " + arg);
        }
        return;
    }

    if (cmd == "search") {
        triggerHomeSearch(arg);
        Serial.println("OK search " + arg);
        return;
    }

    if (cmd == "key") {
        String k = arg;
        k.toLowerCase();
        if (k == "up")        pushInputEvent({CK_UP, 0, false, false, true});
        else if (k == "down") pushInputEvent({CK_DOWN, 0, false, false, true});
        else if (k == "left") pushInputEvent({CK_LEFT, 0, false, false, true});
        else if (k == "right")pushInputEvent({CK_RIGHT, 0, false, false, true});
        else if (k == "enter")pushInputEvent({CK_ENTER, 0, false, false, true});
        else if (k == "esc")  pushInputEvent({CK_ESC, 0, false, false, true});
        else if (k == "del" || k == "bs" || k == "backspace") pushInputEvent({CK_BACKSPACE, 0, false, false, true});
        else if (k == "tab")  pushInputEvent({CK_TAB, 0, false, false, true});
        else if (k == "space")pushInputEvent({CK_CHAR, ' ', false, false, true});
        else if (arg.length() == 1) pushInputEvent({CK_CHAR, arg[0], false, false, true});
        else { Serial.println("ERR unknown key"); return; }
        Serial.println("OK key " + arg);
        return;
    }

    if (cmd == "type") {
        for (size_t i = 0; i < arg.length(); ++i) {
            pushInputEvent({CK_CHAR, arg[i], false, false, true});
        }
        Serial.println("OK type");
        return;
    }

    if (cmd == "wifi") {
        int sp2 = arg.indexOf(' ');
        String sub = (sp2 < 0) ? arg : arg.substring(0, sp2);
        String rest = (sp2 < 0) ? "" : arg.substring(sp2 + 1);
        sub.toLowerCase();
        if (sub == "scan") {
            uint32_t seqBefore = getWifiScanSeq();
            requestWifiScan();
            uint32_t t0 = millis();
            while (getWifiScanSeq() == seqBefore && millis() - t0 < 5000) {
                delay(25);
            }
            auto nets = getWifiScanResults();
            JsonDocument doc;
            JsonArray arr = doc["networks"].to<JsonArray>();
            for (auto& n : nets) {
                JsonObject o = arr.add<JsonObject>();
                o["ssid"] = n.ssid;
                o["rssi"] = n.rssi;
                o["open"] = n.open;
            }
            String out;
            serializeJson(doc, out);
            Serial.println(out);
            return;
        } else if (sub == "set") {
            int sp3 = rest.indexOf(' ');
            String ssid = (sp3 < 0) ? rest : rest.substring(0, sp3);
            String pass = (sp3 < 0) ? "" : rest.substring(sp3 + 1);
            strlcpy(g_settings.wifiSsid, ssid.c_str(), sizeof(g_settings.wifiSsid));
            strlcpy(g_settings.wifiPass, pass.c_str(), sizeof(g_settings.wifiPass));
            saveSettings();
            requestWifiConnect();
            Serial.println("OK wifi connecting to " + ssid);
            return;
        }
        Serial.println("ERR wifi subcmd");
        return;
    }

    if (cmd == "weather" && arg == "refresh") {
        requestWeatherRefresh();
        Serial.println("OK weather refresh");
        return;
    }

    if (cmd == "setting") {
        int sp2 = arg.indexOf(' ');
        if (sp2 < 0) { Serial.println("ERR usage: setting <key> <val>"); return; }
        String k = arg.substring(0, sp2);
        String v = arg.substring(sp2 + 1);
        k.toLowerCase(); v.trim();
        if (k == "theme")           g_settings.themeIdx = v.toInt() % THEME_COUNT;
        else if (k == "brightness") { g_settings.brightness = constrain(v.toInt(), 30, 255); applyBrightness(g_settings.brightness); }
        else if (k == "timeout")    g_settings.screenTimeoutSec = v.toInt();
        else if (k == "led")        { g_settings.ledEnabled = (v.toInt() != 0); updateLedEngine(); }
        else if (k == "ledmode")    { g_settings.ledMode = v.toInt() % LED_MODE_COUNT; updateLedEngine(); }
        else if (k == "24h")        g_settings.use24h = (v.toInt() != 0);
        else if (k == "tempf")      g_settings.tempFahrenheit = (v.toInt() != 0);
        else if (k == "city")       { strlcpy(g_settings.weatherCity, v.c_str(), sizeof(g_settings.weatherCity)); g_settings.weatherLat = 0; g_settings.weatherLon = 0; }
        else if (k == "tz")         { g_settings.tzIdx = v.toInt() % TZ_COUNT; applyTimezoneConfig(); }
        else { Serial.println("ERR unknown setting"); return; }
        saveSettings();
        Serial.println("OK setting " + k);
        return;
    }

    if (cmd == "install") {
        if (arg.isEmpty()) { Serial.println("ERR usage: install <binpath>"); return; }
        Serial.println("OK installing " + arg);
        Serial.flush();
        installAndBootBinInteractive(arg, arg);
        return;
    }

    if (cmd == "sleep") {
        sleepScreen();
        Serial.println("OK sleep");
        return;
    }

    if (cmd == "wake") {
        wakeScreen();
        Serial.println("OK wake");
        return;
    }

    if (cmd == "screenshot") {
        M5Canvas* spr = getScreenCanvas();
        if (!spr || !spr->getBuffer()) {
            Serial.println("ERR no framebuffer");
            return;
        }
        const uint8_t* ptr = (const uint8_t*)spr->getBuffer();
        const size_t total = (size_t)SCREEN_W * (size_t)SCREEN_H * 2;
        Serial.printf("SCREENSHOT %d %d %u\n", SCREEN_W, SCREEN_H, (unsigned)total);
        Serial.flush();
        size_t off = 0;
        while (off < total) {
            size_t chunk = min((size_t)512, total - off);
            Serial.write(ptr + off, chunk);
            off += chunk;
            delay(1);
        }
        Serial.println("\nOK screenshot");
        return;
    }

    if (cmd == "cat") {
        if (arg.isEmpty()) { Serial.println("ERR usage: cat <path>"); return; }
        Serial.println(readTextFile(arg.c_str(), 4096));
        Serial.println("EOF");
        return;
    }

    if (cmd == "tzmap") {
        openTimezoneMap();
        Serial.println("OK tzmap");
        return;
    }

    if (cmd == "profile") {
        String p = arg;
        p.toLowerCase();
        if (p == "sound")       g_settings.deviceProfile = PROF_SOUND;
        else if (p == "silent") g_settings.deviceProfile = PROF_SILENT;
        else if (p == "anon" || p == "anonymous") g_settings.deviceProfile = PROF_ANON;
        else { Serial.println("ERR profile <sound|silent|anon>"); return; }
        saveSettings();
        updateLedEngine();
        Serial.println("OK profile " + String(PROFILE_NAMES[g_settings.deviceProfile]));
        return;
    }

    if (cmd == "testnotif") {
        String a = arg;
        a.toLowerCase();
        NotifApp na = NOTIF_SYSTEM;
        if (a.startsWith("cal")) na = NOTIF_CALENDAR;
        else if (a.startsWith("w") || a.startsWith("rain")) na = NOTIF_WEATHER;
        else if (a.startsWith("bat")) na = NOTIF_BATTERY;
        else if (a.startsWith("min")) na = NOTIF_MINER;
        triggerNotification(na, "Test Notification");
        Serial.println("OK testnotif");
        return;
    }

    if (cmd == "testcharge") {
        playChargingAnimation(g_sys.deepSleepMode || g_sys.screenAsleep);
        Serial.println("OK testcharge");
        return;
    }

    if (cmd == "screensaver") {
        enterScreensaver();
        Serial.println("OK screensaver");
        return;
    }

    if (cmd == "lowpower") {
        g_settings.lowPowerMode = (arg.toInt() != 0);
        saveSettings();
        applyPowerProfile();
        Serial.println(String("OK lowpower ") + (g_settings.lowPowerMode ? "ON" : "OFF"));
        return;
    }

    if (cmd == "widget") {
        g_settings.homeWidget = (uint8_t)(arg.toInt() % WIDGET_COUNT);
        saveSettings();
        Serial.println(String("OK widget ") + HOME_WIDGET_NAMES[g_settings.homeWidget]);
        return;
    }

    if (cmd == "store") {
        if (arg.startsWith("url ")) {
            String u = arg.substring(4);
            u.trim();
            strlcpy(g_settings.storeUrl, u.c_str(), sizeof(g_settings.storeUrl));
            saveSettings();
            Serial.println("OK store url");
            return;
        }
        Serial.println("ERR usage: store url <url>");
        return;
    }

    if (cmd == "miner") {
        int sp2 = arg.indexOf(' ');
        String sub = (sp2 < 0) ? arg : arg.substring(0, sp2);
        String rest = (sp2 < 0) ? "" : arg.substring(sp2 + 1);
        sub.toLowerCase();
        rest.trim();
        if (sub == "refresh") {
            requestMinerRefresh();
            Serial.println("OK miner refresh");
            return;
        } else if (sub == "clear") {
            g_settings.minerWallet[0] = '\0';
            if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
                g_sys.minerPool = MinerPoolLive();
                xSemaphoreGive(g_sysMutex);
            }
            saveSettings();
            Serial.println("OK miner cleared");
            return;
        } else if (sub == "pool") {
            int s1 = rest.indexOf(' ');
            if (s1 < 0) {
                Serial.println("ERR usage: miner pool <coin> <wallet>");
                return;
            }
            String coin = rest.substring(0, s1);
            String wal = rest.substring(s1 + 1);
            coin.trim(); coin.toLowerCase(); wal.trim();
            strlcpy(g_settings.minerCoin, coin.c_str(), sizeof(g_settings.minerCoin));
            strlcpy(g_settings.minerWallet, wal.c_str(), sizeof(g_settings.minerWallet));
            if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
                g_sys.minerPool = MinerPoolLive();
                xSemaphoreGive(g_sysMutex);
            }
            saveSettings();
            requestMinerRefresh();
            Serial.println("OK miner pool set");
            return;
        }
        Serial.println("ERR usage: miner <pool <coin> <wallet>|clear|refresh>");
        return;
    }

    if (cmd == "reorder") {
        int sp2 = arg.indexOf(' ');
        if (sp2 < 0) { Serial.println("ERR usage: reorder <from> <to>"); return; }
        int a = arg.substring(0, sp2).toInt();
        int b = arg.substring(sp2 + 1).toInt();
        int n = (int)g_layout.order.size();
        if (a < 0 || a >= n || b < 0 || b >= n) { Serial.println("ERR index"); return; }
        std::swap(g_layout.order[a], g_layout.order[b]);
        saveHomeLayout();
        if (g_sys.activeApp == APP_HOME) switchApp(APP_HOME);
        Serial.println("OK reorder");
        return;
    }

    if (cmd == "reboot") {
        saveWeatherAndTimeCache();
        Serial.println("OK rebooting");
        Serial.flush();
        delay(50);
        rebootToCardOS();
        return;
    }

    if (cmd == "help") {
        Serial.println("Commands: ping, status, apps, ls <dir>, home, open <app>, search <q>, key <k>, type <txt>, wifi scan, wifi set <ssid> <pass>, weather refresh, setting <k> <v>, reboot");
        return;
    }

    Serial.println("ERR unknown cmd (try 'help')");
}

void pollSerialCtl() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            handleLine(s_cmdBuf);
            s_cmdBuf = "";
        } else if (s_cmdBuf.length() < 180) {
            s_cmdBuf += c;
        }
    }
}
