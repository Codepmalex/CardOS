#include "net_weather.h"
#include "state.h"
#include "hardware.h"
#include "storage.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static volatile bool s_reqWifiConnect = false;
static volatile bool s_reqWeatherRefresh = false;
static volatile bool s_reqMinerRefresh = false;
static volatile bool s_reqUpdateCheck = false;
static volatile bool s_reqWifiScan = false;
static volatile bool s_wifiScanning = false;
static volatile uint32_t s_wifiScanSeq = 0;
static std::vector<WifiNetInfo> s_scanResults;
static TaskHandle_t s_netTaskHandle = nullptr;

static String urlEncode(const String& s) {
    String out;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else if (c == ' ') {
            out += "%20";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
            out += buf;
        }
    }
    return out;
}

void applyTimezoneConfig() {
    const char* tz = TZ_LIST[g_settings.tzIdx % TZ_COUNT].posixTz;
    setenv("TZ", tz, 1);
    tzset();
    configTzTime(tz, "pool.ntp.org", "time.nist.gov", "time.google.com");
}

static bool geolocateByIp(float& lat, float& lon, String& cityOut) {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(7000);
    if (!http.begin(client, "http://ip-api.com/json/?fields=status,city,lat,lon")) {
        return false;
    }
    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
    if (String(doc["status"] | "") != "success") return false;
    lat = doc["lat"] | 0.0f;
    lon = doc["lon"] | 0.0f;
    cityOut = String(doc["city"] | "Local");
    return (lat != 0.0f || lon != 0.0f);
}

static bool geocodeCity(const char* cityName, float& lat, float& lon, String& resolvedName) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    String url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
                 urlEncode(String(cityName)) + "&count=1&language=en&format=json";
    if (!http.begin(client, url)) return false;
    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
    JsonArray results = doc["results"].as<JsonArray>();
    if (results.isNull() || results.size() == 0) return false;
    lat = results[0]["latitude"] | 0.0f;
    lon = results[0]["longitude"] | 0.0f;
    resolvedName = String(results[0]["name"] | cityName);
    return true;
}

static void dayLabelFromIso(const char* isoDate, char* out, size_t outSz) {
    if (!isoDate || strlen(isoDate) < 10) {
        strlcpy(out, "---", outSz);
        return;
    }
    struct tm t = {};
    t.tm_year = atoi(isoDate) - 1900;
    t.tm_mon  = atoi(isoDate + 5) - 1;
    t.tm_mday = atoi(isoDate + 8);
    t.tm_hour = 12;
    mktime(&t);
    static const char* WDAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    strlcpy(out, WDAYS[t.tm_wday % 7], outSz);
}

static void hourLabelFromIso(const char* isoTime, char* out, size_t outSz) {
    // isoTime: YYYY-MM-DDTHH:MM
    if (!isoTime || strlen(isoTime) < 16) {
        strlcpy(out, "--", outSz);
        return;
    }
    int hr = atoi(isoTime + 11);
    if (g_settings.use24h) {
        snprintf(out, outSz, "%02d", hr);
    } else {
        const char* ap = (hr >= 12) ? "p" : "a";
        int h12 = hr % 12;
        if (h12 == 0) h12 = 12;
        snprintf(out, outSz, "%d%s", h12, ap);
    }
}

static void hhmmFromIso(const char* isoTime, char* out, size_t outSz) {
    if (!isoTime || strlen(isoTime) < 16) {
        strlcpy(out, "--:--", outSz);
        return;
    }
    strlcpy(out, isoTime + 11, min(outSz, (size_t)6));
}

