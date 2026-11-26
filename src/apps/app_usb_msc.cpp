#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../usb_msc_mode.h"
#include <SD.h>

extern void switchApp(AppId id);

namespace {

class UsbMscApp : public App {
public:
    AppId id() const override { return APP_USB_MSC; }
    const char* name() const override { return "USB Disk"; }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (ev.code == CK_ENTER) {
            if (!g_sys.sdMounted) {
                showToast("Insert SD card first!", 2000);
                return;
            }
            runUsbMscMode();
            g_sys.lastActivityMs = millis();
            switchApp(APP_HOME);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "USB Mass Storage");

        spr.fillRoundRect(8, 22, SCREEN_W - 16, 86, 5, th.surface);
        spr.drawRoundRect(8, 22, SCREEN_W - 16, 86, 5, th.border);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("Expose SD Card via USB-C", 16, 30);

        spr.setTextColor(th.text);
        spr.drawString("1. Expose SD card over USB-C.", 16, 46);
        spr.drawString("2. Drop .bin apps into /apps/.", 16, 60);
        spr.drawString("3. Eject safely, press ESC/G0.", 16, 74);

        spr.setTextColor(g_sys.sdMounted ? th.ok : th.err);
        spr.drawString(g_sys.sdMounted ? "SD Card Ready" : "No SD Card Detected", 16, 92);

        spr.fillRoundRect(40, 113, SCREEN_W - 80, 17, 4, acc);
        spr.setTextColor(th.bg);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("[Enter] Start USB Mode", SCREEN_W / 2, 121);
        spr.setTextDatum(TL_DATUM);

        ui::drawToastOverlay(spr);
    }
};

} // namespace

App* createUsbMscApp() { return new UsbMscApp(); }

