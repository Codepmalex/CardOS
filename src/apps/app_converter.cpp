#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include <math.h>

extern void switchApp(AppId id);

namespace {

struct UnitDef {
    const char* name;
    double toBase; // base * factor (unused for Temp)
};

struct UnitCategory {
    const char* name;
    int count;
    UnitDef units[5];
};

static const UnitCategory CATEGORIES[8] = {
    {"Length", 5, {{"m", 1.0}, {"km", 1000.0}, {"cm", 0.01}, {"mi", 1609.344}, {"ft", 0.3048}}},
    {"Mass",   4, {{"g", 1.0}, {"kg", 1000.0}, {"lb", 453.59237}, {"oz", 28.3495}}},
    {"Temp",   3, {{"C", 1.0}, {"F", 1.0}, {"K", 1.0}}},
    {"Volume", 4, {{"L", 1.0}, {"mL", 0.001}, {"gal", 3.78541}, {"floz", 0.0295735}}},
    {"Speed",  4, {{"m/s", 1.0}, {"km/h", 0.277778}, {"mph", 0.44704}, {"kt", 0.514444}}},
    {"Area",   4, {{"m2", 1.0}, {"km2", 1e6}, {"ft2", 0.092903}, {"acre", 4046.86}}},
    {"Data",   5, {{"B", 1.0}, {"KB", 1024.0}, {"MB", 1048576.0}, {"GB", 1073741824.0}, {"bit", 0.125}}},
    {"Time",   5, {{"s", 1.0}, {"min", 60.0}, {"hr", 3600.0}, {"day", 86400.0}, {"wk", 604800.0}}}
};

class ConverterApp : public App {
public:
    AppId id() const override { return APP_CONVERTER; }
    const char* name() const override { return "Convert"; }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (isNavUp(ev)) {
            m_focusRow = (m_focusRow + 3) % 4;
        } else if (isNavDown(ev)) {
            m_focusRow = (m_focusRow + 1) % 4;
        } else if (isNavLeft(ev)) {
            stepRow(-1);
        } else if (isNavRight(ev)) {
            stepRow(1);
        } else if (ev.code == CK_BACKSPACE) {
            if (!m_valStr.isEmpty()) m_valStr.remove(m_valStr.length() - 1);
        } else if (ev.code == CK_CHAR && (ev.ch == 's' || ev.ch == 'S')) {
            std::swap(m_fromIdx, m_toIdx);
        } else if (ev.code == CK_CHAR && (isdigit(ev.ch) || ev.ch == '.' || ev.ch == '-')) {
            if (m_valStr.length() < 14) m_valStr += ev.ch;
        } else if (ev.code == CK_CHAR && (ev.ch == 'c' || ev.ch == 'C')) {
            m_valStr = "1";
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Unit Converter");

        const auto& cat = CATEGORIES[m_catIdx];
        m_fromIdx = constrain(m_fromIdx, 0, cat.count - 1);
        m_toIdx   = constrain(m_toIdx,   0, cat.count - 1);

        spr.setTextSize(1);
        // Row 0: Category
        drawRowBox(spr, 0, 20, "Category", String("< ") + cat.name + " >", acc, th);
        // Row 1: From unit
        drawRowBox(spr, 1, 40, "From", String("< ") + cat.units[m_fromIdx].name + " >", acc, th);
        // Row 2: To unit
        drawRowBox(spr, 2, 60, "To", String("< ") + cat.units[m_toIdx].name + " >", acc, th);
        // Row 3: Value input
        drawRowBox(spr, 3, 80, "Input", m_valStr + "_", acc, th);

        // Result Banner
        double res = convertValue(m_valStr.toDouble());
        char rbuf[32];
        if (fabs(res) >= 1e7 || (fabs(res) > 0 && fabs(res) < 1e-4)) {
            snprintf(rbuf, sizeof(rbuf), "%.4e", res);
        } else {
            snprintf(rbuf, sizeof(rbuf), "%.4f", res);
            // Trim trailing zeros after decimal
            String s(rbuf);
            if (s.indexOf('.') >= 0) {
                while (s.endsWith("0")) s.remove(s.length() - 1);
                if (s.endsWith(".")) s.remove(s.length() - 1);
            }
            strlcpy(rbuf, s.c_str(), sizeof(rbuf));
        }

        spr.fillRoundRect(6, 102, SCREEN_W - 12, 20, 4, th.surface2);
        spr.drawRoundRect(6, 102, SCREEN_W - 12, 20, 4, acc);
        spr.setTextColor(acc);
        spr.drawString(String("= ") + rbuf + " " + cat.units[m_toIdx].name, 12, 108);

        spr.setTextColor(th.textDim);
        spr.drawString("Type digits  [S] Swap  [C] Reset", 8, 125);
    }

private:
    int m_focusRow = 0;
    int m_catIdx = 0;
    int m_fromIdx = 0;
    int m_toIdx = 1;
    String m_valStr = "1";

    void stepRow(int d) {
        const auto& cat = CATEGORIES[m_catIdx];
        if (m_focusRow == 0) {
            m_catIdx = (m_catIdx + d + 8) % 8;
            m_fromIdx = 0;
            m_toIdx = (CATEGORIES[m_catIdx].count > 1) ? 1 : 0;
        } else if (m_focusRow == 1) {
            m_fromIdx = (m_fromIdx + d + cat.count) % cat.count;
        } else if (m_focusRow == 2) {
            m_toIdx = (m_toIdx + d + cat.count) % cat.count;
        }
    }

    double convertValue(double v) const {
        const auto& cat = CATEGORIES[m_catIdx];
        if (m_catIdx == 2) { // Temperature
            double c = v;
            if (m_fromIdx == 1) c = (v - 32.0) * 5.0 / 9.0;
            else if (m_fromIdx == 2) c = v - 273.15;

            if (m_toIdx == 0) return c;
            if (m_toIdx == 1) return c * 9.0 / 5.0 + 32.0;
            return c + 273.15;
        }
        double base = v * cat.units[m_fromIdx].toBase;
        return base / cat.units[m_toIdx].toBase;
    }

    void drawRowBox(M5Canvas& spr, int row, int y, const char* lbl, const String& val, uint16_t acc, const ThemePalette& th) {
        bool sel = (m_focusRow == row);
        spr.fillRoundRect(6, y, SCREEN_W - 12, 17, 3, sel ? th.surface2 : th.surface);
        if (sel) spr.drawRoundRect(6, y, SCREEN_W - 12, 17, 3, acc);
        spr.setTextColor(th.textDim);
        spr.drawString(lbl, 12, y + 5);
        spr.setTextColor(sel ? acc : th.text);
        spr.setTextDatum(TR_DATUM);
        spr.drawString(val, SCREEN_W - 12, y + 5);
        spr.setTextDatum(TL_DATUM);
    }
};

} // namespace

App* createConverterApp() { return new ConverterApp(); }

