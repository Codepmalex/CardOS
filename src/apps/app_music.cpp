#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../input.h"
#include "../storage.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <algorithm>

#include <AudioFileSourceSD.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>

extern void switchApp(AppId id);

namespace {

// ---------------------------------------------------------------------------
// Custom Mono M5Speaker AudioOutput for ESP8266Audio
// Note: Cardputer ADV ES8311 codec drives internal speaker and auto-switches
// in hardware to the 3.5mm earphone/AUX jack when a cable is connected.
// ---------------------------------------------------------------------------
class AudioOutputM5Speaker : public AudioOutput {
public:
    explicit AudioOutputM5Speaker(uint8_t channel = 0) : m_ch(channel) {
        SetRate(44100);
        SetChannels(2);
    }
    ~AudioOutputM5Speaker() override { stop(); }

    bool begin() override {
        m_bufPos = 0;
        m_triIdx = 0;
        M5Cardputer.Speaker.begin();
        M5Cardputer.Speaker.setVolume(g_settings.mediaVolume);
        return true;
    }

    bool ConsumeSample(int16_t sample[2]) override {
        if (m_bufPos >= kChunkSamples) {
            if (!flushCurrentBuffer()) return false;
        }
        int32_t mono = ((int32_t)sample[LEFTCHANNEL] + (int32_t)sample[RIGHTCHANNEL]) / 2;
        mono = Amplify(mono);
        if (mono > 32767) mono = 32767;
        else if (mono < -32768) mono = -32768;
        m_triBuf[m_triIdx][m_bufPos++] = (int16_t)mono;

        if (m_bufPos >= kChunkSamples) {
            flushCurrentBuffer();
        }
        return true;
    }

    bool stop() override {
        m_bufPos = 0;
        M5Cardputer.Speaker.stop(m_ch);
        return true;
    }

    void flush() override {
        if (m_bufPos > 0 && M5Cardputer.Speaker.isPlaying(m_ch) < 2) {
            for (size_t i = m_bufPos; i < kChunkSamples; ++i) m_triBuf[m_triIdx][i] = 0;
            flushCurrentBuffer();
        }
    }

    uint16_t recentLevel() const { return m_level; }

private:
    static constexpr size_t kChunkSamples = 512;
    uint8_t m_ch = 0;
    int16_t m_triBuf[3][kChunkSamples];
    size_t m_bufPos = 0;
    uint8_t m_triIdx = 0;
    uint16_t m_level = 0;

