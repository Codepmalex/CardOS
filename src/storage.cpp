#include "storage.h"
#include "config.h"
#include "state.h"
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>

static SPIClass s_sdSPI(HSPI);

bool remountSdCard() {
    SD.end();
    s_sdSPI.end();
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    delay(20);
    s_sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    static const uint32_t kSdFreqs[3] = {20000000, 10000000, 4000000};
    for (int i = 0; i < 3; ++i) {
        if (SD.begin(SD_CS_PIN, s_sdSPI, kSdFreqs[i])) {
            uint8_t cardType = SD.cardType();
            if (cardType != CARD_NONE) {
                g_sys.sdMounted = true;
                refreshSdStats();
                return true;
            }
            SD.end();
        }
        delay(35);
    }
    g_sys.sdMounted = false;
    g_sys.sdTotalMB = 0;
    g_sys.sdFreeMB = 0;
    return false;
}

bool initStorage() {
    if (SPIFFS.begin(true)) {
        g_sys.spiffsMounted = true;
    }

    remountSdCard();

    ensureDir("/apps");
    ensureDir("/notes");
    ensureDir("/calendar");
    return g_sys.sdMounted || g_sys.spiffsMounted;
}

void refreshSdStats() {
    if (!g_sys.sdMounted) return;
    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();
    g_sys.sdTotalMB = (uint32_t)(total / (1024ULL * 1024ULL));
    g_sys.sdFreeMB  = total > used ? (uint32_t)((total - used) / (1024ULL * 1024ULL)) : 0;
}

fs::FS& activeFS() {
    if (g_sys.sdMounted) return SD;
    return SPIFFS;
}

const char* activeFSName() {
    return g_sys.sdMounted ? "SD" : "SPIFFS";
}

bool ensureDir(const char* path) {
    fs::FS& fs = activeFS();
    if (fs.exists(path)) return true;
    return fs.mkdir(path);
}

std::vector<FileEntry> listDir(const char* dirPath) {
    std::vector<FileEntry> out;
    fs::FS& fs = activeFS();
    File dir = fs.open(dirPath);
    if (!dir || !dir.isDirectory()) return out;

    File f = dir.openNextFile();
    while (f) {
        String fname = f.name();
        // Normalize basename
        int slash = fname.lastIndexOf('/');
        String base = (slash >= 0) ? fname.substring(slash + 1) : fname;
        if (base.length() > 0 && !base.startsWith(".")) {
            FileEntry e;
            e.name = base;
            String dp = dirPath;
            if (!dp.endsWith("/")) dp += "/";
            e.path = dp + base;
            e.isDir = f.isDirectory();
            e.size = e.isDir ? 0 : (uint32_t)f.size();
            out.push_back(e);
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name.compareTo(b.name) < 0;
    });
    return out;
}

static void collectBinsInDir(const char* dirPath, std::vector<FileEntry>& out) {
    auto entries = listDir(dirPath);
    for (auto& e : entries) {
        if (!e.isDir) {
            String low = e.name;
            low.toLowerCase();
            if (low.endsWith(".bin") && e.size > 4096) {
                bool dup = false;
                for (auto& ex : out) {
                    if (ex.path == e.path) { dup = true; break; }
                }
                if (!dup) out.push_back(e);
            }
        }
    }
}

std::vector<FileEntry> listSdBinApps() {
    std::vector<FileEntry> out;
    if (!g_sys.sdMounted) return out;
    collectBinsInDir("/apps", out);
    // Also surface any .bin placed in root if user dropped it there
    collectBinsInDir("/", out);
    return out;
}

String readTextFile(const char* path, size_t maxBytes) {
    fs::FS& fs = activeFS();
    File f = fs.open(path, FILE_READ);
    if (!f || f.isDirectory()) return "";
    size_t sz = f.size();
    if (sz > maxBytes) sz = maxBytes;
    String s;
    s.reserve(sz + 1);
    while (f.available() && s.length() < sz) {
        s += (char)f.read();
    }
    f.close();
    return s;
}

bool writeTextFile(const char* path, const String& content) {
    if (g_sys.usbMscActive && g_sys.sdMounted) return false;
    fs::FS& fs = activeFS();
    File f = fs.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    refreshSdStats();
    return written == content.length();
}

bool deletePathRecursive(const char* path) {
    fs::FS& fs = activeFS();
    File f = fs.open(path);
    if (!f) return false;
    bool isDir = f.isDirectory();
    f.close();

    if (!isDir) {
        bool ok = fs.remove(path);
        refreshSdStats();
        return ok;
    }

    auto children = listDir(path);
    for (auto& c : children) {
        deletePathRecursive(c.path.c_str());
    }
    bool ok = fs.rmdir(path);
    refreshSdStats();
    return ok;
}

static void searchDirRecursive(const String& dirPath, const String& qLower, std::vector<SearchMatch>& out, int depth, int maxResults) {
    if (depth > 3 || (int)out.size() >= maxResults) return;
    auto entries = listDir(dirPath.c_str());
    for (auto& e : entries) {
        if ((int)out.size() >= maxResults) break;
        if (e.name == "System Volume Information") continue;

        String nameLow = e.name;
        nameLow.toLowerCase();
        if (nameLow.indexOf(qLower) >= 0) {
            SearchMatch m;
            if (e.isDir) {
                m.kind = SearchMatch::MATCH_DIR;
                m.title = e.name;
                m.subtitle = "Folder: " + e.path;
            } else if (nameLow.endsWith(".bin")) {
                m.kind = SearchMatch::MATCH_SD_BIN;
                m.title = e.name;
                m.subtitle = "App (.bin) " + formatBytes(e.size);
            } else {
                m.kind = SearchMatch::MATCH_FILE;
                m.title = e.name;
                m.subtitle = e.path + " (" + formatBytes(e.size) + ")";
            }
            m.path = e.path;
            out.push_back(m);
        }
        if (e.isDir) {
            searchDirRecursive(e.path, qLower, out, depth + 1, maxResults);
        }
    }
}

std::vector<SearchMatch> searchFilesystem(const String& query, int maxResults) {
    std::vector<SearchMatch> out;
    String qLower = query;
    qLower.trim();
    qLower.toLowerCase();
    if (qLower.isEmpty()) return out;
    searchDirRecursive("/", qLower, out, 0, maxResults);
    return out;
}

String formatBytes(uint32_t bytes) {
    if (bytes < 1024) return String(bytes) + "B";
    if (bytes < 1024 * 1024) return String(bytes / 1024.0f, 1) + "KB";
    return String(bytes / (1024.0f * 1024.0f), 2) + "MB";
}

