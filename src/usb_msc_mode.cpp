#include "usb_msc_mode.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "input.h"
#include "hardware.h"
#include "serial_ctl.h"
#include "ui.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <USB.h>
#include <USBMSC.h>
#include "tusb.h"

extern M5Canvas* getScreenCanvas();

static USBMSC s_msc;
static volatile uint32_t s_readSectors = 0;
static volatile uint32_t s_writeSectors = 0;
static volatile bool s_hostMounted = false;

static int32_t onMscWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void)offset;
    uint32_t count = bufsize / 512;
    for (uint32_t i = 0; i < count; ++i) {
        if (!SD.writeRAW(buffer + (i * 512), lba + i)) {
            return -1;
        }
    }
    s_writeSectors += count;
    return (int32_t)bufsize;
}

static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void)offset;
    uint32_t count = bufsize / 512;
    uint8_t* ptr = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; ++i) {
        if (!SD.readRAW(ptr + (i * 512), lba + i)) {
            return -1;
        }
    }
    s_readSectors += count;
    return (int32_t)bufsize;
}

static bool onMscStartStop(uint8_t power_condition, bool start, bool load_eject) {
    (void)power_condition;
    (void)load_eject;
    s_hostMounted = start;
    return true;
}

void runUsbMscMode() {
    setMscBootFlag(false);

    if (!g_sys.sdMounted) {
        showToast("Insert SD card first!", 2000);
        return;
    }

    uint32_t sectorCount = (uint32_t)(SD.cardSize() / 512ULL);
    if (sectorCount == 0) {
        showToast("SD card not ready!", 2000);
        return;
    }

    M5Canvas* sprPtr = getScreenCanvas();
    if (!sprPtr) return;
    M5Canvas& spr = *sprPtr;

    g_sys.usbMscActive = true;
    s_readSectors = 0;
    s_writeSectors = 0;
    s_hostMounted = true;

    if (g_settings.deviceProfile != PROF_ANON && g_settings.ledEnabled) {
        neopixelWrite(LED_PIN, 0, 20, 40);
    } else {
        updateLedEngine();
    }

    s_msc.vendorID("CardOS");
    s_msc.productID("CardputerSD");
    s_msc.productRevision("1.0");
    s_msc.onRead(onMscRead);
    s_msc.onWrite(onMscWrite);
    s_msc.onStartStop(onMscStartStop);
    s_msc.begin(sectorCount, 512);
    s_msc.mediaPresent(true);

    // Trigger clean USB re-enumeration so host mounts the LUN immediately
    tud_disconnect();
    delay(150);
    tud_connect();

    uint32_t lastDraw = 0;
    while (true) {
        M5Cardputer.update();
        pollKeyboardInput();
        pollSerialCtl();

        bool exitReq = M5Cardputer.BtnA.wasPressed() || (g_sys.activeApp != APP_USB_MSC);
        InputEvent ev;
        while (popInputEvent(ev)) {
            if (isNavBack(ev) || (ev.code == CK_CHAR && (ev.ch == 'q' || ev.ch == 'Q'))) {
                exitReq = true;
            }
        }
        if (exitReq) break;

        uint32_t now = millis();
        if (now - lastDraw > 150) {
            lastDraw = now;
            const auto& th = activeTheme();
            spr.fillScreen(th.bg);
            ui::drawStatusBar(spr, "USB Mass Storage");

            spr.drawRoundRect(8, 22, SCREEN_W - 16, 86, 5, th.border);
            spr.setTextSize(1);
            spr.setTextColor(s_hostMounted ? th.ok : th.warn);
            spr.drawString(s_hostMounted ? "SD Card Exposed to USB Host" : "Host Ejected - Safe to Exit", 16, 30);

            spr.setTextColor(th.text);
            char buf[48];
            snprintf(buf, sizeof(buf), "Size: %lu MB  (%lu sectors)",
                     (unsigned long)(sectorCount / 2048), (unsigned long)sectorCount);
            spr.drawString(buf, 16, 48);

            spr.setTextColor(th.textDim);
            snprintf(buf, sizeof(buf), "Read: %lu KB   Write: %lu KB",
                     (unsigned long)(s_readSectors / 2), (unsigned long)(s_writeSectors / 2));
            spr.drawString(buf, 16, 66);

            spr.setTextColor(th.warn);
            spr.drawString("Drop .bin files into /apps/", 16, 86);

            spr.setTextColor(th.textDim);
            spr.drawString("Eject safely, then ESC (`) or G0", 16, 118);
            spr.pushSprite(0, 0);
        }
        delay(10);
    }

    // Tear down MSC in-place and re-mount FATFS so newly written files appear immediately
    s_msc.mediaPresent(false);
    s_msc.end();
    tud_disconnect();
    delay(150);
    tud_connect();

    SD.end();
    g_sys.sdMounted = false;
    delay(50);
    initStorage();
    g_sys.usbMscActive = false;
    updateLedEngine();
    showToast("USB Mode exited", 1200);
}
