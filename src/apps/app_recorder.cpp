#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <vector>
#include <algorithm>

extern void switchApp(AppId id);

namespace {

static constexpr uint32_t kRecRate = 16000;
static constexpr size_t   kChunkSamples = 512;

static int16_t s_micBuf[2][kChunkSamples];
static volatile int s_doneBufIdx = -1;
static int s_nextQueueIdx = 0;

static void onMicBufferDone(void* args, void* data, size_t len) {
    (void)args;
    (void)len;
    if (data == s_micBuf[0]) s_doneBufIdx = 0;
    else if (data == s_micBuf[1]) s_doneBufIdx = 1;
}

static void writeWavHeader(File& f, uint32_t dataBytes, uint32_t rate) {
    uint8_t h[44];
    uint32_t fileSizeMinus8 = dataBytes + 36;
    uint32_t byteRate = rate * 2;
    memcpy(h + 0, "RIFF", 4);
    h[4] = fileSizeMinus8 & 0xFF; h[5] = (fileSizeMinus8 >> 8) & 0xFF;
    h[6] = (fileSizeMinus8 >> 16) & 0xFF; h[7] = (fileSizeMinus8 >> 24) & 0xFF;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0; // PCM
    h[20] = 1;  h[21] = 0;                      // Linear PCM
    h[22] = 1;  h[23] = 0;                      // Mono
    h[24] = rate & 0xFF; h[25] = (rate >> 8) & 0xFF;
    h[26] = (rate >> 16) & 0xFF; h[27] = (rate >> 24) & 0xFF;
    h[28] = byteRate & 0xFF; h[29] = (byteRate >> 8) & 0xFF;
    h[30] = (byteRate >> 16) & 0xFF; h[31] = (byteRate >> 24) & 0xFF;
    h[32] = 2; h[33] = 0;                       // Block align
    h[34] = 16; h[35] = 0;                      // Bits per sample
    memcpy(h + 36, "data", 4);
    h[40] = dataBytes & 0xFF; h[41] = (dataBytes >> 8) & 0xFF;
    h[42] = (dataBytes >> 16) & 0xFF; h[43] = (dataBytes >> 24) & 0xFF;
    f.seek(0);
    f.write(h, 44);
}

class RecorderApp : public App {
public:
    AppId id() const override { return APP_RECORDER; }
    const char* name() const override { return "Voice"; }

    void onEnter() override {
        m_sel = 0;
        m_scroll = 0;
        refreshFiles();
    }

    void onExit() override {
        stopRecording();
        stopPlayback();
        std::vector<FileEntry>().swap(m_files);
    }

    void onTick() override {
        if (m_recording) {
            int done = s_doneBufIdx;
            if (done >= 0) {
                s_doneBufIdx = -1;
                if (m_recFile) {
                    m_recFile.write((const uint8_t*)s_micBuf[done], kChunkSamples * sizeof(int16_t));
                    m_recBytes += (uint32_t)(kChunkSamples * sizeof(int16_t));
                }
                // Compute peak for level meter
                uint32_t acc = 0;
                for (size_t i = 0; i < kChunkSamples; i += 8) {
                    int32_t v = s_micBuf[done][i];
                    acc += (uint32_t)(v < 0 ? -v : v);
                }
                m_level = (uint16_t)(acc / (kChunkSamples / 8));
                // Re-queue finished buffer
                M5Cardputer.Mic.record(s_micBuf[done], kChunkSamples, kRecRate, false);
            }
            if (M5Cardputer.Mic.isRecording() < 2) {
                // Keep queue topped up
                int next = s_nextQueueIdx;
                if (M5Cardputer.Mic.record(s_micBuf[next], kChunkSamples, kRecRate, false)) {
                    s_nextQueueIdx = (next + 1) % 2;
                }
            }
        } else if (m_playing && m_playFile) {
            while (M5Cardputer.Speaker.isPlaying(0) < 2) {
                int rd = m_playFile.read((uint8_t*)m_playBuf[m_playTriIdx], kChunkSamples * sizeof(int16_t));
                if (rd <= 0) {
                    stopPlayback();
                    break;
                }
                size_t samples = (size_t)rd / sizeof(int16_t);
                uint32_t acc = 0;
                for (size_t i = 0; i < samples; i += 8) {
                    int32_t v = m_playBuf[m_playTriIdx][i];
                    acc += (uint32_t)(v < 0 ? -v : v);
                }
                m_level = samples > 0 ? (uint16_t)(acc / max<size_t>(1, samples / 8)) : 0;
                M5Cardputer.Speaker.setVolume(g_settings.volume);
                M5Cardputer.Speaker.playRaw(m_playBuf[m_playTriIdx], samples, m_playRate, false, 1, 0, false);
                m_playTriIdx = (m_playTriIdx + 1) % 3;
            }
        } else {
            m_level = 0;
        }
    }

