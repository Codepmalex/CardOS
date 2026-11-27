#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

class FoldersApp : public App {
public:
    AppId id() const override { return APP_FOLDERS; }
    const char* name() const override { return "Folders"; }

    void onEnter() override {
        bool resetPos = false;
        if (strlen(g_sys.pendingFolderPath) > 0) {
            if (m_cwd != g_sys.pendingFolderPath) resetPos = true;
            m_cwd = g_sys.pendingFolderPath;
            g_sys.pendingFolderPath[0] = '\0';
        }
        if (m_cwd.isEmpty()) { m_cwd = "/"; resetPos = true; }
        m_creatingDir = false;
        m_confirmDel = false;
        m_dirSlideX = 0.0f;
        loadDir(resetPos);
        m_selAnimY = 20.0f + (m_sel - m_scroll) * 19.0f;
    }

    void onTick() override {
        float tgtY = 20.0f + (m_sel - m_scroll) * 19.0f;
        m_selAnimY = ui::smoothMove(m_selAnimY, tgtY, 0.48f);
        m_dirSlideX = ui::smoothMove(m_dirSlideX, 0.0f, 0.42f);
    }

    void onExit() override {
        std::vector<FileEntry>().swap(m_entries);
        m_newDirName = String();
    }

    void onInput(const InputEvent& ev) override {
        if (m_creatingDir) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
                m_creatingDir = false;
                return;
            }
            if (ev.code == CK_BACKSPACE && !m_newDirName.isEmpty()) {
                m_newDirName.remove(m_newDirName.length() - 1);
                return;
            }
            if (ev.code == CK_ENTER) {
                m_newDirName.trim();
                if (!m_newDirName.isEmpty()) {
                    String np = (m_cwd == "/") ? ("/" + m_newDirName) : (m_cwd + "/" + m_newDirName);
                    ensureDir(np.c_str());
                    loadDir();
                    showToast("Folder created", 1200);
                }
                m_creatingDir = false;
                return;
            }
            if (ev.code == CK_CHAR && (isalnum(ev.ch) || ev.ch == '_' || ev.ch == '-')) {
                if (m_newDirName.length() < 20) m_newDirName += ev.ch;
            }
            return;
        }

        if (m_confirmDel) {
            if (ev.code == CK_CHAR && (ev.ch == 'y' || ev.ch == 'Y') || ev.code == CK_ENTER) {
                if (m_sel >= 0 && m_sel < (int)m_entries.size() && m_entries[m_sel].name != "..") {
                    deletePathRecursive(m_entries[m_sel].path.c_str());
                    loadDir();
                    showToast("Deleted", 1200);
                }
                m_confirmDel = false;
            } else {
                m_confirmDel = false;
            }
            return;
        }

        if (m_infoModal) {
            if (isNavUp(ev) && m_infoScroll > 0) m_infoScroll--;
            else if (isNavDown(ev) && m_infoScroll < max(0, m_infoLines - 8)) m_infoScroll++;
            else if (isNavBack(ev) || ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) {
                m_infoModal = false;
            }
            return;
        }
        if (isNavBack(ev)) {
            if (m_cwd != "/") {
                goUp();
            } else {
                switchApp(APP_HOME);
            }
            return;
        }

        if (isNavUp(ev) && m_sel > 0) {
            m_sel--;
            if (m_sel < m_scroll) m_scroll = m_sel;
        } else if (isNavDown(ev) && m_sel < (int)m_entries.size() - 1) {
            m_sel++;
            if (m_sel >= m_scroll + 5) m_scroll = m_sel - 4;
        } else if (ev.code == CK_ENTER && !m_entries.empty()) {
            openSelected();
        } else if (ev.code == CK_BACKSPACE && m_cwd != "/") {
            goUp();
        } else if (ev.code == CK_CHAR && (ev.ch == 'n' || ev.ch == 'N')) {
            m_newDirName = "";
            m_creatingDir = true;
        } else if (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I') && !m_entries.empty()) {
            const auto& e = m_entries[m_sel];
            m_infoTitle = e.isDir ? "Folder Info" : "File Info";
            m_infoBody = "Name: " + e.name + "\nPath: " + e.path + "\nType: "
                       + (e.isDir ? "Directory" : formatBytes(e.size));
            m_infoScroll = 0;
            m_infoModal = true;
        } else if (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D')) {
            if (m_sel >= 0 && m_sel < (int)m_entries.size() && m_entries[m_sel].name != "..") {
                m_confirmDel = true;
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        String hdr = String(activeFSName()) + ": " + m_cwd;
        ui::drawStatusBar(spr, hdr.c_str());

        spr.setTextSize(1);
        if (m_entries.empty()) {
            spr.setTextColor(th.textDim);
            spr.drawString("(Empty folder)", 12, 52);
        } else {
            int ox = (int)roundf(m_dirSlideX);
            spr.setClipRect(0, 18, SCREEN_W - 6, 98);
            for (int i = 0; i < 5; ++i) {
                int idx = m_scroll + i;
                if (idx >= (int)m_entries.size()) break;
                int y = 20 + i * 19;
                bool sel = (idx == m_sel);
                const auto& e = m_entries[idx];

                spr.fillRoundRect(6 + ox, y, SCREEN_W - 16, 17, 3, sel ? th.surface2 : th.surface);

                spr.setTextColor(e.isDir ? th.warn : (sel ? acc : th.text));
                String prefix = e.isDir ? "[D] " : "    ";
                String nm = prefix + e.name;
                if ((int)nm.length() > 24) nm = nm.substring(0, 23) + "~";
                spr.drawString(nm, 10 + ox, y + 5);

                if (!e.isDir) {
                    spr.setTextColor(th.textDim);
                    spr.setTextDatum(TR_DATUM);
                    spr.drawString(formatBytes(e.size), SCREEN_W - 14 + ox, y + 5);
                    spr.setTextDatum(TL_DATUM);
                }
            }
            spr.drawRoundRect(6 + ox, (int)roundf(m_selAnimY), SCREEN_W - 16, 17, 3, acc);
            spr.clearClipRect();
            ui::drawScrollbar(spr, SCREEN_W - 6, 20, 95, (int)m_entries.size(), 5, m_scroll);
        }

        // Footer
        spr.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, th.surface);
        spr.setTextColor(th.textDim);
        char foot[48];
        if (g_sys.sdMounted) {
            snprintf(foot, sizeof(foot), "[N]Dir [I]Info [D]Del %luMB", (unsigned long)g_sys.sdFreeMB);
        } else {
            snprintf(foot, sizeof(foot), "[N]Dir [I]Info [D]Del (SPIFFS)");
        }
        spr.drawString(foot, 6, SCREEN_H - 12);

        if (m_creatingDir) {
            spr.fillRoundRect(20, 40, 200, 56, 5, th.surface2);
            spr.drawRoundRect(20, 40, 200, 56, 5, acc);
            spr.setTextColor(acc);
            spr.drawString("New Folder Name:", 28, 48);
            spr.setTextColor(th.text);
            spr.drawString(m_newDirName + "_", 28, 65);
        } else if (m_confirmDel) {
            spr.fillRoundRect(20, 42, 200, 52, 5, th.surface2);
            spr.drawRoundRect(20, 42, 200, 52, 5, th.err);
            spr.setTextColor(th.err);
            spr.drawString("Delete item?", 28, 50);
            spr.setTextColor(th.text);
            spr.drawString("[Y/Enter] Yes   [Any] Cancel", 28, 70);
        }

        ui::drawToastOverlay(spr);
        if (m_infoModal) {
            ui::drawTextModal(spr, m_infoTitle.c_str(), m_infoBody, m_infoScroll, &m_infoLines);
        }
    }

