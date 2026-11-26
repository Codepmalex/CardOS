#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include <vector>

extern void switchApp(AppId id);

namespace {

class SnakeApp : public App {
public:
    AppId id() const override { return APP_SNAKE; }
    const char* name() const override { return "Snake"; }

    void onEnter() override {
        resetGame();
    }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }

        if (m_gameOver) {
            if (ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == ' ' || ev.ch == 'r' || ev.ch == 'R'))) {
                resetGame();
            }
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P' || ev.ch == ' ')) {
            m_paused = !m_paused;
            return;
        }

        int ndx = m_dx, ndy = m_dy;
        if (isNavUp(ev) || (ev.code == CK_CHAR && (ev.ch == 'w' || ev.ch == 'W'))) {
            if (m_dy != 1) { ndx = 0; ndy = -1; }
        } else if (isNavDown(ev) || (ev.code == CK_CHAR && (ev.ch == 's' || ev.ch == 'S'))) {
            if (m_dy != -1) { ndx = 0; ndy = 1; }
        } else if (isNavLeft(ev) || (ev.code == CK_CHAR && (ev.ch == 'a' || ev.ch == 'A'))) {
            if (m_dx != 1) { ndx = -1; ndy = 0; }
        } else if (isNavRight(ev) || (ev.code == CK_CHAR && (ev.ch == 'd' || ev.ch == 'D'))) {
            if (m_dx != -1) { ndx = 1; ndy = 0; }
        }
        m_nextDx = ndx;
        m_nextDy = ndy;
    }

    void onTick() override {
        if (m_gameOver || m_paused) return;
        uint32_t interval = (uint32_t)max(65, 145 - m_score * 3);
        uint32_t now = millis();
        if (now - m_lastStepMs < interval) return;
        m_lastStepMs = now;

        m_dx = m_nextDx;
        m_dy = m_nextDy;

        int nx = m_body[0].x + m_dx;
        int ny = m_body[0].y + m_dy;

        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
            triggerGameOver();
            return;
        }
        for (size_t i = 0; i < m_body.size(); ++i) {
            if (m_body[i].x == nx && m_body[i].y == ny) {
                triggerGameOver();
                return;
            }
        }

        m_body.insert(m_body.begin(), {nx, ny});
        if (nx == m_foodX && ny == m_foodY) {
            m_score++;
            if (m_score > (int)g_settings.snakeHighScore) {
                g_settings.snakeHighScore = (uint16_t)m_score;
            }
            spawnFood();
        } else {
            m_body.pop_back();
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Snake");

        // Score strip
        spr.setTextSize(1);
        spr.setTextColor(acc);
        char sb[36];
        snprintf(sb, sizeof(sb), "Score: %d   Best: %u", m_score, (unsigned)g_settings.snakeHighScore);
        spr.drawString(sb, 8, 18);

        // Arena: x=8..224 (24 cols * 9 = 216), y=29..128 (11 rows * 9 = 99)
        const int ox = 12, oy = 28, cell = 9;
        spr.fillRect(ox, oy, COLS * cell, ROWS * cell, th.surface);
        spr.drawRect(ox - 1, oy - 1, COLS * cell + 2, ROWS * cell + 2, th.border);

        // Food
        spr.fillCircle(ox + m_foodX * cell + cell / 2, oy + m_foodY * cell + cell / 2, 3, th.err);

        // Snake segments
        for (size_t i = 0; i < m_body.size(); ++i) {
            int sx = ox + m_body[i].x * cell + 1;
            int sy = oy + m_body[i].y * cell + 1;
            spr.fillRoundRect(sx, sy, cell - 2, cell - 2, 2, (i == 0) ? th.ok : acc);
        }

        if (m_paused || m_gameOver) {
            spr.fillRoundRect(46, 56, SCREEN_W - 92, 42, 5, th.surface2);
            spr.drawRoundRect(46, 56, SCREEN_W - 92, 42, 5, m_gameOver ? th.err : acc);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(m_gameOver ? th.err : acc);
            spr.drawString(m_gameOver ? "GAME OVER" : "PAUSED", SCREEN_W / 2, 69);
            spr.setTextColor(th.text);
            spr.drawString(m_gameOver ? "Enter/Space: Retry" : "Space/P: Resume", SCREEN_W / 2, 85);
            spr.setTextDatum(TL_DATUM);
        }
    }

private:
    static constexpr int COLS = 24;
    static constexpr int ROWS = 11;
    struct Pt { int x; int y; };
    std::vector<Pt> m_body;
    int m_dx = 1, m_dy = 0;
    int m_nextDx = 1, m_nextDy = 0;
    int m_foodX = 16, m_foodY = 5;
    int m_score = 0;
    bool m_paused = false;
    bool m_gameOver = false;
    uint32_t m_lastStepMs = 0;

    void resetGame() {
        m_body.clear();
        m_body.push_back({10, 5});
        m_body.push_back({9, 5});
        m_body.push_back({8, 5});
        m_dx = m_nextDx = 1;
        m_dy = m_nextDy = 0;
        m_score = 0;
        m_paused = false;
        m_gameOver = false;
        m_lastStepMs = millis();
        spawnFood();
    }

    void spawnFood() {
        for (int tries = 0; tries < 40; ++tries) {
            int fx = (int)(esp_random() % COLS);
            int fy = (int)(esp_random() % ROWS);
            bool hit = false;
            for (const auto& p : m_body) {
                if (p.x == fx && p.y == fy) { hit = true; break; }
            }
            if (!hit) { m_foodX = fx; m_foodY = fy; return; }
        }
        m_foodX = 0; m_foodY = 0;
    }

    void triggerGameOver() {
        m_gameOver = true;
        if (m_score >= (int)g_settings.snakeHighScore) {
            saveSettings();
        }
    }
};

} // namespace

App* createSnakeApp() { return new SnakeApp(); }

