#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <SD.h>
#include <WiFi.h>
#include <time.h>

extern void switchApp(AppId id);

namespace {

class TerminalApp : public App {
public:
    AppId id() const override { return APP_TERMINAL; }
    const char* name() const override { return "Shell"; }

    void onEnter() override {
        if (m_log.isEmpty()) {
            m_log = "CardOS Shell v" + String(CARDOS_VERSION) + "\nType 'help' or '@mac <cmd>'.";
        }
        m_cwd = "/";
        m_scroll = 0;
    }

    void onInput(const InputEvent& ev) override {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            switchApp(APP_HOME);
            return;
        }
        if (isNavUp(ev) && m_scroll > 0) { m_scroll--; return; }
        if (isNavDown(ev) && m_scroll < max(0, m_totalLines - 7)) { m_scroll++; return; }
        if (ev.code == CK_BACKSPACE && !m_cmd.isEmpty()) {
            m_cmd.remove(m_cmd.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            String c = m_cmd;
            m_cmd = "";
            c.trim();
            if (!c.isEmpty()) runCmd(c);
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_cmd.length() < 90) {
            m_cmd += ev.ch;
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "CardOS Shell", true);

        spr.fillRoundRect(6, 18, SCREEN_W - 12, 86, 4, th.surface);
        spr.drawRoundRect(6, 18, SCREEN_W - 12, 86, 4, th.border);
        spr.setTextColor(th.text);
        ui::drawWrappedText(spr, m_log, 10, 22, SCREEN_W - 26, 78, 10, m_scroll, &m_totalLines);
        if (m_totalLines > 7) {
            ui::drawScrollbar(spr, SCREEN_W - 10, 20, 82, m_totalLines, 7, m_scroll);
        }

        // Prompt bar
        spr.fillRoundRect(6, 107, SCREEN_W - 12, 16, 4, th.surface2);
        spr.drawRoundRect(6, 107, SCREEN_W - 12, 16, 4, acc);
        spr.setTextSize(1);
        spr.setTextColor(th.ok);
        spr.drawString(">", 10, 111);
        spr.setTextColor(th.text);
        String dc = m_cmd;
        if ((int)dc.length() > 31) dc = dc.substring(dc.length() - 31);
        if ((millis() / 400) % 2 == 0) dc += "_";
        spr.drawString(dc, 19, 111);

        spr.setTextColor(th.textDim);
        spr.drawString("[Ent]Run [Up/Dn]Scroll [ESC]Home", 6, 126);
        ui::drawToastOverlay(spr);
    }

private:
    String m_cwd = "/";
    String m_cmd;
    String m_log;
    int m_scroll = 0;
    int m_totalLines = 0;

    void appendOut(const String& line) {
        if (!m_log.isEmpty()) m_log += "\n";
        m_log += line;
        if (m_log.length() > 2600) {
            int cut = (int)m_log.length() - 2000;
            int nl = m_log.indexOf('\n', cut);
            m_log = (nl >= 0) ? m_log.substring(nl + 1) : m_log.substring(cut);
        }
        m_scroll = max(0, m_totalLines); // auto scroll near bottom
    }

    String resolvePath(String arg) {
        arg.trim();
        if (arg.isEmpty()) return m_cwd;
        if (arg.startsWith("/")) return arg;
        if (m_cwd == "/") return "/" + arg;
        return m_cwd + "/" + arg;
    }

    void runCmd(const String& raw) {
        appendOut("> " + raw);
        if (raw.startsWith("@mac ")) {
            String out;
            macLinkExecCommand(raw.substring(5), out);
            appendOut(out);
            return;
        }
        int sp = raw.indexOf(' ');
        String op = (sp < 0) ? raw : raw.substring(0, sp);
        String arg = (sp < 0) ? "" : raw.substring(sp + 1);
        op.toLowerCase();
        arg.trim();

        if (op == "help") {
            appendOut("ls cd pwd cat mkdir rm free uptime ver wifi ip date echo clear @mac <cmd>");
        } else if (op == "clear") {
            m_log = "";
            m_scroll = 0;
        } else if (op == "ver") {
            appendOut(String("CardOS v") + CARDOS_VERSION + " (" + activeFSName() + ")");
        } else if (op == "pwd") {
            appendOut(m_cwd);
        } else if (op == "free") {
            appendOut("Heap: " + String(ESP.getFreeHeap() / 1024) + "KB  MaxBlk: " + String(ESP.getMaxAllocHeap() / 1024) + "KB");
        } else if (op == "uptime") {
            appendOut(String(millis() / 1000) + "s");
        } else if (op == "ip" || op == "wifi") {
            appendOut(String(g_sys.wifiConnected ? WiFi.localIP().toString() : String("Offline")) + " (" + g_sys.wifiStatus + ")");
        } else if (op == "date") {
            struct tm t;
            if (getSafeLocalTime(t)) {
                char b[32];
                snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
                appendOut(b);
            } else appendOut("No NTP time");
        } else if (op == "echo") {
            appendOut(arg);
        } else if (op == "cd") {
            if (arg == "..") {
                int sl = m_cwd.lastIndexOf('/');
                m_cwd = (sl > 0) ? m_cwd.substring(0, sl) : "/";
            } else {
                String np = resolvePath(arg);
                if (activeFS().exists(np.c_str())) m_cwd = np;
                else appendOut("No such dir");
            }
        } else if (op == "ls") {
            String p = resolvePath(arg);
            auto entries = listDir(p.c_str());
            if (entries.empty()) appendOut("(empty)");
            else {
                String out;
                for (size_t i = 0; i < entries.size() && i < 24; ++i) {
                    if (i > 0) out += "  ";
                    out += (entries[i].isDir ? (entries[i].name + "/") : entries[i].name);
                }
                appendOut(out);
            }
        } else if (op == "cat" && !arg.isEmpty()) {
            String p = resolvePath(arg);
            String txt = readTextFile(p.c_str(), 1024);
            appendOut(txt.length() ? txt : "(empty/missing)");
        } else if (op == "mkdir" && !arg.isEmpty()) {
            String p = resolvePath(arg);
            appendOut(ensureDir(p.c_str()) ? "Created" : "Failed");
        } else if (op == "rm" && !arg.isEmpty()) {
            String p = resolvePath(arg);
            appendOut(deletePathRecursive(p.c_str()) ? "Deleted" : "Failed");
        } else {
            appendOut("Unknown command (try 'help')");
        }
    }
};

} // namespace

App* createTerminalApp() { return new TerminalApp(); }

