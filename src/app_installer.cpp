#include "app_installer.h"
#include "state.h"
#include "storage.h"
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_sleep.h>
#include <esp_system.h>

static constexpr uint8_t  ESP_MAGIC = 0xE9;
static constexpr uint8_t  ESP_MAX_SEGS = 16;
static constexpr uint32_t ESP_HASH_LEN = 32;
static constexpr uint32_t PART_TABLE_OFF = 0x8000;
static constexpr uint32_t PART_TABLE_SZ  = 0x1000;
static constexpr uint32_t PART_ENTRY_SZ  = 0x20;

static uint32_t alignUp(uint32_t v, uint32_t a) {
    return ((v + a - 1) / a) * a;
}

static uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t measureEspImageSize(File& f, uint32_t imageOffset, uint32_t fileSize) {
    if (imageOffset + 24 > fileSize) return 0;
    if (!f.seek(imageOffset)) return 0;

    uint8_t hdr[24];
    if (f.read(hdr, 24) != 24) return 0;

    uint8_t magic = hdr[0];
    uint8_t segCount = hdr[1];
    uint8_t hashAppended = hdr[23];
    if (magic != ESP_MAGIC || segCount == 0 || segCount > ESP_MAX_SEGS) return 0;

    uint32_t cursor = imageOffset + 24;
    for (uint8_t i = 0; i < segCount; ++i) {
        if (cursor + 8 > fileSize) return 0;
        if (!f.seek(cursor)) return 0;
        uint8_t shdr[8];
        if (f.read(shdr, 8) != 8) return 0;
        uint32_t segSize = readU32LE(shdr + 4);
        cursor += 8;
        if (segSize > fileSize || cursor > fileSize - segSize) return 0;
        cursor += segSize;
    }

    uint32_t end = alignUp(cursor, 16) + 1;
    if (hashAppended) end += ESP_HASH_LEN;
    end = alignUp(end, 16);
    if (end <= imageOffset || end > fileSize) return 0;
    return end - imageOffset;
}

void ensureBootToCardOS() {
    const esp_partition_t* app0 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, (esp_partition_subtype_t)0x20, nullptr); // test partition
    if (!app0) {
        app0 = esp_ota_get_running_partition();
    }
    if (app0) {
        const esp_partition_t* bootPart = esp_ota_get_boot_partition();
        if (bootPart != app0) {
            esp_ota_set_boot_partition(app0);
        }
    }
}

BinImageInfo inspectBinFile(const char* path) {
    BinImageInfo info;
    if (!g_sys.sdMounted) {
        info.error = "SD card not mounted";
        return info;
    }
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        info.error = "Cannot open .bin file";
        return info;
    }
    info.fileSize = (uint32_t)f.size();
    if (info.fileSize < 4096) {
        f.close();
        info.error = "File too small";
        return info;
    }

    // Check if merged image (partition table magic 0xAA 0x50 0x01 at 0x8000)
    bool merged = false;
    if (info.fileSize >= PART_TABLE_OFF + PART_ENTRY_SZ) {
        f.seek(PART_TABLE_OFF);
        uint8_t magic3[3] = {0};
        if (f.read(magic3, 3) == 3 && magic3[0] == 0xAA && magic3[1] == 0x50 && magic3[2] == 0x01) {
            merged = true;
        }
    }

    if (merged) {
        info.isMerged = true;
        uint32_t appOffset = 0;
        uint32_t appSize = 0;

        for (uint32_t i = 0; i < PART_TABLE_SZ; i += PART_ENTRY_SZ) {
            f.seek(PART_TABLE_OFF + i);
            uint8_t entry[PART_ENTRY_SZ];
            if (f.read(entry, PART_ENTRY_SZ) != PART_ENTRY_SZ) break;
            if ((entry[0] == 0xEB && entry[1] == 0xEB) || (entry[0] == 0xFF && entry[1] == 0xFF)) break;
            uint8_t ptype = entry[2];
            uint8_t subtype = entry[3];
            uint32_t off = readU32LE(entry + 4);
            uint32_t declSz = readU32LE(entry + 8);
            // App partition: factory(0x00), ota_0(0x10), test(0x20)
            if (ptype == 0x00 && (subtype == 0x00 || subtype == 0x10 || subtype == 0x20)) {
                appOffset = off ? off : 0x10000;
                uint32_t measured = measureEspImageSize(f, appOffset, info.fileSize);
                appSize = declSz;
                if (info.fileSize < appOffset + appSize) {
                    appSize = info.fileSize - appOffset;
                }
                if (measured > 0 && (appSize == 0 || measured < appSize)) {
                    appSize = measured;
                }
                break;
            }
        }
        if (appOffset == 0 || appSize == 0) {
            f.close();
            info.error = "No app in merged bin";
            return info;
        }
        info.appOffset = appOffset;
        info.appSize = appSize;
    } else {
        // Bare app image: check magic at offset 0
        f.seek(0);
        uint8_t firstByte = 0;
        f.read(&firstByte, 1);
        if (firstByte != ESP_MAGIC) {
            f.close();
            info.error = "Invalid ESP32 .bin";
            return info;
        }
        uint32_t measured = measureEspImageSize(f, 0, info.fileSize);
        info.appOffset = 0;
        info.appSize = measured ? measured : info.fileSize;
    }

    f.close();

    // Check against app1 partition capacity
    const esp_partition_t* app1 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (!app1) {
        info.error = "app1 partition missing";
        return info;
    }
    if (info.appSize > app1->size) {
        info.error = "Too large for app1 (" + formatBytes(app1->size) + ")";
        return info;
    }

    info.valid = true;
    return info;
}