    void onInput(const InputEvent& ev) override {
        if (isNavBack(ev)) {
            if (m_recording) { stopRecording(); return; }
            if (m_playing) { stopPlayback(); return; }
            switchApp(APP_HOME);
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
            if (m_recording) stopRecording();
            else startRecording();
            return;
        }
        if (m_recording) return;

        if (isNavUp(ev) && m_sel > 0) {
            m_sel--;
            if (m_sel < m_scroll) m_scroll = m_sel;
        } else if (isNavDown(ev) && m_sel < (int)m_files.size() - 1) {
            m_sel++;
            if (m_sel >= m_scroll + 4) m_scroll = m_sel - 3;
        } else if ((ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == ' ')) && !m_files.empty()) {
            if (m_playing) stopPlayback();
            else startPlayback(m_files[m_sel].path);
        } else if (ev.code == CK_CHAR && (ev.ch == 'x' || ev.ch == 'X' || ev.ch == 'd' || ev.ch == 'D') && !m_files.empty()) {
            stopPlayback();
            SD.remove(m_files[m_sel].path.c_str());
            refreshFiles();
            showToast("Recording deleted", 1200);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Voice Recorder", true);

        // Live meter card (y: 18..54)
        spr.fillRoundRect(6, 18, SCREEN_W - 12, 36, 5, th.surface);
        spr.drawRoundRect(6, 18, SCREEN_W - 12, 36, 5, m_recording ? th.err : (m_playing ? th.ok : th.border));
        spr.setTextSize(1);

        if (m_recording) {
            uint32_t sec = (millis() - m_recStartMs) / 1000;
            char tb[32];
            snprintf(tb, sizeof(tb), "REC %02u:%02u (%uKB)", (unsigned)(sec / 60), (unsigned)(sec % 60),
                     (unsigned)(m_recBytes / 1024));
            spr.fillCircle(14, 27, 3, th.err);
            spr.setTextColor(th.err);
            spr.drawString(tb, 22, 23);
        } else if (m_playing && !m_files.empty()) {
            spr.setTextColor(th.ok);
            String nm = m_files[m_sel].name;
            if ((int)nm.length() > 26) nm = nm.substring(0, 25) + "~";
            spr.drawString("PLAY: " + nm, 12, 23);
        } else {
            spr.setTextColor(g_sys.sdMounted ? th.textDim : th.warn);
            spr.drawString(g_sys.sdMounted ? "Ready (16kHz WAV on SD)" : "Insert SD card to record", 12, 23);
        }

        // Animated audio meter bars
        int barFill = min(14, (int)(m_level / 350));
        for (int i = 0; i < 14; ++i) {
            int bx = 12 + i * 15;
            bool lit = (i < barFill);
            uint16_t col = lit ? (i > 10 ? th.err : (i > 6 ? th.warn : th.ok)) : th.surface2;
            spr.fillRect(bx, 38, 11, 10, col);
        }

        // Recordings list (y: 57..120)
        if (m_files.empty()) {
            spr.setTextColor(th.textDim);
            spr.drawString("No recordings in /recordings", 12, 74);
            spr.drawString("Press [R] to start recording!", 12, 88);
        } else {
            for (int r = 0; r < 4; ++r) {
                int idx = m_scroll + r;
                if (idx >= (int)m_files.size()) break;
                int y = 57 + r * 16;
                bool sel = (idx == m_sel);
                spr.fillRoundRect(6, y, SCREEN_W - 16, 14, 3, sel ? th.surface2 : th.surface);
                if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 14, 3, acc);
                String nm = m_files[idx].name;
                if ((int)nm.length() > 22) nm = nm.substring(0, 21) + "~";
                spr.setTextColor(sel ? acc : th.text);
                spr.drawString(nm, 11, y + 3);
                spr.setTextDatum(TR_DATUM);
                spr.setTextColor(th.textDim);
                spr.drawString(formatBytes(m_files[idx].size), SCREEN_W - 14, y + 3);
                spr.setTextDatum(TL_DATUM);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, 57, 62, (int)m_files.size(), 4, m_scroll);
        }

