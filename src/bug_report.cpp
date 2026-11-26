#include "bug_report.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "ui.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace {

struct OsErrState {
    bool active = false;
    bool reporting = false;
    bool reportedOk = false;
    String source;
    String msg;
    String reportStatus;
};

static OsErrState s_err;

String deriveOwnerRepo() {
    String url = String(g_settings.storeUrl);
    const char* rawHost = "raw.githubusercontent.com/";
    int p = url.indexOf(rawHost);
    if (p >= 0) {
        String rest = url.substring(p + (int)strlen(rawHost));
        int s1 = rest.indexOf('/');
        int s2 = (s1 >= 0) ? rest.indexOf('/', s1 + 1) : -1;
        if (s1 > 0 && s2 > s1 + 1) {
            return rest.substring(0, s2);
        }
    }
    return "Codepmalex/CardOS";
}

void appendReportLog(const char* filePath, const String& tag, const String& details) {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return;
    File f = activeFS().open(filePath, FILE_APPEND);
    if (!f) f = activeFS().open(filePath, FILE_WRITE);
    if (!f) return;

    struct tm tmInfo;
    char tbuf[32] = "no-ntp";
    if (getSafeLocalTime(tmInfo)) {
        snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d",
                 tmInfo.tm_year + 1900, tmInfo.tm_mon + 1, tmInfo.tm_mday,
                 tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);
    }
    f.printf("[%s] CardOS v%s | %s | %s | Heap:%luK Max:%luK Bat:%d%%\n",
             tbuf, CARDOS_VERSION, tag.c_str(), details.c_str(),
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getMaxAllocHeap() / 1024),
             g_sys.batteryPct);
    f.close();
}

bool postGithubIssue(const String& issueTitle, const String& issueBody, const char* label,
                     const char* okPrefix, String& statusOut) {
    if (WiFi.status() != WL_CONNECTED) {
        statusOut = "Saved to SD (Wi-Fi offline)";
        return false;
    }
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) {
        statusOut = "No storage for token";
        return false;
    }

    String tok = readTextFile("/cardos_gh_token.txt", 512);
    tok.trim();
    if (tok.isEmpty()) {
        statusOut = "Saved SD! Add /cardos_gh_token.txt";
        return false;
    }

    String ownerRepo = deriveOwnerRepo();
    String apiUrl = "https://api.github.com/repos/" + ownerRepo + "/issues";

    JsonDocument payload;
    String safeTitle = issueTitle;
    if (safeTitle.length() > 110) safeTitle = safeTitle.substring(0, 107) + "...";
    payload["title"] = safeTitle;
    payload["body"] = issueBody;
    JsonArray labels = payload["labels"].to<JsonArray>();
    labels.add(label);

    String jsonStr;
    serializeJson(payload, jsonStr);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, apiUrl)) {
        statusOut = "HTTP init err (saved SD)";
        return false;
    }
    http.addHeader("User-Agent", "CardOS");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Authorization", "Bearer " + tok);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");

    int code = http.POST(jsonStr);
    String resp = http.getString();
    http.end();

    if (code == 201 || code == 200) {
        JsonDocument rdoc;
        if (deserializeJson(rdoc, resp) == DeserializationError::Ok && rdoc["number"].is<int>()) {
            int num = rdoc["number"].as<int>();
            statusOut = String(okPrefix) + " GitHub #" + String(num);
        } else {
            statusOut = String(okPrefix) + " GitHub!";
        }
        return true;
    }

    statusOut = "GitHub HTTP " + String(code) + " (saved SD)";
    return false;
}

} // namespace

bool hasGithubToken() {
    if (!g_sys.sdMounted && !g_sys.spiffsMounted) return false;
    String tok = readTextFile("/cardos_gh_token.txt", 512);
    tok.trim();
    return !tok.isEmpty();
}

