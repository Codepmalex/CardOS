#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

static constexpr const char* kTasksPath = "/tasks.tsv";

struct TaskItem {
    bool done = false;
    bool prio = false;
    String text;
};

class TasksApp : public App {
public:
    AppId id() const override { return APP_TASKS; }
    const char* name() const override { return "Tasks"; }

    void onEnter() override {
        m_adding = false;
        m_newText = "";
        loadTasks();
        rebuildView();
    }

    void onExit() override {
        m_adding = false;
        m_newText = String();
        std::vector<TaskItem>().swap(m_tasks);
        std::vector<int>().swap(m_viewIdx);
    }

    void onInput(const InputEvent& ev) override {
        if (m_adding) {
            if (isNavBack(ev)) {
                m_adding = false;
                m_newText = "";
                return;
            }
            if (ev.code == CK_BACKSPACE && !m_newText.isEmpty()) {
                m_newText.remove(m_newText.length() - 1);
                return;
            }
            if (ev.code == CK_ENTER) {
                m_newText.trim();
                if (!m_newText.isEmpty()) {
                    m_newText.replace("|", " ");
                    m_newText.replace("\n", " ");
                    TaskItem nt;
                    nt.done = false;
                    nt.prio = false;
                    nt.text = m_newText;
                    m_tasks.insert(m_tasks.begin(), nt);
                    saveTasks();
                    rebuildView();
                    m_sel = 0;
                    m_scroll = 0;
                    showToast("Task added", 900);
                }
                m_adding = false;
                m_newText = "";
                return;
            }
            if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_newText.length() < 40) {
                m_newText += ev.ch;
            }
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        char ch = (ev.code == CK_CHAR) ? (char)tolower((unsigned char)ev.ch) : 0;

        if (ch == 'a') {
            m_adding = true;
            m_newText = "";
            return;
        }
        if (ch == 'f') {
            m_filter = (m_filter + 1) % 3;
            m_sel = 0;
            m_scroll = 0;
            rebuildView();
            return;
        }
        if (ch == 'c') {
            int removed = 0;
            for (int i = (int)m_tasks.size() - 1; i >= 0; --i) {
                if (m_tasks[i].done) {
                    m_tasks.erase(m_tasks.begin() + i);
                    removed++;
                }
            }
            if (removed > 0) {
                saveTasks();
                rebuildView();
                showToast("Cleared done", 900);
            }
            return;
        }

        if (m_viewIdx.empty()) return;

        if (isNavUp(ev) && m_sel > 0) {
            m_sel--;
            if (m_sel < m_scroll) m_scroll = m_sel;
        } else if (isNavDown(ev) && m_sel < (int)m_viewIdx.size() - 1) {
            m_sel++;
            if (m_sel >= m_scroll + 4) m_scroll = m_sel - 3;
        } else if (ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' ')) {
            int ti = m_viewIdx[m_sel];
            m_tasks[ti].done = !m_tasks[ti].done;
            saveTasks();
            rebuildView();
        } else if (ch == 'p') {
            int ti = m_viewIdx[m_sel];
            m_tasks[ti].prio = !m_tasks[ti].prio;
            saveTasks();
            rebuildView();
        } else if (ev.code == CK_DEL || ev.code == CK_BACKSPACE || ch == 'd') {
            int ti = m_viewIdx[m_sel];
            m_tasks.erase(m_tasks.begin() + ti);
            saveTasks();
            rebuildView();
            showToast("Deleted", 800);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Tasks");

        if (m_adding) {
            spr.fillRoundRect(10, 26, SCREEN_W - 20, 82, 6, th.surface);
            spr.drawRoundRect(10, 26, SCREEN_W - 20, 82, 6, acc);
            spr.setTextSize(1);
            spr.setTextColor(acc);
            spr.drawString("New Task:", 18, 34);
            spr.fillRoundRect(18, 50, SCREEN_W - 36, 22, 4, th.bg);
            spr.drawRoundRect(18, 50, SCREEN_W - 36, 22, 4, acc);
            String disp = m_newText;
            if ((int)disp.length() > 30) disp = disp.substring(disp.length() - 30);
            if ((millis() / 400) % 2 == 0) disp += "_";
            spr.setTextColor(th.text);
            spr.drawString(disp, 24, 57);
            spr.setTextColor(th.textDim);
            spr.drawString("[Enter] Save   [ESC] Cancel", 18, 86);
            ui::drawToastOverlay(spr);
            return;
        }

        // Top summary + filter pill
        static const char* FILT_NAMES[3] = {"All", "Todo", "Done"};
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        char sb[32];
        snprintf(sb, sizeof(sb), "%d left / %d total", g_sys.pendingTasksCount, (int)m_tasks.size());
        spr.drawString(sb, 6, 20);

        spr.fillRoundRect(184, 18, 50, 12, 3, th.surface2);
        spr.setTextColor(acc);
        spr.setTextDatum(MC_DATUM);
        spr.drawString(String("[F]") + FILT_NAMES[m_filter], 209, 24);
        spr.setTextDatum(TL_DATUM);

        if (m_viewIdx.empty()) {
            spr.fillRoundRect(8, 36, SCREEN_W - 16, 78, 6, th.surface);
            spr.drawRoundRect(8, 36, SCREEN_W - 16, 78, 6, th.border);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(th.textDim);
            spr.drawString(m_tasks.empty() ? "No tasks yet on SD" : "No tasks in this filter", SCREEN_W / 2, 66);
            spr.setTextColor(acc);
            spr.drawString("Press [A] to add a task", SCREEN_W / 2, 84);
            spr.setTextDatum(TL_DATUM);
        } else {
            int y0 = 33;
            for (int r = 0; r < 4; ++r) {
                int vi = m_scroll + r;
                if (vi >= (int)m_viewIdx.size()) break;
                const auto& t = m_tasks[m_viewIdx[vi]];
                int y = y0 + r * 22;
                bool sel = (vi == m_sel);

                spr.fillRoundRect(6, y, SCREEN_W - 16, 20, 4, sel ? th.surface2 : th.surface);
                spr.drawRoundRect(6, y, SCREEN_W - 16, 20, 4, sel ? acc : th.border);

                // Checkbox
                spr.drawRect(11, y + 5, 10, 10, t.done ? th.ok : (t.prio ? th.err : th.textDim));
                if (t.done) {
                    spr.drawLine(13, y + 10, 15, y + 12, th.ok);
                    spr.drawLine(15, y + 12, 19, y + 7, th.ok);
                } else if (t.prio) {
                    spr.setTextColor(th.err);
                    spr.drawString("!", 14, y + 6);
                }

                String txt = t.text;
                if ((int)txt.length() > 31) txt = txt.substring(0, 29) + "..";
                spr.setTextColor(t.done ? th.textDim : (sel ? th.text : (t.prio ? th.warn : th.text)));
                spr.drawString(txt, 26, y + 6);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, y0, 86, (int)m_viewIdx.size(), 4, m_scroll);
        }

        spr.setTextColor(th.textDim);
        spr.drawString("[A]Add [Ent]Done [P]! [D]Del [C]Clear", 6, 124);
        ui::drawToastOverlay(spr);
    }

private:
    std::vector<TaskItem> m_tasks;
    std::vector<int> m_viewIdx;
    int m_sel = 0;
    int m_scroll = 0;
    int m_filter = 0; // 0=All, 1=Todo, 2=Done
    bool m_adding = false;
    String m_newText;