        spr.setTextColor(th.textDim);
        spr.drawString(m_recording ? "[R/ESC] Stop & Save" : "[R]Rec [Ent]Play [X]Del [ESC]Home", 6, 125);
        ui::drawToastOverlay(spr);
    }

private:
    std::vector<FileEntry> m_files;
    int m_sel = 0;
    int m_scroll = 0;
    bool m_recording = false;
    bool m_playing = false;
    File m_recFile;
    File m_playFile;
    uint32_t m_recStartMs = 0;
    uint32_t m_recBytes = 0;
    uint32_t m_playRate = kRecRate;
    uint16_t m_level = 0;
    int16_t m_playBuf[3][kChunkSamples];
    uint8_t m_playTriIdx = 0;

    void refreshFiles() {
        m_files.clear();
        if (!g_sys.sdMounted) return;
        if (!SD.exists("/recordings")) SD.mkdir("/recordings");
        auto all = listDir("/recordings");
        for (auto& f : all) {
            String lo = f.name;
            lo.toLowerCase();
            if (!f.isDir && lo.endsWith(".wav")) m_files.push_back(f);
        }
        std::sort(m_files.begin(), m_files.end(), [](const FileEntry& a, const FileEntry& b) {
            return a.name.compareTo(b.name) > 0;
        });
        if (m_sel >= (int)m_files.size()) m_sel = max(0, (int)m_files.size() - 1);
        m_scroll = constrain(m_scroll, 0, max(0, (int)m_files.size() - 4));
    }

    void startRecording() {
        if (!g_sys.sdMounted) { showToast("SD card required!", 1500); return; }
        stopPlayback();
        stopMusicPlayback();
        M5Cardputer.Speaker.end();

        if (!SD.exists("/recordings")) SD.mkdir("/recordings");
        int n = 1;
        String path;
        do {
            char b[36];
            snprintf(b, sizeof(b), "/recordings/rec_%03d.wav", n++);
            path = b;
        } while (SD.exists(path.c_str()) && n < 999);

        m_recFile = SD.open(path.c_str(), FILE_WRITE);
        if (!m_recFile) {
            M5Cardputer.Speaker.begin();
            showToast("Failed to open WAV", 1500);
            return;
        }
        writeWavHeader(m_recFile, 0, kRecRate);
        m_recBytes = 0;
        m_recStartMs = millis();

        auto cfg = M5Cardputer.Mic.config();
        cfg.sample_rate = kRecRate;
        M5Cardputer.Mic.config(cfg);
        s_doneBufIdx = -1;
        s_nextQueueIdx = 0;
        M5Cardputer.Mic.setBufferReleaseCallback(nullptr, onMicBufferDone);
        if (!M5Cardputer.Mic.begin()) {
            m_recFile.close();
            SD.remove(path.c_str());
            M5Cardputer.Speaker.begin();
            showToast("Mic init failed", 1500);
            return;
        }
        M5Cardputer.Mic.record(s_micBuf[0], kChunkSamples, kRecRate, false);
        M5Cardputer.Mic.record(s_micBuf[1], kChunkSamples, kRecRate, false);
        m_recording = true;
    }

    void stopRecording() {
        if (!m_recording) return;
        m_recording = false;
        M5Cardputer.Mic.end();
        M5Cardputer.Mic.setBufferReleaseCallback(nullptr, nullptr);
        if (m_recFile) {
            writeWavHeader(m_recFile, m_recBytes, kRecRate);
            m_recFile.close();
        }
        M5Cardputer.Speaker.begin();
        refreshFiles();
        showToast("Saved recording!", 1400);
    }

    void startPlayback(const String& path) {
        if (!g_sys.sdMounted) return;
        stopMusicPlayback();
        M5Cardputer.Mic.end();
        M5Cardputer.Speaker.begin();

        m_playFile = SD.open(path.c_str(), FILE_READ);
        if (!m_playFile || m_playFile.size() <= 44) {
            if (m_playFile) m_playFile.close();
            showToast("Empty WAV file", 1400);
            return;
        }
        uint8_t hdr[44];
        m_playFile.read(hdr, 44);
        uint32_t rate = (uint32_t)hdr[24] | ((uint32_t)hdr[25] << 8) |
                        ((uint32_t)hdr[26] << 16) | ((uint32_t)hdr[27] << 24);
        if (rate < 8000 || rate > 48000) rate = kRecRate;
        m_playRate = rate;
        m_playTriIdx = 0;
        m_playing = true;
    }

    void stopPlayback() {
        if (!m_playing) return;
        m_playing = false;
        M5Cardputer.Speaker.stop(0);
        if (m_playFile) m_playFile.close();
    }
};

} // namespace

App* createRecorderApp() { return new RecorderApp(); }