bool isBinAlreadyInstalled(const char* path, uint32_t fileSize) {
    if (strlen(g_settings.installedBinName) == 0) return false;
    return strcmp(g_settings.installedBinName, path) == 0 && g_settings.installedBinSize == fileSize;
}

bool installBinToApp1(const char* path, FlashProgressCb cb, String& errOut) {
    BinImageInfo info = inspectBinFile(path);
    if (!info.valid) {
        errOut = info.error;
        return false;
    }

    const esp_partition_t* app1 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (!app1) {
        errOut = "app1 partition not found";
        return false;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
        errOut = "Failed to reopen .bin";
        return false;
    }

    uint32_t eraseSize = alignUp(info.appSize, 4096);
    if (eraseSize > app1->size) eraseSize = app1->size;

    // Erase in 64KB blocks so we can update UI progress smoothly
    if (cb) cb(0, info.appSize, "Erasing flash...");
    uint32_t erased = 0;
    while (erased < eraseSize) {
        uint32_t chunk = min((uint32_t)65536, eraseSize - erased);
        // Align chunk to 4096
        chunk = alignUp(chunk, 4096);
        esp_err_t err = esp_partition_erase_range(app1, erased, chunk);
        if (err != ESP_OK) {
            f.close();
            errOut = "Erase failed: " + String((int)err);
            return false;
        }
        erased += chunk;
        if (cb) cb(erased / 8, info.appSize, "Erasing flash...");
    }

    uint8_t* buf = (uint8_t*)malloc(4096);
    if (!buf) {
        f.close();
        errOut = "Out of memory";
        return false;
    }

    f.seek(info.appOffset);
    uint32_t written = 0;
    while (written < info.appSize) {
        uint32_t toRead = min((uint32_t)4096, info.appSize - written);
        size_t got = f.read(buf, toRead);
        if (got == 0) break;

        // Pad tail to 4-byte alignment if needed
        uint32_t writeLen = alignUp((uint32_t)got, 4);
        if (writeLen > got) {
            memset(buf + got, 0xFF, writeLen - got);
        }

        esp_err_t err = esp_partition_write(app1, written, buf, writeLen);
        if (err != ESP_OK) {
            free(buf);
            f.close();
            errOut = "Write failed: " + String((int)err);
            return false;
        }
        written += got;
        if (cb && ((written & 0x3FFF) == 0 || written >= info.appSize)) {
            cb(written, info.appSize, "Flashing app...");
        }
    }

    free(buf);
    f.close();

    if (written < info.appSize) {
        errOut = "Short read from SD";
        return false;
    }

    strlcpy(g_settings.installedBinName, path, sizeof(g_settings.installedBinName));
    g_settings.installedBinSize = info.fileSize;
    saveSettings();
    return true;
}

bool launchInstalledApp1(String& errOut) {
    const esp_partition_t* app1 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (!app1) {
        errOut = "app1 partition not found";
        return false;
    }
    esp_err_t err = esp_ota_set_boot_partition(app1);
    if (err != ESP_OK) {
        errOut = "Boot set failed: " + String((int)err);
        return false;
    }
    delay(80);
    esp_restart();
    return true;
}

void rebootToCardOS() {
    esp_sleep_enable_timer_wakeup(50000); // 50ms -> DEEPSLEEP_RESET (5), which bootloader_cardos.bin boots into app0 (CardOS)
    esp_deep_sleep_start();
}