static void fetchOpenMeteoWeather() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (xSemaphoreTake(g_sysMutex, portMAX_DELAY)) {
        g_sys.weather.fetching = true;
        strlcpy(g_sys.weather.errorMsg, "", sizeof(g_sys.weather.errorMsg));
        xSemaphoreGive(g_sysMutex);
    }

    float lat = g_settings.weatherLat;
    float lon = g_settings.weatherLon;
    String cityLabel = g_settings.weatherCity;

    if (cityLabel.equalsIgnoreCase("Auto") || cityLabel.isEmpty() || (lat == 0.0f && lon == 0.0f)) {
        String ipCity;
        if (cityLabel.equalsIgnoreCase("Auto") || cityLabel.isEmpty()) {
            if (geolocateByIp(lat, lon, ipCity)) {
                cityLabel = ipCity;
            }
        } else {
            String resolved;
            if (geocodeCity(cityLabel.c_str(), lat, lon, resolved)) {
                cityLabel = resolved;
                g_settings.weatherLat = lat;
                g_settings.weatherLon = lon;
            }
        }
    }

    if (lat == 0.0f && lon == 0.0f) {
        lat = 40.7128f;
        lon = -74.0060f;
        cityLabel = "New York";
    }

    char url[512];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,surface_pressure,wind_speed_10m,wind_direction_10m,is_day"
             "&hourly=temperature_2m,precipitation_probability,weather_code&forecast_hours=12"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,sunrise,sunset&forecast_days=5&timezone=auto",
             lat, lon);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    bool ok = false;
    String err = "HTTP fail";
    bool rainAlert = false;
    int maxRain3h = 0;
    int curCode = 0;

    if (http.begin(client, url)) {
        int code = http.GET();
        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                if (xSemaphoreTake(g_sysMutex, portMAX_DELAY)) {
                    JsonObject cur = doc["current"];
                    g_sys.weather.tempC        = cur["temperature_2m"] | 0.0f;
                    g_sys.weather.feelsLikeC   = cur["apparent_temperature"] | g_sys.weather.tempC;
                    g_sys.weather.humidity     = cur["relative_humidity_2m"] | 0;
                    g_sys.weather.precipMm     = cur["precipitation"] | 0.0f;
                    g_sys.weather.pressureHpa  = cur["surface_pressure"] | 0.0f;
                    g_sys.weather.windKmh      = cur["wind_speed_10m"] | 0.0f;
                    g_sys.weather.windDirDeg   = cur["wind_direction_10m"] | 0;
                    g_sys.weather.weatherCode  = cur["weather_code"] | 0;
                    g_sys.weather.isDay        = (cur["is_day"] | 1) != 0;
                    strlcpy(g_sys.weather.city, cityLabel.c_str(), sizeof(g_sys.weather.city));
                    curCode = g_sys.weather.weatherCode;

                    JsonObject hourly = doc["hourly"];
                    JsonArray hTimes = hourly["time"];
                    JsonArray hTemps = hourly["temperature_2m"];
                    JsonArray hProbs = hourly["precipitation_probability"];
                    JsonArray hCodes = hourly["weather_code"];
                    int hCnt = min(12, (int)hTimes.size());
                    g_sys.weather.hourlyCount = hCnt;
                    for (int i = 0; i < hCnt; ++i) {
                        const char* hs = hTimes[i] | "";
                        hourLabelFromIso(hs, g_sys.weather.hourly[i].label, sizeof(g_sys.weather.hourly[i].label));
                        g_sys.weather.hourly[i].tempC       = (i < (int)hTemps.size()) ? (hTemps[i] | 0.0f) : 0.0f;
                        g_sys.weather.hourly[i].precipProb  = (i < (int)hProbs.size()) ? (hProbs[i] | 0) : 0;
                        g_sys.weather.hourly[i].weatherCode = (i < (int)hCodes.size()) ? (hCodes[i] | 0) : 0;
                        if (i < 3 && g_sys.weather.hourly[i].precipProb > maxRain3h) {
                            maxRain3h = g_sys.weather.hourly[i].precipProb;
                        }
                    }
                    g_sys.weather.precipProbNow = (hCnt > 0) ? g_sys.weather.hourly[0].precipProb : 0;

                    JsonObject daily = doc["daily"];
                    JsonArray dates = daily["time"];
                    JsonArray codes = daily["weather_code"];
                    JsonArray tmaxs = daily["temperature_2m_max"];
                    JsonArray tmins = daily["temperature_2m_min"];
                    JsonArray pmaxs = daily["precipitation_probability_max"];
                    JsonArray uvs   = daily["uv_index_max"];
                    JsonArray srs   = daily["sunrise"];
                    JsonArray sss   = daily["sunset"];

                    g_sys.weather.uvMax = (uvs.size() > 0) ? (uvs[0] | 0.0f) : 0.0f;
                    hhmmFromIso((srs.size() > 0) ? (srs[0] | "") : "", g_sys.weather.sunrise, sizeof(g_sys.weather.sunrise));
                    hhmmFromIso((sss.size() > 0) ? (sss[0] | "") : "", g_sys.weather.sunset, sizeof(g_sys.weather.sunset));

                    for (int i = 0; i < 5; ++i) {
                        const char* ds = (i < (int)dates.size()) ? (dates[i] | "") : "";
                        dayLabelFromIso(ds, g_sys.weather.forecast[i].dayLabel, 8);
                        g_sys.weather.forecast[i].weatherCode   = (i < (int)codes.size()) ? (codes[i] | 0) : 0;
                        g_sys.weather.forecast[i].tempMaxC      = (i < (int)tmaxs.size()) ? (tmaxs[i] | 0.0f) : 0.0f;
                        g_sys.weather.forecast[i].tempMinC      = (i < (int)tmins.size()) ? (tmins[i] | 0.0f) : 0.0f;
                        g_sys.weather.forecast[i].precipProbMax = (i < (int)pmaxs.size()) ? (pmaxs[i] | 0) : 0;
                    }

                    g_sys.weather.valid = true;
                    g_sys.weather.lastFetchMs = millis();
                    strlcpy(g_sys.weather.errorMsg, "", sizeof(g_sys.weather.errorMsg));
                    ok = true;
                    xSemaphoreGive(g_sysMutex);
                }
            } else {
                err = "JSON parse err";
            }
        } else {
            err = "HTTP " + String(code);
        }
        http.end();
    }

    if (xSemaphoreTake(g_sysMutex, portMAX_DELAY)) {
        g_sys.weather.fetching = false;
        if (!ok) {
            strlcpy(g_sys.weather.errorMsg, err.c_str(), sizeof(g_sys.weather.errorMsg));
        }
        xSemaphoreGive(g_sysMutex);
    }

    if (ok) {
        saveWeatherAndTimeCache();

        bool isRaining = (curCode >= 51 && curCode <= 67) || (curCode >= 80 && curCode <= 99);
        rainAlert = isRaining || (maxRain3h >= g_settings.rainAlertPct);
        if (rainAlert) {
            if (!g_sys.rainNotified) {
                g_sys.rainNotified = true;
                char msg[48];
                if (isRaining) snprintf(msg, sizeof(msg), "Rain Now! (%d%%)", maxRain3h);
                else snprintf(msg, sizeof(msg), "Rain Alert: %d%% in 3h", maxRain3h);
                triggerNotification(NOTIF_WEATHER, msg);
            }
        } else {
            g_sys.rainNotified = false;
            if (!g_sys.weatherNotified) {
                g_sys.weatherNotified = true;
                triggerNotification(NOTIF_WEATHER, "Weather updated");
            }
        }
    }
}