    bool flushCurrentBuffer() {
        if (M5Cardputer.Speaker.isPlaying(m_ch) >= 2) return false;
        uint32_t acc = 0;
        for (size_t i = 0; i < kChunkSamples; i += 8) {
            int32_t v = m_triBuf[m_triIdx][i];
            acc += (uint32_t)(v < 0 ? -v : v);
        }
        m_level = (uint16_t)(acc / (kChunkSamples / 8));
        M5Cardputer.Speaker.setVolume(g_settings.mediaVolume);
        M5Cardputer.Speaker.playRaw(m_triBuf[m_triIdx], kChunkSamples,
                                    (uint32_t)(hertz > 0 ? hertz : 44100),
                                    false, 1, (int)m_ch, false);
        m_triIdx = (m_triIdx + 1) % 3;
        m_bufPos = 0;
        return true;
    }
};

struct MusicTrack {
    String name;
    String path;
    String playlist;
};

struct MusicPlaylist {
    String name;
    std::vector<int> trackIndices;
};

// Shared background player engine state
static std::vector<MusicTrack>    s_tracks;
static std::vector<MusicPlaylist> s_playlists;
static int  s_curPlaylist = 0;
static int  s_curPosInPlaylist = 0; // Index inside s_playlists[s_curPlaylist].trackIndices
static bool s_scanned = false;

static AudioGeneratorMP3*        s_mp3 = nullptr;
static AudioFileSourceSD*        s_sdSrc = nullptr;
static AudioFileSourceHTTPStream* s_httpSrc = nullptr;
static AudioFileSourceBuffer*    s_bufSrc = nullptr;
static AudioOutputM5Speaker*     s_out = nullptr;

// Wi-Fi Phone Stream / Upload WebServer
static WebServer* s_server = nullptr;
static File       s_uploadFile;
static String     s_uploadPlaylist = "phone";
static String     s_lastPhoneStatus = "";

static void updateMusicLiveState(const String& track, const String& pl, bool playing, bool paused, bool streaming) {
    g_sys.music.playing = playing;
    g_sys.music.paused = paused;
    g_sys.music.streaming = streaming;
    strlcpy(g_sys.music.trackName, track.c_str(), sizeof(g_sys.music.trackName));
    strlcpy(g_sys.music.playlist, pl.c_str(), sizeof(g_sys.music.playlist));
}

static void cleanupDecoder() {
    if (s_mp3) {
        if (s_mp3->isRunning()) s_mp3->stop();
        delete s_mp3;
        s_mp3 = nullptr;
    }
    if (s_bufSrc) {
        s_bufSrc->close();
        delete s_bufSrc;
        s_bufSrc = nullptr;
    }
    if (s_sdSrc) {
        s_sdSrc->close();
        delete s_sdSrc;
        s_sdSrc = nullptr;
    }
    if (s_httpSrc) {
        s_httpSrc->close();
        delete s_httpSrc;
        s_httpSrc = nullptr;
    }
    if (s_out) {
        s_out->stop();
        delete s_out;
        s_out = nullptr;
    }
}

static String stripMp3Ext(const String& fn) {
    if (fn.endsWith(".mp3") || fn.endsWith(".MP3")) return fn.substring(0, fn.length() - 4);
    return fn;
}

static void scanDirRecursive(const String& dirPath, const String& topPlaylist, int depth) {
    if (depth > 4 || s_tracks.size() >= 180) return;
    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) return;

    File entry = dir.openNextFile();
    while (entry && s_tracks.size() < 180) {
        String name = String(entry.name());
        int sl = name.lastIndexOf('/');
        String base = (sl >= 0) ? name.substring(sl + 1) : name;
        String fullPath = dirPath + "/" + base;
        if (base.startsWith(".")) {
            entry.close();
            entry = dir.openNextFile();
            continue;
        }
        if (entry.isDirectory()) {
            String nextPl = topPlaylist.isEmpty() ? base : topPlaylist;
            entry.close();
            scanDirRecursive(fullPath, nextPl, depth + 1);
        } else {
            String lower = base;
            lower.toLowerCase();
            if (lower.endsWith(".mp3")) {
                MusicTrack t;
                t.name = stripMp3Ext(base);
                t.path = fullPath;
                t.playlist = topPlaylist.isEmpty() ? "Unsorted" : topPlaylist;
                s_tracks.push_back(t);
            }
            entry.close();
        }
        entry = dir.openNextFile();
    }
    dir.close();
}

static void scanMusicLibrary() {
    s_tracks.clear();
    s_playlists.clear();
    s_scanned = true;

    MusicPlaylist all;
    all.name = "All Songs";
    s_playlists.push_back(all);

    if (!g_sys.sdMounted) return;
    if (!SD.exists("/music")) SD.mkdir("/music");
    scanDirRecursive("/music", "", 0);

    std::sort(s_tracks.begin(), s_tracks.end(), [](const MusicTrack& a, const MusicTrack& b) {
        return a.name.compareTo(b.name) < 0;
    });

    for (int i = 0; i < (int)s_tracks.size(); ++i) {
        s_playlists[0].trackIndices.push_back(i);
        const String& plName = s_tracks[i].playlist;
        if (plName == "Unsorted") continue;
        int found = -1;
        for (int p = 1; p < (int)s_playlists.size(); ++p) {
            if (s_playlists[p].name == plName) { found = p; break; }
        }
        if (found < 0) {
            MusicPlaylist np;
            np.name = plName;
            np.trackIndices.push_back(i);
            s_playlists.push_back(np);
        } else {
            s_playlists[found].trackIndices.push_back(i);
        }
    }

    if (s_curPlaylist >= (int)s_playlists.size()) s_curPlaylist = 0;
    if (s_curPosInPlaylist >= (int)s_playlists[s_curPlaylist].trackIndices.size()) s_curPosInPlaylist = 0;
}

