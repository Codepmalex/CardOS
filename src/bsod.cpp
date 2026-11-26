#include "bsod.h"
#include "config.h"
#include "ui.h"
#include <esp_system.h>
#include <string.h>

namespace {

struct CrashBreadcrumb {
    uint32_t magic;
    char lastApp[20];
    uint32_t lastFreeHeapKB;
    uint32_t lastUptimeSec;
    uint32_t checksum;
};

RTC_NOINIT_ATTR static CrashBreadcrumb s_crumb;
static constexpr uint32_t kCrumbMagic = 0xCA4D05BB;

uint32_t calcChecksum(const CrashBreadcrumb& c) {
    uint32_t h = c.magic ^ c.lastFreeHeapKB ^ (c.lastUptimeSec << 3);
    for (size_t i = 0; i < sizeof(c.lastApp); ++i) {
        h = (h * 33U) ^ (uint8_t)c.lastApp[i];
    }
    return h;
}

const char* resetReasonStopCode(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_PANIC:    return "KERNEL_GURU_MEDITATION";
        case ESP_RST_INT_WDT:  return "INTERRUPT_WATCHDOG_TIMEOUT";
        case ESP_RST_TASK_WDT: return "TASK_WATCHDOG_BARKED";
        case ESP_RST_WDT:      return "SYSTEM_WATCHDOG_RESET";
        case ESP_RST_BROWNOUT: return "LOW_POWER_BROWNOUT";
        default:               return nullptr;
    }
}

} // namespace

void bsodRecordHeartbeat(const char* appName) {
    s_crumb.magic = kCrumbMagic;
    strlcpy(s_crumb.lastApp, appName ? appName : "CardOS", sizeof(s_crumb.lastApp));
    s_crumb.lastFreeHeapKB = (uint32_t)(ESP.getFreeHeap() / 1024);
    s_crumb.lastUptimeSec = (uint32_t)(millis() / 1000);
    s_crumb.checksum = calcChecksum(s_crumb);
}

void showBsodScreen(M5Canvas& spr, const char* stopCode, const char* detail, bool waitKey) {
    uint16_t winBlue = rgb565(0, 120, 215);
    spr.fillScreen(winBlue);
    spr.setTextDatum(TL_DATUM);

    spr.setTextSize(3);
    spr.setTextColor(TFT_WHITE);
    spr.drawString(":(", 12, 8);

    spr.setTextSize(1);
    spr.setTextColor(TFT_WHITE);
    ui::drawWrappedText(spr,
        "CardOS ran into a tiny problem and tripped over a wire. Let's shake it off!",
        54, 10, SCREEN_W - 62, 34, 10);

    spr.setTextColor(rgb565(224, 242, 254));
    String info = String("STOP: ") + (stopCode ? stopCode : "UNKNOWN_OOPS");
    if (detail && detail[0]) {
        info += "\n" + String(detail);
    }
    ui::drawWrappedText(spr, info, 12, 52, SCREEN_W - 24, 58, 10);

    spr.fillRect(12, 114, SCREEN_W - 24, 14, rgb565(0, 90, 165));
    spr.setTextColor(TFT_WHITE);
    spr.drawString("100% confused  |  Press any key / G0", 16, 117);
    spr.pushSprite(0, 0);

    if (!waitKey) return;
    delay(250);
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
        if (M5Cardputer.BtnA.wasPressed()) break;
        delay(20);
    }
    delay(120);
}

void bsodCheckOnBoot(M5Canvas& spr) {
    esp_reset_reason_t reason = esp_reset_reason();
    const char* stopCode = resetReasonStopCode(reason);
    if (!stopCode) return;

    char detail[128];
    if (s_crumb.magic == kCrumbMagic && s_crumb.checksum == calcChecksum(s_crumb)) {
        snprintf(detail, sizeof(detail),
                 "Last App: %s\nFree RAM: %u KB | Uptime: %us\nVer: CardOS v%s",
                 s_crumb.lastApp,
                 (unsigned)s_crumb.lastFreeHeapKB,
                 (unsigned)s_crumb.lastUptimeSec,
                 CARDOS_VERSION);
    } else {
        snprintf(detail, sizeof(detail),
                 "Early boot fault (reason=%d)\nVer: CardOS v%s",
                 (int)reason, CARDOS_VERSION);
    }

    showBsodScreen(spr, stopCode, detail, true);
}