static float jsonToFloat(JsonVariantConst v) {
    if (v.is<const char*>()) return (float)atof(v.as<const char*>());
    return v | 0.0f;
}

static void pollKryptexPool() {
    if (!g_sys.wifiConnected || strlen(g_settings.minerWallet) == 0) return;

    const char* coin = strlen(g_settings.minerCoin) ? g_settings.minerCoin : "xtm-rx";
    const char* wallet = g_settings.minerWallet;

    MinerPoolLive prev;
    if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
        g_sys.minerPool.fetching = true;
        prev = g_sys.minerPool;
        xSemaphoreGive(g_sysMutex);
    } else {
        g_sys.minerPool.fetching = true;
        prev = g_sys.minerPool;
    }

    MinerPoolLive next = prev;
    next.fetching = false;
    next.errorMsg[0] = '\0';

    WiFiClientSecure client;
    client.setInsecure();

    char url[256];
    bool workersOk = false;
    {
        snprintf(url, sizeof(url), "https://pool.kryptex.com/%s/api/v3/miner/workers/%s", coin, wallet);
        HTTPClient http;
        http.setConnectTimeout(4500);
        http.setTimeout(7000);
        if (http.begin(client, url)) {
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString();
                JsonDocument doc;
                if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                    JsonArray res = doc["results"];
                    next.totalWorkers = (int)res.size();
                    next.workerCount = min(MAX_MINER_WORKERS, next.totalWorkers);
                    next.onlineWorkers = 0;
                    next.totalHash30m = 0.0f;
                    next.totalHash3h = 0.0f;
                    next.totalHash24h = 0.0f;
                    next.totalBadShares = 0;
                    for (int i = 0; i < next.workerCount; ++i) {
                        JsonObject r = res[i];
                        auto& w = next.workers[i];
                        strlcpy(w.name, r["worker"] | "worker", sizeof(w.name));
                        strlcpy(w.scheme, r["scheme"] | "", sizeof(w.scheme));
                        w.online = (String(r["status"] | "") == "online");
                        w.hash30m = jsonToFloat(r["avg_hashrate_30m"]);
                        w.hash3h  = jsonToFloat(r["avg_hashrate_3h"]);
                        w.hash24h = jsonToFloat(r["avg_hashrate_24h"]);
                        w.valid   = r["valid"] | 0UL;
                        w.stale   = r["stale"] | 0UL;
                        w.invalid = r["invalid"] | 0UL;

                        if (w.online) {
                            next.onlineWorkers++;
                            next.totalHash30m += w.hash30m;
                            next.totalHash3h  += w.hash3h;
                            next.totalHash24h += w.hash24h;
                        }
                        next.totalBadShares += w.stale + w.invalid;
                    }
                    workersOk = true;
                } else {
                    strlcpy(next.errorMsg, "Workers JSON error", sizeof(next.errorMsg));
                }
            } else {
                snprintf(next.errorMsg, sizeof(next.errorMsg), "Workers HTTP %d", code);
            }
            http.end();
        } else {
            strlcpy(next.errorMsg, "Workers conn failed", sizeof(next.errorMsg));
        }
    }

    bool balOk = false;
    {
        snprintf(url, sizeof(url), "https://pool.kryptex.com/%s/api/v1/miner/balance/%s", coin, wallet);
        HTTPClient http;
        http.setConnectTimeout(4500);
        http.setTimeout(7000);
        if (http.begin(client, url)) {
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString();
                JsonDocument doc;
                if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                    next.balTotal       = doc["total"] | 0.0f;
                    next.balConfirmed   = doc["confirmed"] | 0.0f;
                    next.balUnconfirmed = doc["unconfirmed"] | 0.0f;
                    next.threshold      = doc["threshold"] | 0.0f;
                    next.reachedPct     = doc["reached_pct"] | 0.0f;
                    balOk = true;
                }
            } else if (next.errorMsg[0] == '\0') {
                snprintf(next.errorMsg, sizeof(next.errorMsg), "Balance HTTP %d", code);
            }
            http.end();
        }
    }

    if (workersOk || balOk) {
        next.valid = true;
        next.everChecked = true;
        next.lastFetchMs = millis();
        next.errorMsg[0] = '\0';
    }

    if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
        g_sys.minerPool = next;
        xSemaphoreGive(g_sysMutex);
    } else {
        g_sys.minerPool = next;
    }

    // Fire miner alerts on state transitions after initial baseline poll
    if (prev.everChecked && workersOk) {
        for (int i = 0; i < next.workerCount; ++i) {
            for (int j = 0; j < prev.workerCount; ++j) {
                if (strcmp(next.workers[i].name, prev.workers[j].name) == 0 &&
                    strcmp(next.workers[i].scheme, prev.workers[j].scheme) == 0) {
                    if (prev.workers[j].online && !next.workers[i].online) {
                        char msg[48];
                        snprintf(msg, sizeof(msg), "Offline: %s", next.workers[i].name);
                        triggerNotification(NOTIF_MINER, msg);
                    } else if (!prev.workers[j].online && next.workers[i].online) {
                        char msg[48];
                        snprintf(msg, sizeof(msg), "Online: %s (%.0f H/s)", next.workers[i].name, next.workers[i].hash30m);
                        triggerNotification(NOTIF_MINER, msg);
                    }
                    break;
                }
            }
        }
        if (next.totalBadShares > prev.totalBadShares) {
            triggerNotification(NOTIF_MINER, "Miner stale/invalid share");
        }
        if (next.reachedPct >= 100.0f && prev.reachedPct < 100.0f) {
            triggerNotification(NOTIF_MINER, "Payout threshold reached!");
        }
    }
}