#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include "config.h"
#include "storage.h"
#include "state.h"

namespace {

static const char* kOtaBinPath = "/cardos_ota.bin";

void drawOtaModal(const char* title, const char* subtitle, uint32_t done, uint32_t total) {
    auto& d = M5Cardputer.Display;
    int mx = 16, my = 24, mw = SCREEN_W - 32, mh = SCREEN_H - 48;
    d.fillRoundRect(mx, my, mw, mh, 6, 0x0841);
    d.drawRoundRect(mx, my, mw, mh, 6, 0x3DFB);
    d.setTextSize(1);
    d.setTextDatum(TC_DATUM);
    d.setTextColor(0xFFFF, 0x0841);
    d.drawString(title, SCREEN_W / 2, my + 8);
    d.setTextColor(0xBDF7, 0x0841);
    d.drawString(subtitle, SCREEN_W / 2, my + 24);

    int bx = mx + 12, by = my + 44, bw = mw - 24, bh = 12;
    d.drawRoundRect(bx, by, bw, bh, 3, 0x7BEF);
    if (total > 0) {
        uint32_t pct = (done >= total) ? 100 : (uint32_t)((uint64_t)done * 100ULL / total);
        int fw = (int)((uint64_t)(bw - 4) * pct / 100ULL);
        if (fw > 0) d.fillRoundRect(bx + 2, by + 2, fw, bh - 4, 2, 0x2EEB);
        char pbuf[32];
        snprintf(pbuf, sizeof(pbuf), "%u%% (%u/%uKB)", (unsigned)pct, (unsigned)(done / 1024), (unsigned)(total / 1024));
        d.setTextColor(0x9E7F, 0x0841);
        d.drawString(pbuf, SCREEN_W / 2, by + 18);
    }
    d.setTextDatum(TL_DATUM);
}

int compareSemVer(const String& a, const String& b) {
    int pa[3] = {0, 0, 0}, pb[3] = {0, 0, 0};
    auto parse = [](const String& s, int out[3]) {
        int idx = 0, cur = 0;
        for (size_t i = 0; i < s.length() && idx < 3; ++i) {
            char c = s[i];
            if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
            } else if (c == '.') {
                out[idx++] = cur;
                cur = 0;
            }
        }
        if (idx < 3) out[idx] = cur;
    };
    parse(a, pa);
    parse(b, pb);
    for (int i = 0; i < 3; ++i) {
        if (pa[i] != pb[i]) return (pa[i] > pb[i]) ? 1 : -1;
    }
    return 0;
}

String deriveRepoBaseUrl() {
    String url = String(g_settings.storeUrl);
    int p = url.indexOf("/store/");
    if (p > 8) return url.substring(0, p);
    return "https://raw.githubusercontent.com/Codepmalex/CardOS/main";
}

bool waitOtaKeyChoice(bool allowForceR) {
    delay(150);
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto st = M5Cardputer.Keyboard.keysState();
            if (st.enter) return true;
            for (char c : st.word) {
                if (c == 'y' || c == 'Y') return true;
                if (allowForceR && (c == 'r' || c == 'R')) return true;
                if (c == 'n' || c == 'N' || c == 27 || c == '`') return false;
            }
            if (st.del) return false;
        }
        if (M5Cardputer.BtnA.wasPressed()) return false;
        delay(20);
    }
}

} // namespace