    void updateCount() {
        int pend = 0;
        for (const auto& t : m_tasks) {
            if (!t.done) pend++;
        }
        g_sys.pendingTasksCount = pend;
    }

    void rebuildView() {
        m_viewIdx.clear();
        for (int i = 0; i < (int)m_tasks.size(); ++i) {
            if (m_filter == 1 && m_tasks[i].done) continue;
            if (m_filter == 2 && !m_tasks[i].done) continue;
            m_viewIdx.push_back(i);
        }
        if (m_sel >= (int)m_viewIdx.size()) m_sel = max(0, (int)m_viewIdx.size() - 1);
        m_scroll = constrain(m_scroll, 0, max(0, (int)m_viewIdx.size() - 4));
        updateCount();
    }

    void loadTasks() {
        m_tasks.clear();
        if (!g_sys.sdMounted && !g_sys.spiffsMounted) {
            updateCount();
            return;
        }
        fs::FS& fs = activeFS();
        if (!fs.exists(kTasksPath)) {
            updateCount();
            return;
        }
        File f = fs.open(kTasksPath, FILE_READ);
        if (!f) {
            updateCount();
            return;
        }
        while (f.available() && m_tasks.size() < 80) {
            String ln = f.readStringUntil('\n');
            ln.trim();
            if (ln.isEmpty()) continue;
            int p1 = ln.indexOf('|');
            int p2 = (p1 >= 0) ? ln.indexOf('|', p1 + 1) : -1;
            if (p1 > 0 && p2 > p1) {
                TaskItem it;
                it.done = (ln.substring(0, p1).toInt() != 0);
                it.prio = (ln.substring(p1 + 1, p2).toInt() != 0);
                it.text = ln.substring(p2 + 1);
                if (!it.text.isEmpty()) m_tasks.push_back(it);
            }
        }
        f.close();
        updateCount();
    }

    void saveTasks() {
        updateCount();
        if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
        fs::FS& fs = activeFS();
        File f = fs.open(kTasksPath, FILE_WRITE);
        if (!f) return;
        for (const auto& t : m_tasks) {
            f.printf("%d|%d|%s\n", t.done ? 1 : 0, t.prio ? 1 : 0, t.text.c_str());
        }
        f.close();
    }
};

} // namespace

App* createTasksApp() { return new TasksApp(); }