static int compareSemVerStr(const String& a, const String& b) {
    int pa[3] = {0, 0, 0}, pb[3] = {0, 0, 0};
    auto parse = [](const String& s, int out[3]) {
        int idx = 0, cur = 0;
        for (size_t i = 0; i < s.length() && idx < 3; ++i) {
            char c = s[i];
            if (c >= '0' && c <= '9') cur = cur * 10 + (c - '0');
            else if (c == '.') { out[idx++] = cur; cur = 0; }
        }
        if (idx < 3) out[idx] = cur;
    };
    parse(a, pa); parse(b, pb);
    for (int i = 0; i < 3; ++i) {
        if (pa[i] != pb[i]) return (pa[i] > pb[i]) ? 1 : -1;
    }
    return 0;
}

static void checkSoftwareAndAppUpdates() {
    if (WiFi.status() != WL_CONNECTED || ESP.getFreeHeap() < 75000) return;

    String url = String(g_settings.storeUrl);
    int p = url.indexOf("/store/");
    String base = (p > 8) ? url.substring(0, p) : "https://raw.githubusercontent.com/Codepmalex/CardOS/main";

    bool osNew = false;
    String osVer;
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(8000);
        if (http.begin(client, base + "/store/os_version.json")) {
            if (http.GET() == 200) {
                JsonDocument doc;
                if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
                    osVer = String(doc["version"] | "");
                    if (!osVer.isEmpty()) {
                        strlcpy(g_sys.latestOsVer, osVer.c_str(), sizeof(g_sys.latestOsVer));
                        if (compareSemVerStr(osVer, CARDOS_VERSION) > 0) osNew = true;
                    }
                }
            }
            http.end();
        }
    }

    int appUpdCount = 0;
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(9000);
        if (http.begin(client, g_settings.storeUrl)) {
            if (http.GET() == 200) {
                JsonDocument doc;
                if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
                    for (JsonObject a : doc["apps"].as<JsonArray>()) {
                        String id = String(a["id"] | "");
                        String rVer = String(a["version"] | "");
                        if (id.isEmpty() || rVer.isEmpty()) continue;
                        if (!isStoreAppInstalled(id) && id != "music") continue;
                        String lVer;
                        String pkg = "/store/" + id + ".json";
                        if ((g_sys.sdMounted || g_sys.spiffsMounted) && activeFS().exists(pkg.c_str())) {
                            JsonDocument ld;
                            if (deserializeJson(ld, readTextFile(pkg.c_str(), 2048)) == DeserializationError::Ok) {
                                lVer = String(ld["version"] | "");
                            }
                        } else if (id == "music") {
                            lVer = CARDOS_VERSION;
                        }
                        if (lVer.isEmpty() || compareSemVerStr(rVer, lVer) > 0) appUpdCount++;
                    }
                }
            }
            http.end();
        }
    }
    g_sys.pendingAppUpdates = appUpdCount;

    if (osNew && appUpdCount > 0) {
        triggerNotification(NOTIF_UPDATES, ("OS v" + osVer + " & " + String(appUpdCount) + " app upd!").c_str());
    } else if (osNew) {
        triggerNotification(NOTIF_UPDATES, ("CardOS v" + osVer + " available!").c_str());
    } else if (appUpdCount > 0) {
        triggerNotification(NOTIF_UPDATES, (String(appUpdCount) + " app update(s) ready!").c_str());
    }
}