static bool startTrackFromPlaylist(int plIdx, int posIdx) {
    if (!g_sys.sdMounted) { showToast("Insert SD card!", 1400); return false; }
    if (plIdx < 0 || plIdx >= (int)s_playlists.size()) return false;
    auto& pl = s_playlists[plIdx];
    if (pl.trackIndices.empty()) return false;
    if (posIdx < 0 || posIdx >= (int)pl.trackIndices.size()) posIdx = 0;

    int tid = pl.trackIndices[posIdx];
    const auto& tr = s_tracks[tid];

    cleanupDecoder();
    s_out = new AudioOutputM5Speaker(0);
    s_sdSrc = new AudioFileSourceSD(tr.path.c_str());
    s_bufSrc = new AudioFileSourceBuffer(s_sdSrc, 4096);
    s_mp3 = new AudioGeneratorMP3();

    if (!s_mp3->begin(s_bufSrc, s_out)) {
        cleanupDecoder();
        updateMusicLiveState("", "", false, false, false);
        showToast("Bad MP3 file", 1400);
        return false;
    }

    s_curPlaylist = plIdx;
    s_curPosInPlaylist = posIdx;
    updateMusicLiveState(tr.name, pl.name, true, false, false);
    return true;
}

static bool startHttpStreamUrl(const String& url, const String& title) {
    if (WiFi.status() != WL_CONNECTED) { showToast("Connect Wi-Fi first!", 1400); return false; }
    if (!url.startsWith("http://")) {
        showToast("Use http:// stream URL", 1600);
        return false;
    }

    cleanupDecoder();
    s_out = new AudioOutputM5Speaker(0);
    s_httpSrc = new AudioFileSourceHTTPStream(url.c_str());
    s_httpSrc->SetReconnect(2, 200);
    s_bufSrc = new AudioFileSourceBuffer(s_httpSrc, 8192);
    s_mp3 = new AudioGeneratorMP3();

    if (!s_mp3->begin(s_bufSrc, s_out)) {
        cleanupDecoder();
        updateMusicLiveState("", "", false, false, false);
        showToast("Stream failed!", 1500);
        return false;
    }

    String lbl = title.length() > 0 ? title : "Phone Stream";
    updateMusicLiveState(lbl, "Wi-Fi Stream", true, false, true);
    s_lastPhoneStatus = "Streaming from phone";
    return true;
}

static void playNextTrack(int dir) {
    if (s_playlists.empty()) return;
    auto& pl = s_playlists[s_curPlaylist];
    if (pl.trackIndices.empty()) return;
    int n = (int)pl.trackIndices.size();
    int nextPos = s_curPosInPlaylist;
    if (g_sys.music.shuffle && n > 1) {
        int tries = 0;
        do {
            nextPos = (int)(esp_random() % (uint32_t)n);
            tries++;
        } while (nextPos == s_curPosInPlaylist && tries < 6);
    } else {
        nextPos = (s_curPosInPlaylist + dir + n) % n;
    }
    startTrackFromPlaylist(s_curPlaylist, nextPos);
}

static void togglePlayPause() {
    if (!g_sys.music.playing) {
        if (!s_scanned) scanMusicLibrary();
        if (!s_playlists[s_curPlaylist].trackIndices.empty()) {
            startTrackFromPlaylist(s_curPlaylist, s_curPosInPlaylist);
        } else {
            showToast("No MP3s in /music", 1400);
        }
        return;
    }
    g_sys.music.paused = !g_sys.music.paused;
    if (g_sys.music.paused && s_out) {
        s_out->stop();
    }
}