private:
    bool m_infoModal = false;
    String m_infoTitle;
    String m_infoBody;
    int m_infoScroll = 0;
    int m_infoLines = 0;
    String m_cwd = "/";
    std::vector<FileEntry> m_entries;
    int m_sel = 0;
    int m_scroll = 0;
    float m_selAnimY = 20.0f;
    float m_dirSlideX = 0.0f;
    bool m_creatingDir = false;
    String m_newDirName;
    bool m_confirmDel = false;

    void loadDir(bool resetPos = true) {
        m_entries.clear();
        if (m_cwd != "/") {
            FileEntry up;
            up.name = "..";
            up.path = "..";
            up.isDir = true;
            m_entries.push_back(up);
        }
        auto list = listDir(m_cwd.c_str());
        for (auto& e : list) m_entries.push_back(e);
        if (resetPos) {
            m_sel = 0;
            m_scroll = 0;
        } else {
            m_sel = constrain(m_sel, 0, max(0, (int)m_entries.size() - 1));
            int maxScroll = max(0, (int)m_entries.size() - 5);
            m_scroll = constrain(m_scroll, 0, maxScroll);
            if (m_sel < m_scroll) m_scroll = m_sel;
            if (m_sel >= m_scroll + 5) m_scroll = m_sel - 4;
        }
    }

    void goUp() {
        int sl = m_cwd.lastIndexOf('/');
        m_cwd = (sl > 0) ? m_cwd.substring(0, sl) : "/";
        m_dirSlideX = -48.0f;
        m_selAnimY = 20.0f;
        loadDir();
    }

    void openSelected() {
        const auto& e = m_entries[m_sel];
        if (e.name == "..") {
            goUp();
            return;
        }
        if (e.isDir) {
            m_cwd = e.path;
            m_dirSlideX = 48.0f;
            m_selAnimY = 20.0f;
            loadDir();
            return;
        }
        String low = e.name;
        low.toLowerCase();
        if (low.endsWith(".bin")) {
            installAndBootBinInteractive(e.path, e.name);
        } else if (low.endsWith(".txt") || low.endsWith(".md") || low.endsWith(".json") || low.endsWith(".csv") || low.endsWith(".log")) {
            strlcpy(g_sys.pendingNotePath, e.path.c_str(), sizeof(g_sys.pendingNotePath));
            g_sys.returnApp = APP_FOLDERS;
            switchApp(APP_NOTES);
        } else {
            showToast("Binary/Unsupported preview", 1500);
        }
    }
};

} // namespace

App* createFoldersApp() { return new FoldersApp(); }