static void netWorkerTask(void* param) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (g_settings.wifiAutoConnect && strlen(g_settings.wifiSsid) > 0) {
        s_reqWifiConnect = true;
    }

    uint32_t connectStartMs = 0;
    uint32_t lastNtpCheckMs = 0;
    uint32_t lastMinerPollMs = 0;
    uint32_t lastRoamScanMs = 0;
    uint32_t lastUpdateCheckMs = 0;
    bool trySavedRoam = false;

    while (true) {
        if (g_sys.deepSleepMode) {
            delay(250);
            continue;
        }
        uint32_t now = millis();

        if (s_reqWifiScan) {
            s_reqWifiScan = false;
            s_wifiScanning = true;
            if (WiFi.status() != WL_CONNECTED) {
                g_sys.wifiConnecting = false;
            }
            WiFi.mode(WIFI_STA);
            auto res = scanWifiNetworks();
            if (xSemaphoreTake(g_sysMutex, portMAX_DELAY)) {
                s_scanResults = std::move(res);
                xSemaphoreGive(g_sysMutex);
            }
            s_wifiScanSeq++;
            s_wifiScanning = false;
        }

        if (s_reqWifiConnect) {
            s_reqWifiConnect = false;
            if (strlen(g_settings.wifiSsid) > 0) {
                g_sys.wifiConnecting = true;
                g_sys.wifiConnected = false;
                strlcpy(g_sys.wifiStatus, "Connecting...", sizeof(g_sys.wifiStatus));
                WiFi.setAutoReconnect(true);
                WiFi.disconnect(false);
                WiFi.begin(g_settings.wifiSsid, g_settings.wifiPass);
                connectStartMs = now;
            }
        }

        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            bool justConnected = !g_sys.wifiConnected;
            g_sys.wifiConnected = true;
            g_sys.wifiConnecting = false;
            strlcpy(g_sys.wifiStatus, WiFi.SSID().c_str(), sizeof(g_sys.wifiStatus));
            strlcpy(g_sys.wifiIp, WiFi.localIP().toString().c_str(), sizeof(g_sys.wifiIp));
            g_sys.wifiRssi = WiFi.RSSI();

            if (justConnected) {
                applyTimezoneConfig();
                s_reqWeatherRefresh = true;
                s_reqUpdateCheck = true;
                triggerNotification(NOTIF_SYSTEM, "Wi-Fi Connected");
            }
        } else {
            g_sys.wifiConnected = false;
            if (g_sys.wifiConnecting) {
                if (now - connectStartMs > 10000) {
                    g_sys.wifiConnecting = false;
                    WiFi.setAutoReconnect(false);
                    WiFi.disconnect(false, false);
                    strlcpy(g_sys.wifiStatus, "Connect Failed", sizeof(g_sys.wifiStatus));
                    trySavedRoam = true;
                }
            } else {
                strlcpy(g_sys.wifiStatus, strlen(g_settings.wifiSsid) ? "Disconnected" : "No Wi-Fi Set", sizeof(g_sys.wifiStatus));
                if (g_settings.wifiAutoConnect && (trySavedRoam || now - lastRoamScanMs >= 45000UL)) {
                    trySavedRoam = false;
                    lastRoamScanMs = now;
                    bool hasAnySaved = false;
                    for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
                        if (strlen(g_settings.savedWifis[i].ssid) > 0) { hasAnySaved = true; break; }
                    }
                    if (hasAnySaved) {
                        auto scanned = scanWifiNetworks();
                        for (const auto& net : scanned) {
                            const char* sp = findSavedWifiPass(net.ssid.c_str());
                            if (sp != nullptr) {
                                strlcpy(g_settings.wifiSsid, net.ssid.c_str(), sizeof(g_settings.wifiSsid));
                                strlcpy(g_settings.wifiPass, sp, sizeof(g_settings.wifiPass));
                                s_reqWifiConnect = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Check NTP sync
        if (now - lastNtpCheckMs > 2000) {
            lastNtpCheckMs = now;
            struct tm tmInfo;
            if (getSafeLocalTime(tmInfo)) {
                bool firstNtp = !g_sys.ntpSynced;
                g_sys.ntpSynced = true;
                if (firstNtp) saveWeatherAndTimeCache();
            }
        }

        // Auto-refresh weather (pause background TLS while Lua / Store / Phone / Recorder active)
        bool busyTls = (g_sys.activeApp == APP_LUA_RUNNER || g_sys.activeApp == APP_STORE ||
                        g_sys.activeApp == APP_PHONE || g_sys.activeApp == APP_RECORDER);
        uint32_t mult = g_settings.lowPowerMode ? 2UL : 1UL;
        uint32_t refreshMs = max((uint32_t)5, (uint32_t)g_settings.weatherRefreshMin) * 60000UL * mult;
        bool due = g_sys.wifiConnected && !busyTls &&
                   (isStoreAppInstalled("weather") || g_settings.homeWidget == WIDGET_WEATHER) &&
                   (!g_sys.weather.valid || g_sys.weather.lastFetchMs == 0 || (now - g_sys.weather.lastFetchMs > refreshMs));

        if (g_sys.wifiConnected && !busyTls && (s_reqWeatherRefresh || due)) {
            s_reqWeatherRefresh = false;
            fetchOpenMeteoWeather();
        }

        // Poll Kryptex Pool API (60s normal, 120s in Low Battery mode; skip when miner isn't installed or Lua/Store active)
        uint32_t minerInt = 60000UL * mult;
        if (g_sys.wifiConnected && !busyTls && isStoreAppInstalled("miner") && strlen(g_settings.minerWallet) > 0 &&
            (s_reqMinerRefresh || lastMinerPollMs == 0 || (now - lastMinerPollMs >= minerInt))) {
            s_reqMinerRefresh = false;
            lastMinerPollMs = now;
            pollKryptexPool();
        }

        // Check GitHub OS & Store updates on connect/boot and every 24h
        if (g_sys.wifiConnected && !busyTls &&
            (s_reqUpdateCheck || (lastUpdateCheckMs > 0 && (now - lastUpdateCheckMs >= 86400000UL)))) {
            s_reqUpdateCheck = false;
            lastUpdateCheckMs = now;
            checkSoftwareAndAppUpdates();
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void initNetWeather() {
    if (!s_netTaskHandle) {
        xTaskCreatePinnedToCore(netWorkerTask, "NetWeather", 10240, nullptr, 1, &s_netTaskHandle, 0);
    }
}

void requestWifiConnect() {
    s_reqWifiConnect = true;
}

void requestWifiDisconnect() {
    WiFi.disconnect(true);
    g_sys.wifiConnected = false;
    g_sys.wifiConnecting = false;
    strlcpy(g_sys.wifiStatus, "Disconnected", sizeof(g_sys.wifiStatus));
}

void requestWeatherRefresh() {
    s_reqWeatherRefresh = true;
}

void requestMinerRefresh() {
    s_reqMinerRefresh = true;
}

std::vector<WifiNetInfo> scanWifiNetworks() {
    std::vector<WifiNetInfo> out;
    WiFi.mode(WIFI_STA);
    bool wasConnected = (WiFi.status() == WL_CONNECTED);
    if (!wasConnected) {
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        delay(120);
    }
    WiFi.scanDelete();
    int n = WiFi.scanNetworks(false, false);
    if (n < 0) {
        WiFi.disconnect(false, false);
        delay(150);
        n = WiFi.scanNetworks(false, false);
    }
    for (int i = 0; i < n; ++i) {
        String s = WiFi.SSID(i);
        if (s.isEmpty()) continue;
        bool dup = false;
        for (auto& ex : out) {
            if (ex.ssid == s) { dup = true; break; }
        }
        if (!dup) {
            WifiNetInfo info;
            info.ssid = s;
            info.rssi = WiFi.RSSI(i);
            info.open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
            out.push_back(info);
        }
    }
    WiFi.scanDelete();
    std::sort(out.begin(), out.end(), [](const WifiNetInfo& a, const WifiNetInfo& b) {
        return a.rssi > b.rssi;
    });
    return out;
}

void requestWifiScan() {
    s_wifiScanning = true;
    s_reqWifiScan = true;
}

bool isWifiScanInProgress() {
    return s_wifiScanning || s_reqWifiScan;
}

uint32_t getWifiScanSeq() {
    return s_wifiScanSeq;
}

std::vector<WifiNetInfo> getWifiScanResults() {
    std::vector<WifiNetInfo> copy;
    if (g_sysMutex && xSemaphoreTake(g_sysMutex, pdMS_TO_TICKS(50))) {
        copy = s_scanResults;
        xSemaphoreGive(g_sysMutex);
    }
    return copy;
}

const char* weatherCodeToShort(int code) {
    if (code == 0) return "Clear";
    if (code <= 3) return "Cloudy";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 57) return "Drizzle";
    if (code >= 61 && code <= 67) return "Rain";
    if (code >= 71 && code <= 77) return "Snow";
    if (code >= 80 && code <= 82) return "Showers";
    if (code >= 85 && code <= 86) return "SnowShw";
    if (code >= 95) return "Storm";
    return "Clear";
}

const char* weatherCodeToLong(int code) {
    if (code == 0) return "Clear Sky";
    if (code == 1) return "Mainly Clear";
    if (code == 2) return "Partly Cloudy";
    if (code == 3) return "Overcast";
    if (code == 45 || code == 48) return "Foggy";
    if (code >= 51 && code <= 55) return "Light Drizzle";
    if (code >= 61 && code <= 65) return "Rainy";
    if (code >= 71 && code <= 77) return "Snowfall";
    if (code >= 80 && code <= 82) return "Rain Showers";
    if (code >= 95) return "Thunderstorm";
    return "Unknown";
}