static const char PHONE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CardOS Music</title>
<style>
body{background:#0b111c;color:#e2e8f0;font-family:system-ui,sans-serif;margin:0;padding:14px}
.card{background:#152033;border:1px solid #263854;border-radius:12px;padding:14px;margin-bottom:12px}
h2{margin:0 0 8px;color:#38bdf8;font-size:18px}
button{background:#38bdf8;color:#08101c;border:0;border-radius:8px;padding:10px 14px;font-weight:700;margin:3px}
input{width:100%;box-sizing:border-box;padding:9px;border-radius:8px;border:1px solid #2d4363;background:#0e1726;color:#fff;margin:6px 0}
.row{display:flex;gap:6px;flex-wrap:wrap}
</style></head><body>
<div class="card"><h2>CardOS Music & AUX</h2>
<div id="now">Loading...</div>
<div class="row" style="margin-top:8px">
<button onclick="cmd('prev')">Prev</button>
<button onclick="cmd('play')">Play/Pause</button>
<button onclick="cmd('next')">Next</button>
<button onclick="cmd('shf')">Shuffle</button>
</div></div>
<div class="card"><h2>Stream Phone HTTP Audio</h2>
<input id="surl" placeholder="http://192.168.1.x:8000/song.mp3">
<button onclick="goStream()">Stream to Speaker / AUX</button>
</div>
<div class="card"><h2>Send MP3 to Playlist Folder</h2>
<form id="uf" method="POST" action="/upload" enctype="multipart/form-data">
<input name="pl" placeholder="Playlist folder (e.g. playlist1)" value="playlist1">
<input type="file" name="mp3" accept=".mp3,audio/mpeg" required>
<button type="submit">Upload to Cardputer</button>
</form></div>
<script>
async function st(){let r=await fetch('/api/state');let j=await r.json();
document.getElementById('now').innerText=(j.playing?(j.paused?'Paused: ':'Playing: '):'Idle - ')+j.track+' ['+j.playlist+']'+(j.shuffle?' (Shuffle)':'');}
async function cmd(c){await fetch('/api/cmd?c='+c,{method:'POST'});st();}
async function goStream(){let u=document.getElementById('surl').value;
await fetch('/api/stream?url='+encodeURIComponent(u),{method:'POST'});st();}
st();setInterval(st,2500);
</script></body></html>)rawliteral";

static String sanitizeFilename(String s) {
    int sl = s.lastIndexOf('/');
    if (sl >= 0) s = s.substring(sl + 1);
    int bs = s.lastIndexOf('\\');
    if (bs >= 0) s = s.substring(bs + 1);
    s.replace("..", "");
    s.trim();
    if (s.length() == 0) s = "track.mp3";
    return s;
}

static void startPhoneServer() {
    if (s_server) return;
    if (WiFi.status() != WL_CONNECTED) {
        showToast("Connect Wi-Fi first!", 1500);
        return;
    }
    s_server = new WebServer(8080);

    s_server->on("/", HTTP_GET, []() {
        s_server->send_P(200, "text/html", PHONE_HTML);
    });
    s_server->on("/api/state", HTTP_GET, []() {
        String j = "{\"playing\":" + String(g_sys.music.playing ? "true" : "false")
                 + ",\"paused\":" + String(g_sys.music.paused ? "true" : "false")
                 + ",\"shuffle\":" + String(g_sys.music.shuffle ? "true" : "false")
                 + ",\"track\":\"" + String(g_sys.music.trackName)
                 + "\",\"playlist\":\"" + String(g_sys.music.playlist) + "\"}";
        s_server->send(200, "application/json", j);
    });
    s_server->on("/api/cmd", HTTP_POST, []() {
        String c = s_server->arg("c");
        if (c == "play") togglePlayPause();
        else if (c == "next") playNextTrack(1);
        else if (c == "prev") playNextTrack(-1);
        else if (c == "shf") g_sys.music.shuffle = !g_sys.music.shuffle;
        s_server->send(200, "text/plain", "ok");
    });
    s_server->on("/api/stream", HTTP_POST, []() {
        String u = s_server->arg("url");
        bool ok = startHttpStreamUrl(u, "Phone Stream");
        s_server->send(ok ? 200 : 400, "text/plain", ok ? "streaming" : "failed");
    });
    s_server->on("/upload", HTTP_POST, []() {
        scanMusicLibrary();
        s_lastPhoneStatus = "Uploaded to /music/" + s_uploadPlaylist;
        s_server->sendHeader("Location", "/");
        s_server->send(303);
    }, []() {
        HTTPUpload& up = s_server->upload();
        if (up.status == UPLOAD_FILE_START) {
            if (!g_sys.sdMounted) return;
            if (s_server->hasArg("pl")) {
                s_uploadPlaylist = sanitizeFilename(s_server->arg("pl"));
                if (s_uploadPlaylist.isEmpty()) s_uploadPlaylist = "phone";
            } else {
                s_uploadPlaylist = "phone";
            }
            if (!SD.exists("/music")) SD.mkdir("/music");
            String dir = "/music/" + s_uploadPlaylist;
            if (!SD.exists(dir.c_str())) SD.mkdir(dir.c_str());
            String fn = sanitizeFilename(up.filename);
            String path = dir + "/" + fn;
            SD.remove(path.c_str());
            s_uploadFile = SD.open(path.c_str(), FILE_WRITE);
        } else if (up.status == UPLOAD_FILE_WRITE) {
            if (s_uploadFile) s_uploadFile.write(up.buf, up.currentSize);
        } else if (up.status == UPLOAD_FILE_END) {
            if (s_uploadFile) s_uploadFile.close();
        }
    });
    s_server->begin();
    g_sys.music.streaming = true;
    s_lastPhoneStatus = "Ready on :8080";
}

static void stopPhoneServer() {
    if (!s_server) return;
    s_server->stop();
    delete s_server;
    s_server = nullptr;
    if (!g_sys.music.playing) g_sys.music.streaming = false;
    s_lastPhoneStatus = "";
}

enum MusicTab : uint8_t {
    MTAB_NOW = 0,
    MTAB_PLAYLISTS,
    MTAB_SONGS,
    MTAB_PHONE
};

class MusicApp : public App {
public:
    AppId id() const override { return APP_MUSIC; }
    const char* name() const override { return "Music"; }

    void onEnter() override {
        if (!s_scanned || s_tracks.empty()) scanMusicLibrary();
        if (s_playlists.empty()) {
            MusicPlaylist all;
            all.name = "All Songs";
            s_playlists.push_back(all);
        }
        if (m_plSel >= (int)s_playlists.size()) m_plSel = 0;
        clampSongSel();
        m_tab = g_sys.music.playing ? MTAB_NOW : (s_tracks.empty() ? MTAB_PHONE : MTAB_SONGS);
    }

    void onInput(const InputEvent& ev) override {
        if (ev.code == CK_ESC) {
            switchApp(APP_HOME);
            return;
        }
        if (ev.code == CK_TAB) {
            m_tab = (MusicTab)(((int)m_tab + 1) % 4);
            return;
        }
        if (ev.code == CK_CHAR) {
            if (ev.ch == 's' || ev.ch == 'S') {
                g_sys.music.shuffle = !g_sys.music.shuffle;
                showToast(g_sys.music.shuffle ? "Shuffle ON" : "Shuffle OFF", 1200);
                return;
            }
            if (ev.ch == ' ' && m_tab == MTAB_NOW) {
                togglePlayPause();
                return;
            }
            if (ev.ch == 'r' || ev.ch == 'R') {
                scanMusicLibrary();
                clampSongSel();
                showToast(("Found " + String((int)s_tracks.size()) + " MP3(s)").c_str(), 1400);
                return;
            }
            if (ev.ch == '=' || ev.ch == '+') {
                g_settings.mediaVolume = (uint8_t)constrain((int)g_settings.mediaVolume + 20, 0, 255);
                M5Cardputer.Speaker.setVolume(g_settings.mediaVolume);
                saveSettings();
                showToast(("Media Vol: " + String((g_settings.mediaVolume * 100) / 255) + "%").c_str(), 900);
                return;
            }
            if (ev.ch == '-' || ev.ch == '_') {
                g_settings.mediaVolume = (uint8_t)constrain((int)g_settings.mediaVolume - 20, 0, 255);
                M5Cardputer.Speaker.setVolume(g_settings.mediaVolume);
                saveSettings();
                showToast(("Media Vol: " + String((g_settings.mediaVolume * 100) / 255) + "%").c_str(), 900);
                return;
            }
            if (ev.ch == 'p' || ev.ch == 'P') {
                if (s_server) { stopPhoneServer(); showToast("Phone server OFF", 1200); }
                else { startPhoneServer(); }
                return;
            }
        }

        if (m_tab == MTAB_NOW) {
            if (ev.code == CK_ENTER) togglePlayPause();
            else if (isNavLeft(ev)) playNextTrack(-1);
            else if (isNavRight(ev)) playNextTrack(1);
            else if (isNavUp(ev)) { m_tab = MTAB_SONGS; }
            else if (isNavDown(ev)) { m_tab = MTAB_PLAYLISTS; }
        } else if (m_tab == MTAB_PLAYLISTS) {
            if (isNavUp(ev) && m_plSel > 0) {
                m_plSel--;
                if (m_plSel < m_plScroll) m_plScroll = m_plSel;
            } else if (isNavDown(ev) && m_plSel < (int)s_playlists.size() - 1) {
                m_plSel++;
                if (m_plSel >= m_plScroll + 5) m_plScroll = m_plSel - 4;
            } else if (ev.code == CK_ENTER) {
                m_songSel = 0;
                m_songScroll = 0;
                m_tab = MTAB_SONGS;
            } else if (isNavRight(ev)) {
                m_tab = MTAB_SONGS;
            } else if (isNavLeft(ev)) {
                m_tab = MTAB_NOW;
            }
        } else if (m_tab == MTAB_SONGS) {
            const auto& idxs = s_playlists[m_plSel].trackIndices;
            if (isNavUp(ev) && m_songSel > 0) {
                m_songSel--;
                if (m_songSel < m_songScroll) m_songScroll = m_songSel;
            } else if (isNavDown(ev) && m_songSel < (int)idxs.size() - 1) {
                m_songSel++;
                if (m_songSel >= m_songScroll + 5) m_songScroll = m_songSel - 4;
            } else if (ev.code == CK_ENTER && !idxs.empty()) {
                if (startTrackFromPlaylist(m_plSel, m_songSel)) {
                    m_tab = MTAB_NOW;
                }
            } else if (isNavLeft(ev)) {
                m_tab = MTAB_PLAYLISTS;
            } else if (isNavRight(ev)) {
                m_tab = MTAB_PHONE;
            }
        } else if (m_tab == MTAB_PHONE) {
            if (ev.code == CK_ENTER) {
                if (s_server) stopPhoneServer();
                else startPhoneServer();
            } else if (isNavLeft(ev)) {
                m_tab = MTAB_SONGS;
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Music Player", true);

        // Top tab bar (y=17..29)
        const char* tabs[4] = {"Now", "Lists", "Songs", "Phone"};
        int tw = 56;
        for (int i = 0; i < 4; ++i) {
            int x = 6 + i * (tw + 2);
            bool act = ((int)m_tab == i);
            spr.fillRoundRect(x, 18, tw, 12, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.drawString(tabs[i], x + tw / 2, 24);
        }
        spr.setTextDatum(TL_DATUM);

        if (m_tab == MTAB_NOW) {
            renderNowPlaying(spr, th, acc);
        } else if (m_tab == MTAB_PLAYLISTS) {
            renderPlaylists(spr, th, acc);
        } else if (m_tab == MTAB_SONGS) {
            renderSongs(spr, th, acc);
        } else {
            renderPhonePair(spr, th, acc);
        }
    }

private:
    MusicTab m_tab = MTAB_NOW;
    int m_plSel = 0;
    int m_plScroll = 0;
    int m_songSel = 0;
    int m_songScroll = 0;
    uint8_t m_bars[12] = {2, 4, 6, 3, 7, 5, 4, 8, 3, 6, 5, 2};

    void clampSongSel() {
        if (m_plSel >= (int)s_playlists.size()) m_plSel = 0;
        int n = (int)s_playlists[m_plSel].trackIndices.size();
        if (m_songSel >= n) m_songSel = max(0, n - 1);
    }

    void renderNowPlaying(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.fillRoundRect(6, 34, SCREEN_W - 12, 84, 6, th.surface);
        spr.drawRoundRect(6, 34, SCREEN_W - 12, 84, 6, th.border);

        spr.setTextSize(1);
        spr.setTextColor(acc);
        String pl = g_sys.music.playlist[0] ? String(g_sys.music.playlist) : s_playlists[m_plSel].name;
        spr.drawString("Playlist: " + pl, 14, 40);

        spr.setTextColor(g_sys.music.shuffle ? th.ok : th.textDim);
        spr.setTextDatum(TR_DATUM);
        spr.drawString(g_sys.music.shuffle ? "SHUFFLE" : "SEQ", SCREEN_W - 14, 40);
        spr.setTextDatum(TL_DATUM);

        String title = g_sys.music.trackName[0] ? String(g_sys.music.trackName) : "No track playing";
        if (title.length() > 30) title = title.substring(0, 27) + "...";
        spr.setTextColor(th.text);
        spr.drawString(title, 14, 54);

        // Animated equalizer bars
        int lvl = (s_out && g_sys.music.playing && !g_sys.music.paused) ? (int)(s_out->recentLevel() / 420) : 0;
        if (lvl > 18) lvl = 18;
        for (int i = 0; i < 12; ++i) {
            int target = (g_sys.music.playing && !g_sys.music.paused)
                ? constrain(3 + lvl + (int)((esp_random() % 11)) - 4, 3, 22)
                : 3;
            m_bars[i] = (uint8_t)((m_bars[i] * 2 + target) / 3);
            int bx = 18 + i * 17;
            int bh = m_bars[i];
            uint16_t bc = (i & 1) ? acc : th.accent2;
            spr.fillRect(bx, 94 - bh, 11, bh, bc);
        }

        // Vol + Output badge (hardware auto-switches to AUX when plugged in)
        spr.setTextColor(th.textDim);
        char vb[42];
        snprintf(vb, sizeof(vb), "%s  Vol:%d%%  Out:Spk/AUX",
                 g_sys.music.playing ? (g_sys.music.paused ? "PAUSED" : "PLAYING") : "STOPPED",
                 (int)((g_settings.mediaVolume * 100) / 255));
        spr.drawString(vb, 14, 102);

        spr.drawString("[Spc]Play [</>]Skip [S]Shf [+/-]Vol", 6, 124);
    }

    void renderPlaylists(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.setTextSize(1);
        for (int r = 0; r < 5; ++r) {
            int idx = m_plScroll + r;
            if (idx >= (int)s_playlists.size()) break;
            bool sel = (idx == m_plSel);
            int y = 34 + r * 17;
            spr.fillRoundRect(6, y, SCREEN_W - 12, 15, 3, sel ? th.surface2 : th.surface);
            if (sel) spr.drawRoundRect(6, y, SCREEN_W - 12, 15, 3, acc);
            spr.setTextColor(sel ? th.text : th.textDim);
            spr.drawString(s_playlists[idx].name, 12, y + 4);
            spr.setTextDatum(TR_DATUM);
            spr.setTextColor(acc);
            spr.drawString(String((int)s_playlists[idx].trackIndices.size()) + " tracks", SCREEN_W - 12, y + 4);
            spr.setTextDatum(TL_DATUM);
        }
        spr.setTextColor(th.textDim);
        spr.drawString("[Ent] Open  [Tab] Views  [R] Rescan", 6, 124);
    }

    void renderSongs(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        const auto& idxs = s_playlists[m_plSel].trackIndices;
        spr.setTextSize(1);
        if (idxs.empty()) {
            spr.fillRoundRect(8, 40, SCREEN_W - 16, 68, 6, th.surface);
            spr.setTextColor(th.warn);
            spr.drawString("No MP3 files in /music", 16, 52);
            spr.setTextColor(th.textDim);
            spr.drawString("Put files in /music/<playlist>/song.mp3", 16, 68);
            spr.drawString("or use [Phone] tab to upload over Wi-Fi", 16, 82);
        } else {
            for (int r = 0; r < 5; ++r) {
                int pos = m_songScroll + r;
                if (pos >= (int)idxs.size()) break;
                const auto& tr = s_tracks[idxs[pos]];
                bool sel = (pos == m_songSel);
                int y = 34 + r * 17;
                spr.fillRoundRect(6, y, SCREEN_W - 12, 15, 3, sel ? th.surface2 : th.surface);
                if (sel) spr.drawRoundRect(6, y, SCREEN_W - 12, 15, 3, acc);
                String nm = tr.name;
                if (nm.length() > 26) nm = nm.substring(0, 23) + "...";
                spr.setTextColor(sel ? th.text : th.textDim);
                spr.drawString(nm, 12, y + 4);
                spr.setTextDatum(TR_DATUM);
                spr.setTextColor(th.accent2);
                String pl = tr.playlist;
                if (pl.length() > 9) pl = pl.substring(0, 9);
                spr.drawString(pl, SCREEN_W - 12, y + 4);
                spr.setTextDatum(TL_DATUM);
            }
        }
        spr.setTextColor(th.textDim);
        spr.drawString("[Ent] Play  [S] Shuffle  [R] Rescan", 6, 124);
    }

    void renderPhonePair(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.fillRoundRect(6, 34, SCREEN_W - 12, 84, 6, th.surface);
        spr.drawRoundRect(6, 34, SCREEN_W - 12, 84, 6, th.border);
        spr.setTextSize(1);
        spr.setTextColor(acc);
        spr.drawString("Phone Wi-Fi Audio Pair", 14, 41);

        if (WiFi.status() != WL_CONNECTED) {
            spr.setTextColor(th.warn);
            spr.drawString("Wi-Fi offline. Connect in Wi-Fi app!", 14, 60);
        } else if (s_server) {
            spr.setTextColor(th.ok);
            spr.drawString("Server ON - Open on phone browser:", 14, 56);
            spr.setTextColor(th.text);
            spr.drawString("http://" + WiFi.localIP().toString() + ":8080", 14, 70);
            spr.setTextColor(th.textDim);
            spr.drawString("Stream URL, upload MP3s, remote control", 14, 86);
            if (!s_lastPhoneStatus.isEmpty()) {
                spr.setTextColor(acc);
                spr.drawString(s_lastPhoneStatus, 14, 100);
            }
        } else {
            spr.setTextColor(th.text);
            spr.drawString("Press [Ent] or [P] to start pairing", 14, 60);
            spr.setTextColor(th.textDim);
            spr.drawString("Stream phone audio & send MP3 files", 14, 76);
            spr.drawString("to speaker / 3.5mm AUX jack.", 14, 90);
        }
        spr.setTextColor(th.textDim);
        spr.drawString("[Ent/P] Toggle Server   [Tab] Views", 6, 124);
    }
};

} // namespace

void tickMusicPlayer() {
    if (s_server) {
        s_server->handleClient();
    }
    if (!g_sys.music.playing || g_sys.music.paused || !s_mp3) return;

    if (s_mp3->isRunning()) {
        for (int i = 0; i < 4 && s_mp3->isRunning(); ++i) {
            if (!s_mp3->loop()) {
                if (g_sys.music.streaming && s_httpSrc) {
                    cleanupDecoder();
                    updateMusicLiveState("", "", false, false, s_server != nullptr);
                } else {
                    playNextTrack(1);
                }
                break;
            }
        }
    } else {
        if (g_sys.music.streaming && s_httpSrc) {
            cleanupDecoder();
            updateMusicLiveState("", "", false, false, s_server != nullptr);
        } else {
            playNextTrack(1);
        }
    }
}

void stopMusicPlayback() {
    cleanupDecoder();
    stopPhoneServer();
    std::vector<MusicTrack>().swap(s_tracks);
    std::vector<MusicPlaylist>().swap(s_playlists);
    s_scanned = false;
    updateMusicLiveState("", "", false, false, false);
}

App* createMusicApp() { return new MusicApp(); }