bool checkAndApplyPendingOsUpdate() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* app0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app0");
    if (!app0) app0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
    if (!running || !app0) return false;
    if (running->address == app0->address) return false; // Already running app0

    fs::FS& fs = activeFS();
    if (!fs.exists(kOtaBinPath)) return false;

    File f = fs.open(kOtaBinPath, FILE_READ);
    if (!f) {
        esp_ota_set_boot_partition(app0);
        return false;
    }
    uint32_t total = (uint32_t)f.size();
    uint8_t magic = 0;
    if (total < 100000 || total > app0->size || f.read(&magic, 1) != 1 || magic != 0xE9) {
        f.close();
        fs.remove(kOtaBinPath);
        esp_ota_set_boot_partition(app0);
        return false;
    }
    f.seek(0);

    drawOtaModal("CardOS Updater 2/2", "Erasing app0...", 0, total);
    uint32_t eraseSize = (total + SPI_FLASH_SEC_SIZE - 1) & ~(uint32_t)(SPI_FLASH_SEC_SIZE - 1);
    if (eraseSize > app0->size) eraseSize = app0->size;
    if (esp_partition_erase_range(app0, 0, eraseSize) != ESP_OK) {
        f.close();
        fs.remove(kOtaBinPath);
        esp_ota_set_boot_partition(app0);
        return false;
    }

    static uint8_t buf[4096];
    uint32_t offset = 0;
    uint32_t lastUi = 0;
    bool ok = true;
    while (offset < total) {
        size_t chunk = total - offset;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);
        int got = f.read(buf, chunk);
        if (got <= 0) { ok = false; break; }
        if (esp_partition_write(app0, offset, buf, (size_t)got) != ESP_OK) { ok = false; break; }
        offset += (uint32_t)got;
        if (offset - lastUi >= 32768 || offset == total) {
            lastUi = offset;
            drawOtaModal("CardOS Updater 2/2", "Flashing CardOS...", offset, total);
        }
    }
    f.close();
    fs.remove(kOtaBinPath);
    esp_ota_set_boot_partition(app0);

    drawOtaModal(ok ? "Update Complete!" : "Update Failed", ok ? "Rebooting to CardOS..." : "Restoring app0...", total, total);
    delay(900);
    rebootToCardOS();
    return ok;
}