bool submitBugReportToGithub(const String& source, const String& errorMsg, String& statusOut) {
    appendReportLog("/bug_reports.log", source, errorMsg);
    String title = "[CardOS v" + String(CARDOS_VERSION) + "] " + source + ": " + errorMsg;
    String body = "## Automated CardOS Error Report\n"
                  "- **Version:** CardOS v" + String(CARDOS_VERSION) + "\n"
                  "- **Source:** `" + source + "`\n"
                  "- **Error:** `" + errorMsg + "`\n"
                  "- **Free Heap:** " + String(ESP.getFreeHeap() / 1024) + " KB (Max alloc: " + String(ESP.getMaxAllocHeap() / 1024) + " KB)\n"
                  "- **Battery:** " + String(g_sys.batteryPct) + "% (" + String(g_sys.batteryMv) + " mV)\n"
                  "- **Storage:** " + String(activeFSName()) + (g_sys.sdMounted ? (" (" + String(g_sys.sdFreeMB) + "MB free)") : "") + "\n"
                  "- **Wi-Fi RSSI:** " + String(WiFi.RSSI()) + " dBm\n";
    return postGithubIssue(title, body, "bug-report", "Reported!", statusOut);
}

bool submitFeatureRequestToGithub(const String& title, const String& details, String& statusOut) {
    appendReportLog("/feature_requests.log", title, details);
    String issueTitle = "[Feature] " + title;
    String body = "## CardOS Feature Request\n"
                  "- **Submitted From:** CardOS v" + String(CARDOS_VERSION) + "\n"
                  "- **Title:** " + title + "\n\n"
                  "### Details\n"
                  + (details.length() > 0 ? details : String("_No extra details provided._")) + "\n";
    return postGithubIssue(issueTitle, body, "feature-request", "Requested!", statusOut);
}

void triggerOsError(const char* source, const char* msg) {
    s_err.active = true;
    s_err.reporting = false;
    s_err.reportedOk = false;
    s_err.source = source ? String(source) : String("OS");
    s_err.msg = msg ? String(msg) : String("Unknown error");
    s_err.reportStatus = "";
}

bool hasActiveOsError() {
    return s_err.active;
}

void handleOsErrorInput(const InputEvent& ev) {
    if (!s_err.active) return;
    if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
        if (s_err.reporting) return;
        s_err.reporting = true;
        showToast("Reporting to GitHub...", 1200);
        String out;
        s_err.reportedOk = submitBugReportToGithub(s_err.source, s_err.msg, out);
        s_err.reportStatus = out;
        s_err.reporting = false;
        showToast(out.c_str(), 2200);
        return;
    }
    if (ev.code == CK_ENTER || isNavBack(ev)) {
        s_err.active = false;
    }
}

void renderOsErrorOverlay(M5Canvas& spr) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();
    spr.fillScreen(th.bg);
    ui::drawStatusBar(spr, "OS Error");

    spr.fillRoundRect(6, 20, SCREEN_W - 12, 92, 6, th.surface);
    spr.drawRoundRect(6, 20, SCREEN_W - 12, 92, 6, th.err);

    spr.setTextSize(1);
    spr.setTextColor(th.err);
    spr.drawString("Error in " + s_err.source + ":", 12, 26);

    spr.setTextColor(th.text);
    int y = 39;
    int maxY = s_err.reportStatus.isEmpty() ? 98 : 84;
    for (int pos = 0; pos < (int)s_err.msg.length() && y < maxY; pos += 35) {
        spr.drawString(s_err.msg.substring(pos, min((int)s_err.msg.length(), pos + 35)), 12, y);
        y += 11;
    }

    if (!s_err.reportStatus.isEmpty()) {
        spr.setTextColor(s_err.reportedOk ? th.ok : th.warn);
        String st = s_err.reportStatus;
        if ((int)st.length() > 35) st = st.substring(0, 34) + "~";
        spr.drawString(st, 12, 96);
    }

    spr.fillRoundRect(6, 116, 108, 16, 4, th.err);
    spr.setTextColor(th.bg);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("[R] Report Error", 60, 124);
    spr.setTextDatum(TL_DATUM);

    spr.setTextColor(acc);
    spr.drawString("[Ent/Esc] Close", 122, 120);
    ui::drawToastOverlay(spr);
}
