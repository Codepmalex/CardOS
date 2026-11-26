#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

class NotesApp : public App {
public:
    AppId id() const override { return APP_NOTES; }
    const char* name() const override { return "Notes"; }

    void onEnter() override {
        ensureDir("/notes");
        m_confirmExit = false;
        if (strlen(g_sys.pendingNotePath) > 0) {
            m_returnApp = (g_sys.returnApp == APP_FOLDERS || g_sys.returnApp == APP_HOME)
                              ? g_sys.returnApp
                              : APP_NOTES;
            g_sys.returnApp = APP_NOTES;
            String path = g_sys.pendingNotePath;
            g_sys.pendingNotePath[0] = '\0';
            openNote(path);
        } else {
            m_returnApp = APP_NOTES;
            g_sys.returnApp = APP_NOTES;
            refreshList();
            m_mode = MODE_LIST;
        }
    }

    void onInput(const InputEvent& ev) override {
        if (m_mode == MODE_LIST) {
            handleListInput(ev);
        } else if (m_mode == MODE_NEW_NAME) {
            handleNewNameInput(ev);
        } else {
            handleEditInput(ev);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);

        if (m_mode == MODE_LIST) {
            ui::drawStatusBar(spr, "Notes");
            spr.setTextSize(1);
            spr.setTextColor(th.textDim);
            spr.drawString("[N] New  [Enter] Open  [D] Del", 6, 19);

            if (m_files.empty()) {
                spr.setTextColor(th.textDim);
                spr.drawString("No notes yet. Press [N] to create.", 12, 54);
            } else {
                for (int i = 0; i < 5; ++i) {
                    int idx = m_scroll + i;
                    if (idx >= (int)m_files.size()) break;
                    int y = 32 + i * 19;
                    bool sel = (idx == m_sel);
                    spr.fillRoundRect(6, y, SCREEN_W - 16, 17, 3, sel ? th.surface2 : th.surface);
                    if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 17, 3, acc);

                    spr.setTextColor(sel ? acc : th.text);
                    String nm = m_files[idx].name;
                    if ((int)nm.length() > 24) nm = nm.substring(0, 23) + "~";
                    spr.drawString(nm, 11, y + 5);

                    spr.setTextColor(th.textDim);
                    spr.setTextDatum(TR_DATUM);
                    spr.drawString(formatBytes(m_files[idx].size), SCREEN_W - 14, y + 5);
                    spr.setTextDatum(TL_DATUM);
                }
                ui::drawScrollbar(spr, SCREEN_W - 6, 32, 95, (int)m_files.size(), 5, m_scroll);
            }
        } else if (m_mode == MODE_NEW_NAME) {
            ui::drawStatusBar(spr, "New Note");
            spr.drawRoundRect(12, 34, SCREEN_W - 24, 68, 5, th.border);
            spr.setTextSize(1);
            spr.setTextColor(th.text);
            spr.drawString("Filename (.txt auto-added):", 20, 44);
            spr.fillRoundRect(20, 60, SCREEN_W - 40, 18, 3, th.surface2);
            spr.drawRoundRect(20, 60, SCREEN_W - 40, 18, 3, acc);
            spr.drawString(m_newName + "_", 25, 65);
            spr.setTextColor(th.textDim);
            spr.drawString("Enter: Create   ESC: Cancel", 20, 86);
        } else {
            String title = m_curPath;
            int sl = title.lastIndexOf('/');
            if (sl >= 0) title = title.substring(sl + 1);
            if (m_dirty) title += "*";
            ui::drawStatusBar(spr, title.c_str());

            // Word-wrap m_content into 37-char lines
            std::vector<String> lines = wrapLines(m_content, 37);
            int maxVis = 8;
            if ((int)lines.size() > maxVis && m_viewScroll > (int)lines.size() - maxVis) {
                m_viewScroll = max(0, (int)lines.size() - maxVis);
            }
            spr.setTextSize(1);
            spr.setTextColor(th.text);
            for (int i = 0; i < maxVis; ++i) {
                int li = m_viewScroll + i;
                if (li >= (int)lines.size()) break;
                spr.drawString(lines[li], 5, 20 + i * 12);
            }

            // Footer hint
            spr.fillRect(0, SCREEN_H - 13, SCREEN_W, 13, th.surface);
            spr.setTextColor(th.textDim);
            char foot[48];
            snprintf(foot, sizeof(foot), "%s | %uch | Ctrl+S Save | ESC Exit",
                     activeFSName(), (unsigned)m_content.length());
            spr.drawString(foot, 5, SCREEN_H - 11);

            if (m_confirmExit) {
                int pw = 196, ph = 64;
                int px = (SCREEN_W - pw) / 2;
                int py = 36;
                spr.fillRoundRect(px, py, pw, ph, 6, th.surface2);
                spr.drawRoundRect(px, py, pw, ph, 6, acc);

                spr.setTextSize(1);
                spr.setTextColor(th.text);
                spr.setTextDatum(TC_DATUM);
                spr.drawString("Save changes before exit?", SCREEN_W / 2, py + 8);

                String fn = title;
                if (fn.endsWith("*")) fn.remove(fn.length() - 1);
                if ((int)fn.length() > 28) fn = fn.substring(0, 27) + "~";
                spr.setTextColor(th.textDim);
                spr.drawString(fn, SCREEN_W / 2, py + 21);

                const char* opts[3] = {"[Y]Save", "[N]Discard", "[C]Cancel"};
                int bw = 58, bh = 16;
                int gap = 5;
                int startX = px + (pw - (bw * 3 + gap * 2)) / 2;
                for (int i = 0; i < 3; ++i) {
                    int bx = startX + i * (bw + gap);
                    int by = py + 38;
                    bool sel = (m_confirmSel == i);
                    uint16_t fillCol = sel ? (i == 1 ? th.err : acc) : th.surface;
                    spr.fillRoundRect(bx, by, bw, bh, 3, fillCol);
                    spr.drawRoundRect(bx, by, bw, bh, 3, sel ? fillCol : th.border);
                    spr.setTextColor(sel ? th.bg : th.text);
                    spr.setTextDatum(MC_DATUM);
                    spr.drawString(opts[i], bx + bw / 2, by + bh / 2);
                }
                spr.setTextDatum(TL_DATUM);
            }
        }

