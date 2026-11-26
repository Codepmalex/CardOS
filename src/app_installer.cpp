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

