#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include <esp_system.h>
#include <esp_partition.h>

extern void switchApp(AppId id);

namespace {

class SysInfoApp : public App {
public:
    AppId id() const override { return APP_SYSINFO; }
    const char* name() const override { return "SysInfo"; }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (isNavLeft(ev)) m_tab = (m_tab + 2) % 3;
        else if (isNavRight(ev)) m_tab = (m_tab + 1) % 3;
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "SysInfo");

        // Tab strip (y: 18..31)
        const char* tabs[3] = {"Compute", "Storage", "Power/Net"};
        for (int t = 0; t < 3; ++t) {
            int tx = 6 + t * 76;
            bool act = (t == m_tab);
            spr.fillRoundRect(tx, 18, 72, 13, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.setTextDatum(MC_DATUM);
            spr.drawString(tabs[t], tx + 36, 24);
            spr.setTextDatum(TL_DATUM);
        }

        spr.fillRoundRect(6, 35, SCREEN_W - 12, 85, 5, th.surface);
        spr.drawRoundRect(6, 35, SCREEN_W - 12, 85, 5, th.border);
        spr.setTextSize(1);

        char b[64];
        if (m_tab == 0) {
            uint32_t up = millis() / 1000;
            uint32_t heapTot = ESP.getHeapSize() / 1024;
            uint32_t heapFree = ESP.getFreeHeap() / 1024;
            uint32_t heapUsed = (heapTot > heapFree) ? (heapTot - heapFree) : 0;
            uint32_t minFree = ESP.getMinFreeHeap() / 1024;

            spr.setTextColor(acc);
            snprintf(b, sizeof(b), "ESP32-S3  @  %u MHz", (unsigned)getCpuFrequencyMhz());
            spr.drawString(b, 14, 42);

            spr.setTextColor(th.text);
            snprintf(b, sizeof(b), "RAM: %lu/%lu KB (Min %luK)",
                     (unsigned long)heapUsed, (unsigned long)heapTot, (unsigned long)minFree);
            spr.drawString(b, 14, 57);

            int barW = SCREEN_W - 36;
            spr.fillRoundRect(14, 68, barW, 8, 2, th.bg);
            int rw = (heapTot > 0) ? (int)((heapUsed * barW) / heapTot) : 0;
            spr.fillRoundRect(14, 68, rw, 8, 2, acc);

            spr.setTextColor(th.textDim);
            snprintf(b, sizeof(b), "Uptime: %luh %lum %lus  |  IMU: %s",
                     (unsigned long)(up / 3600), (unsigned long)((up % 3600) / 60),
                     (unsigned long)(up % 60), g_sys.imuReady ? "OK" : "Off");
            spr.drawString(b, 14, 83);

            snprintf(b, sizeof(b), "ECO: %s  |  Flash: %uMB",
                     g_settings.lowPowerMode ? "ON" : "OFF",
                     (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
            spr.drawString(b, 14, 98);
        } else if (m_tab == 1) {
            uint32_t skUsed = g_sys.sdTotalMB > g_sys.sdFreeMB ? (g_sys.sdTotalMB - g_sys.sdFreeMB) : 0;
            spr.setTextColor(g_sys.sdMounted ? th.ok : th.err);
            snprintf(b, sizeof(b), "SD Card: %s (%lu / %lu MB free)",
                     g_sys.sdMounted ? "Mounted" : "Missing",
                     (unsigned long)g_sys.sdFreeMB, (unsigned long)g_sys.sdTotalMB);
            spr.drawString(b, 14, 42);

            int barW = SCREEN_W - 36;
            spr.fillRoundRect(14, 54, barW, 7, 2, th.bg);
            int sw = (g_sys.sdTotalMB > 0) ? (int)((skUsed * (uint64_t)barW) / g_sys.sdTotalMB) : 0;
            if (sw > 0) spr.fillRoundRect(14, 54, max(3, sw), 7, 2, th.ok);

            uint32_t skSz = ESP.getSketchSize() / 1024;
            uint32_t skFree = ESP.getFreeSketchSpace() / 1024;
            spr.setTextColor(th.text);
            snprintf(b, sizeof(b), "OS Image: %lu KB (Free %lu KB)",
                     (unsigned long)skSz, (unsigned long)skFree);
            spr.drawString(b, 14, 68);

            spr.setTextColor(th.textDim);
            const char* binName = strlen(g_settings.installedBinName) ? g_settings.installedBinName : "(Empty)";
            snprintf(b, sizeof(b), "App1: %s", binName);
            String s(b);
            if ((int)s.length() > 34) s = s.substring(0, 33) + "~";
            spr.drawString(s, 14, 83);

            snprintf(b, sizeof(b), "SPIFFS: %s  |  Notes: %d",
                     g_sys.spiffsMounted ? "OK" : "Off", g_sys.notesCount);
            spr.drawString(b, 14, 98);
        } else {
            spr.setTextColor(g_sys.isCharging ? rgb565(250, 204, 21) : th.ok);
            snprintf(b, sizeof(b), "Battery: %d%% (%dmV) %s",
                     g_sys.batteryPct, g_sys.batteryMv,
                     g_sys.isCharging ? "[Charging]" : "[Disch]");
            spr.drawString(b, 14, 42);

            spr.setTextColor(g_sys.wifiConnected ? th.ok : th.warn);
            snprintf(b, sizeof(b), "Wi-Fi: %s", g_sys.wifiStatus);
            spr.drawString(b, 14, 57);

            spr.setTextColor(th.text);
            snprintf(b, sizeof(b), "IP: %s  |  RSSI: %d dBm", g_sys.wifiIp, g_sys.wifiRssi);
            spr.drawString(b, 14, 72);

            int savedCnt = 0;
            for (int i = 0; i < MAX_SAVED_WIFIS; ++i) {
                if (strlen(g_settings.savedWifis[i].ssid) > 0) savedCnt++;
            }
            spr.setTextColor(th.textDim);
            snprintf(b, sizeof(b), "Saved Wi-Fis: %d/%d  |  NTP: %s",
                     savedCnt, MAX_SAVED_WIFIS, g_sys.ntpSynced ? "Synced" : "Pending");
            spr.drawString(b, 14, 87);

            snprintf(b, sizeof(b), "Saver: %s (%u%%)  |  Mode: %s",
                     g_settings.screensaverEnabled ? "ON" : "OFF",
                     (unsigned)((g_settings.screensaverBright * 100) / 255),
                     PROFILE_NAMES[g_settings.deviceProfile % PROF_COUNT]);
            spr.drawString(b, 14, 102);
        }

        spr.setTextColor(th.textDim);
        spr.drawString("</> Tab   [ESC] Home", 8, 124);
        ui::drawToastOverlay(spr);
    }

private:
    int m_tab = 0;
};

} // namespace

App* createSysInfoApp() { return new SysInfoApp(); }