        ui::drawToastOverlay(spr);
    }

private:
    enum Mode { MODE_LIST, MODE_NEW_NAME, MODE_EDIT } m_mode = MODE_LIST;
    std::vector<FileEntry> m_files;
    int m_sel = 0;
    int m_scroll = 0;

    String m_newName;
    String m_curPath;
    String m_content;
    bool m_dirty = false;
    int m_viewScroll = 0;
    AppId m_returnApp = APP_NOTES;
    bool m_confirmExit = false;
    int m_confirmSel = 0;

    void refreshList() {
        m_files.clear();
        auto all = listDir("/notes");
        for (auto& f : all) {
            if (!f.isDir) m_files.push_back(f);
        }
        if (m_sel >= (int)m_files.size()) m_sel = max(0, (int)m_files.size() - 1);
        g_sys.notesCount = (int)m_files.size();
    }

    void openNote(const String& path) {
        m_curPath = path;
        m_content = readTextFile(path.c_str(), 12288);
        m_dirty = false;
        m_confirmExit = false;
        m_viewScroll = 0;
        m_mode = MODE_EDIT;
    }

    void exitEditor() {
        m_confirmExit = false;
        if (m_returnApp != APP_NOTES) {
            AppId dest = m_returnApp;
            m_returnApp = APP_NOTES;
            switchApp(dest);
        } else {
            refreshList();
            m_mode = MODE_LIST;
        }
    }

    std::vector<String> wrapLines(const String& txt, int cols) {
        std::vector<String> out;
        String cur;
        for (size_t i = 0; i < txt.length(); ++i) {
            char c = txt[i];
            if (c == '\r') continue;
            if (c == '\n') {
                out.push_back(cur);
                cur = "";
            } else {
                cur += c;
                if ((int)cur.length() >= cols) {
                    out.push_back(cur);
                    cur = "";
                }
            }
        }
        String tail = cur + ((millis() / 450) % 2 == 0 ? "_" : "");
        out.push_back(tail);
        return out;
    }

    void handleListInput(const InputEvent& ev) {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (isNavUp(ev) && m_sel > 0) {
            m_sel--;
            if (m_sel < m_scroll) m_scroll = m_sel;
        } else if (isNavDown(ev) && m_sel < (int)m_files.size() - 1) {
            m_sel++;
            if (m_sel >= m_scroll + 5) m_scroll = m_sel - 4;
        } else if (ev.code == CK_ENTER && !m_files.empty()) {
            openNote(m_files[m_sel].path);
        } else if (ev.code == CK_CHAR && (ev.ch == 'n' || ev.ch == 'N')) {
            m_newName = "";
            m_mode = MODE_NEW_NAME;
        } else if (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D') && !m_files.empty()) {
            deletePathRecursive(m_files[m_sel].path.c_str());
            refreshList();
            showToast("Note deleted", 1200);
        }
    }

    void handleNewNameInput(const InputEvent& ev) {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            m_mode = MODE_LIST;
            return;
        }
        if (ev.code == CK_BACKSPACE && !m_newName.isEmpty()) {
            m_newName.remove(m_newName.length() - 1);
            return;
        }
        if (ev.code == CK_ENTER) {
            String nm = m_newName;
            nm.trim();
            if (nm.isEmpty()) nm = "note_" + String(millis() / 1000);
            if (!nm.endsWith(".txt") && !nm.endsWith(".md")) nm += ".txt";
            String path = "/notes/" + nm;
            m_curPath = path;
            m_content = "";
            m_dirty = false;
            m_confirmExit = false;
            m_viewScroll = 0;
            m_mode = MODE_EDIT;
            return;
        }
        if (ev.code == CK_CHAR && (isalnum(ev.ch) || ev.ch == '_' || ev.ch == '-' || ev.ch == '.')) {
            if (m_newName.length() < 24) m_newName += ev.ch;
        }
    }

    void handleEditInput(const InputEvent& ev) {
        if (m_confirmExit) {
            if (isNavLeft(ev)) {
                m_confirmSel = (m_confirmSel + 2) % 3;
                return;
            }
            if (isNavRight(ev)) {
                m_confirmSel = (m_confirmSel + 1) % 3;
                return;
            }
            if ((ev.code == CK_CHAR && (ev.ch == 'y' || ev.ch == 'Y')) || (ev.code == CK_ENTER && m_confirmSel == 0)) {
                if (writeTextFile(m_curPath.c_str(), m_content)) {
                    m_dirty = false;
                    showToast("Saved!", 1200);
                } else {
                    showToast("Save failed", 1500);
                }
                exitEditor();
                return;
            }
            if ((ev.code == CK_CHAR && (ev.ch == 'n' || ev.ch == 'N')) || (ev.code == CK_ENTER && m_confirmSel == 1)) {
                m_dirty = false;
                showToast("Discarded", 1000);
                exitEditor();
                return;
            }
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && (ev.ch == '`' || ev.ch == 'c' || ev.ch == 'C')) || (ev.code == CK_ENTER && m_confirmSel == 2)) {
                m_confirmExit = false;
                return;
            }
            return;
        }

        if (ev.code == CK_ESC || (ev.ctrl && (ev.ch == 'q' || ev.ch == 'Q'))) {
            if (m_dirty) {
                m_confirmExit = true;
                m_confirmSel = 0;
            } else {
                exitEditor();
            }
            return;
        }
        if ((ev.ctrl && (ev.ch == 's' || ev.ch == 'S')) || (ev.fn && ev.code == CK_ENTER)) {
            if (writeTextFile(m_curPath.c_str(), m_content)) {
                m_dirty = false;
                showToast("Saved!", 1200);
            } else {
                showToast("Save failed", 1500);
            }
            return;
        }
        if (ev.code == CK_UP) {
            if (m_viewScroll > 0) m_viewScroll--;
            return;
        }
        if (ev.code == CK_DOWN) {
            m_viewScroll++;
            return;
        }
        if (ev.code == CK_BACKSPACE) {
            if (!m_content.isEmpty()) {
                m_content.remove(m_content.length() - 1);
                m_dirty = true;
            }
            return;
        }
        if (ev.code == CK_ENTER) {
            m_content += '\n';
            m_dirty = true;
            auto lines = wrapLines(m_content, 37);
            if ((int)lines.size() > 8) m_viewScroll = (int)lines.size() - 8;
            return;
        }
        if (ev.code == CK_TAB) {
            m_content += "  ";
            m_dirty = true;
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126) {
            m_content += ev.ch;
            m_dirty = true;
            auto lines = wrapLines(m_content, 37);
            if ((int)lines.size() > 8) m_viewScroll = (int)lines.size() - 8;
        }
    }
};

} // namespace

App* createNotesApp() { return new NotesApp(); }