bool runGithubOsUpdateInteractive() {
    if (WiFi.status() != WL_CONNECTED) {
        drawOtaModal("Software Update", "Connect Wi-Fi first!", 0, 0);
        delay(1600);
        return false;
    }
    if (!g_sys.sdMounted) {
        drawOtaModal("Software Update", "SD card required for OTA", 0, 0);
        delay(1800);
        return false;
    }

    String baseUrl = deriveRepoBaseUrl();
    String verUrl = baseUrl + "/store/os_version.json";
    drawOtaModal("Software Update", "Checking GitHub...", 0, 0);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, verUrl)) {
        drawOtaModal("Software Update", "HTTP init failed", 0, 0);
        delay(1500);
        return false;
    }
    int code = http.GET();
    if (code != 200) {
        http.end();
        char eb[32];
        snprintf(eb, sizeof(eb), "Check failed (%d)", code);
        drawOtaModal("Software Update", eb, 0, 0);
        delay(1600);
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        drawOtaModal("Software Update", "Invalid version JSON", 0, 0);
        delay(1600);
        return false;
    }
    String remoteVer = doc["version"] | "";
    String notes = doc["notes"] | "";
    String fallbackBin = baseUrl + "/firmware/cardos_latest.bin";
    const char* binC = doc["bin_url"] | fallbackBin.c_str();
    String binUrl = String(binC);
    if (remoteVer.length() == 0) {
        drawOtaModal("Software Update", "Missing version info", 0, 0);
        delay(1500);
        return false;
    }

    bool newer = compareSemVer(remoteVer, CARDOS_VERSION) > 0;
    auto& d = M5Cardputer.Display;
    int mx = 12, my = 18, mw = SCREEN_W - 24, mh = SCREEN_H - 36;
    d.fillRoundRect(mx, my, mw, mh, 6, 0x0841);
    d.drawRoundRect(mx, my, mw, mh, 6, 0x3DFB);
    d.setTextSize(1);
    d.setTextDatum(TC_DATUM);
    d.setTextColor(newer ? 0x2EEB : 0xFFFF, 0x0841);
    String hdr = newer ? ("New CardOS v" + remoteVer) : ("Up to date (v" + String(CARDOS_VERSION) + ")");
    d.drawString(hdr, SCREEN_W / 2, my + 8);
    d.setTextColor(0xBDF7, 0x0841);
    String cur = "Current: v" + String(CARDOS_VERSION) + "  Latest: v" + remoteVer;
    d.drawString(cur, SCREEN_W / 2, my + 24);
    if (notes.length() > 32) notes = notes.substring(0, 29) + "...";
    if (notes.length() > 0) {
        d.setTextColor(0x9E7F, 0x0841);
        d.drawString(notes, SCREEN_W / 2, my + 39);
    }
    d.setTextColor(0xFD20, 0x0841);
    d.drawString("Uses App1 slot temporarily", SCREEN_W / 2, my + 56);
    d.setTextColor(0xFFFF, 0x0841);
    d.drawString(newer ? "[Ent] Install   [Esc] Cancel" : "[R] Reinstall   [Esc] Back", SCREEN_W / 2, my + 74);
    d.setTextDatum(TL_DATUM);

    if (!waitOtaKeyChoice(!newer)) return false;

    SD.remove(kOtaBinPath);
    File out = SD.open(kOtaBinPath, FILE_WRITE);
    if (!out) {
        drawOtaModal("Software Update", "Cannot write SD file", 0, 0);
        delay(1600);
        return false;
    }

    drawOtaModal("CardOS Updater 1/2", "Downloading firmware...", 0, 0);
    WiFiClientSecure dlClient;
    dlClient.setInsecure();
    HTTPClient dlHttp;
    dlHttp.setTimeout(15000);
    dlHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!dlHttp.begin(dlClient, binUrl) || dlHttp.GET() != 200) {
        dlHttp.end();
        out.close();
        SD.remove(kOtaBinPath);
        drawOtaModal("Software Update", "Download failed!", 0, 0);
        delay(1700);
        return false;
    }

    int contentLen = dlHttp.getSize();
    WiFiClient* stream = dlHttp.getStreamPtr();
    static uint8_t dlBuf[2048];
    uint32_t downloaded = 0;
    uint32_t lastUi = 0;
    uint32_t lastDataMs = millis();
    uint8_t firstByte = 0;
    while (dlHttp.connected() && (contentLen <= 0 || (int)downloaded < contentLen)) {
        size_t avail = stream->available();
        if (avail > 0) {
            if (avail > sizeof(dlBuf)) avail = sizeof(dlBuf);
            int rd = stream->readBytes(dlBuf, avail);
            if (rd > 0) {
                if (downloaded == 0) firstByte = dlBuf[0];
                out.write(dlBuf, (size_t)rd);
                downloaded += (uint32_t)rd;
                lastDataMs = millis();
                if (downloaded - lastUi >= 24576) {
                    lastUi = downloaded;
                    drawOtaModal("CardOS Updater 1/2", "Downloading firmware...", downloaded, contentLen > 0 ? (uint32_t)contentLen : downloaded + 65536);
                }
            }
        } else {
            if (millis() - lastDataMs > 10000) break;
            delay(5);
        }
    }
    dlHttp.end();
    out.close();

    if (downloaded < 200000 || firstByte != 0xE9 || (contentLen > 0 && downloaded < (uint32_t)contentLen)) {
        SD.remove(kOtaBinPath);
        drawOtaModal("Software Update", "Incomplete or bad binary", 0, 0);
        delay(1800);
        return false;
    }

    const esp_partition_t* app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app1");
    if (!app1) app1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (!app1 || downloaded > app1->size) {
        SD.remove(kOtaBinPath);
        drawOtaModal("Software Update", "App1 slot unavailable", 0, 0);
        delay(1800);
        return false;
    }

    File in = SD.open(kOtaBinPath, FILE_READ);
    if (!in) {
        SD.remove(kOtaBinPath);
        return false;
    }

    esp_ota_handle_t otaHandle = 0;
    drawOtaModal("CardOS Updater 1/2", "Staging to App1...", 0, downloaded);
    if (esp_ota_begin(app1, downloaded, &otaHandle) != ESP_OK) {
        in.close();
        SD.remove(kOtaBinPath);
        drawOtaModal("Software Update", "OTA begin failed", 0, 0);
        delay(1800);
        return false;
    }

    uint32_t written = 0;
    lastUi = 0;
    bool writeOk = true;
    while (written < downloaded) {
        size_t chunk = downloaded - written;
        if (chunk > sizeof(dlBuf)) chunk = sizeof(dlBuf);
        int rd = in.read(dlBuf, chunk);
        if (rd <= 0) { writeOk = false; break; }
        if (esp_ota_write(otaHandle, dlBuf, (size_t)rd) != ESP_OK) { writeOk = false; break; }
        written += (uint32_t)rd;
        if (written - lastUi >= 32768 || written == downloaded) {
            lastUi = written;
            drawOtaModal("CardOS Updater 1/2", "Staging to App1...", written, downloaded);
        }
    }
    in.close();

    if (!writeOk || esp_ota_end(otaHandle) != ESP_OK || esp_ota_set_boot_partition(app1) != ESP_OK) {
        SD.remove(kOtaBinPath);
        drawOtaModal("Software Update", "OTA verify failed", 0, 0);
        delay(1800);
        return false;
    }

    g_settings.installedBinName[0] = '\0';
    g_settings.installedBinSize = 0;
    saveSettings();

    drawOtaModal("CardOS Updater 1/2", "Rebooting to Stage 2...", downloaded, downloaded);
    delay(900);
    ESP.restart();
    return true;
}

