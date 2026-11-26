#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"

extern void switchApp(AppId id);

namespace {

class FlappyApp : public App {
public:
    AppId id() const override { return APP_FLAPPY; }
    const char* name() const override { return "Flappy"; }

    void onEnter() override {
        resetGame();
    }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        bool flap = (ev.code == CK_UP || ev.code == CK_ENTER ||
                     (ev.code == CK_CHAR && (ev.ch == ' ' || ev.ch == 'w' || ev.ch == 'W' || ev.ch == ';')));
        if (!flap) return;

        if (m_gameOver) {
            resetGame();
            m_started = true;
            m_velY = -3.1f;
        } else {
            m_started = true;
            m_velY = -3.1f;
        }
    }

    void onTick() override {
        uint32_t now = millis();
        if (now - m_lastTickMs < 24) return;
        m_lastTickMs = now;

        if (!m_started || m_gameOver) return;

        m_velY += 0.24f;
        if (m_velY > 4.2f) m_velY = 4.2f;
        m_birdY += m_velY;

        if (m_birdY < 20.0f) m_birdY = 20.0f;
        if (m_birdY > SCREEN_H - 10.0f) {
            endGame();
            return;
        }

        const int pipeW = 18;
        const int gapH = 38;
        const float birdX = 48.0f;
        for (int i = 0; i < 2; ++i) {
            m_pipes[i].x -= 2.2f;
            if (m_pipes[i].x + pipeW < 0) {
                int other = 1 - i;
                m_pipes[i].x = m_pipes[other].x + 128.0f;
                m_pipes[i].gapY = 38 + (int)(esp_random() % 44);
                m_pipes[i].scored = false;
            }
            if (!m_pipes[i].scored && m_pipes[i].x + pipeW < birdX) {
                m_pipes[i].scored = true;
                m_score++;
                if (m_score > (int)g_settings.flappyHighScore) {
                    g_settings.flappyHighScore = (uint16_t)m_score;
                }
            }
            if (birdX + 5 > m_pipes[i].x && birdX - 5 < m_pipes[i].x + pipeW) {
                if (m_birdY - 5 < m_pipes[i].gapY || m_birdY + 5 > m_pipes[i].gapY + gapH) {
                    endGame();
                    return;
                }
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Flappy Bird");

        const int pipeW = 18;
        const int gapH = 38;
        for (int i = 0; i < 2; ++i) {
            int px = (int)m_pipes[i].x;
            int gy = m_pipes[i].gapY;
            spr.fillRect(px, 16, pipeW, max(0, gy - 16), th.ok);
            spr.fillRect(px, gy + gapH, pipeW, SCREEN_H - (gy + gapH) - 6, th.ok);
        }

        // Ground strip
        spr.fillRect(0, SCREEN_H - 6, SCREEN_W, 6, th.border);

        // Bird
        int bx = 48, by = (int)m_birdY;
        spr.fillCircle(bx, by, 5, rgb565(250, 204, 21));
        spr.fillCircle(bx + 2, by - 2, 1, TFT_BLACK);
        spr.fillTriangle(bx + 4, by - 1, bx + 8, by + 1, bx + 4, by + 2, rgb565(251, 146, 60));

        // Live Score
        spr.setTextSize(1);
        spr.setTextColor(th.text);
        char sb[32];
        snprintf(sb, sizeof(sb), "Score: %d  Best: %u", m_score, (unsigned)g_settings.flappyHighScore);
        spr.drawString(sb, 8, 20);

        if (!m_started || m_gameOver) {
            spr.fillRoundRect(42, 54, SCREEN_W - 84, 42, 5, th.surface2);
            spr.drawRoundRect(42, 54, SCREEN_W - 84, 42, 5, m_gameOver ? th.err : acc);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(m_gameOver ? th.err : acc);
            spr.drawString(m_gameOver ? "CRASHED!" : "FLAPPY BIRD", SCREEN_W / 2, 67);
            spr.setTextColor(th.text);
            spr.drawString("Space / Enter to Flap", SCREEN_W / 2, 83);
            spr.setTextDatum(TL_DATUM);
        }
    }

private:
    struct Pipe { float x; int gapY; bool scored; };
    Pipe m_pipes[2];
    float m_birdY = 68.0f;
    float m_velY = 0.0f;
    int m_score = 0;
    bool m_started = false;
    bool m_gameOver = false;
    uint32_t m_lastTickMs = 0;

    void resetGame() {
        m_birdY = 68.0f;
        m_velY = 0.0f;
        m_score = 0;
        m_started = false;
        m_gameOver = false;
        m_lastTickMs = millis();
        m_pipes[0] = {150.0f, 48, false};
        m_pipes[1] = {278.0f, 64, false};
    }

    void endGame() {
        m_gameOver = true;
        if (m_score >= (int)g_settings.flappyHighScore) {
            saveSettings();
        }
    }
};

} // namespace

App* createFlappyApp() { return new FlappyApp(); }

