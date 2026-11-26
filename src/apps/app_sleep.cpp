#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../hardware.h"
#include <M5Cardputer.h>

extern void switchApp(AppId id);
extern M5Canvas* getScreenCanvas();

namespace {

class SleepApp : public App {
public:
    AppId id() const override { return APP_SLEEP; }
    const char* name() const override { return "Sleep"; }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' ')) {
            if (M5Canvas* sprPtr = getScreenCanvas()) {
                const auto& th = activeTheme();
                sprPtr->fillScreen(th.bg);
                sprPtr->setTextSize(1);
                sprPtr->setTextColor(th.textDim);
                sprPtr->setTextDatum(MC_DATUM);
                sprPtr->drawString("Entering Sleep...", SCREEN_W / 2, SCREEN_H / 2);
                sprPtr->setTextDatum(TL_DATUM);
                sprPtr->pushSprite(0, 0);
                delay(120);
            }
            enterChargeSleepMode();
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Sleep");

        spr.fillRoundRect(8, 22, SCREEN_W - 16, 86, 5, th.surface);
        spr.drawRoundRect(8, 22, SCREEN_W - 16, 86, 5, th.border);

        spr.setTextSize(1);
        spr.setTextColor(g_sys.isCharging ? th.ok : acc);
        char batBuf[48];
        snprintf(batBuf, sizeof(batBuf), "Battery: %d%% (%s)",
                 g_sys.batteryPct, g_sys.isCharging ? "Charging" : "On Battery");
        spr.drawString(batBuf, 16, 30);

        spr.setTextColor(th.text);
        spr.drawString("Turns off screen, Wi-Fi, LED,", 16, 48);
        spr.drawString("sound & all notifications so it", 16, 62);
        spr.drawString("stays silent while charging.", 16, 76);

        spr.setTextColor(th.textDim);
        spr.drawString("Wake: Press G0 or any key", 16, 94);

        spr.fillRoundRect(46, 113, SCREEN_W - 92, 17, 4, acc);
        spr.setTextColor(th.bg);
        spr.setTextDatum(MC_DATUM);
        spr.drawString("[Enter] Sleep Now", SCREEN_W / 2, 121);
        spr.setTextDatum(TL_DATUM);

        ui::drawToastOverlay(spr);
    }
};

} // namespace

App* createSleepApp() { return new SleepApp(); }

