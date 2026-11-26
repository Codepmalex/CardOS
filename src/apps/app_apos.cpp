#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../input.h"
#include "../ui.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <vector>

extern void switchApp(AppId id);
extern bool openAppByName(const String& name);
extern const char* getAppName(AppId id);

namespace {

static bool       s_apOn = false;
static DNSServer* s_dns  = nullptr;
static WebServer* s_web  = nullptr;
static File       s_upFile;
static String     s_upDir = "/";

static const IPAddress kApIp(192, 168, 4, 1);
static const IPAddress kApMask(255, 255, 255, 0);

static String cleanRelPath(String p) {
    p.replace("..", "");
    p.trim();
    if (!p.startsWith("/")) p = "/" + p;
    while (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
    return p;
}

static String safeFilename(String nm) {
    int sl = nm.lastIndexOf('/');
    if (sl >= 0) nm = nm.substring(sl + 1);
    int bs = nm.lastIndexOf('\\');
    if (bs >= 0) nm = nm.substring(bs + 1);
    nm.replace("..", "");
    nm.trim();
    if (nm.isEmpty()) nm = "upload.bin";
    return nm;
}

static const char APOS_PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ApOS - Cardputer Workspace</title>
<style>
*{box-sizing:border-box}body{background:#090f1a;color:#e2e8f0;font-family:system-ui,-apple-system,sans-serif;margin:0;padding:14px}
.top{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;margin-bottom:12px}
h1{margin:0;font-size:20px;color:#38bdf8}.sub{color:#94a3b8;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px}
.card{background:#111b2e;border:1px solid #243754;border-radius:12px;padding:14px}
h2{margin:0 0 10px;font-size:15px;color:#7dd3fc}
.stats{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
.stat{background:#0b1322;border:1px solid #1e2e47;border-radius:8px;padding:8px;font-size:13px}
.stat b{display:block;color:#34d399;font-size:15px}
input,textarea{width:100%;background:#0b1322;color:#f8fafc;border:1px solid #2a3f60;border-radius:8px;padding:8px;font-family:monospace;margin:6px 0}
button{background:#38bdf8;color:#08101c;border:0;border-radius:8px;padding:7px 12px;font-weight:700;cursor:pointer;margin:2px}
button.sec{background:#1e293b;color:#cbd5e1}button.del{background:#f43f5e;color:#fff}
.files{max-height:240px;overflow:auto;border:1px solid #1e2e47;border-radius:8px;padding:6px;background:#0b1322}
.frow{display:flex;justify-content:space-between;align-items:center;padding:5px 6px;border-bottom:1px solid #162338;font-size:13px}
.fname{cursor:pointer;color:#e2e8f0;word-break:break-all}.dir{color:#fbbf24;font-weight:600}
pre{background:#0b1322;border:1px solid #1e2e47;border-radius:8px;padding:8px;max-height:160px;overflow:auto;font-size:12px;white-space:pre-wrap}
</style></head><body>
<div class="top"><div><h1>ApOS Workspace</h1><div class="sub">Cardputer ADV Wireless Computer Console (Universal Captive Portal)</div></div>
<div class="sub" id="sysline">Connecting...</div></div>
<div class="grid">
<div class="card"><h2>Cardputer Status & Terminal</h2>
<div class="stats" id="stats"></div>
<div style="display:flex;gap:6px;margin-top:8px"><input id="cmd" placeholder="status, free, ls /, open music, toast hi"><button onclick="runCmd()">Run</button></div>
<pre id="out">Type a command above (help, status, ls /, free, open <app>, toast <msg>)...</pre>
<div style="display:flex;gap:6px"><input id="typ" placeholder="Type text onto Cardputer screen..."><button class="sec" onclick="sendType()">Send Keys</button></div>
</div>
<div class="card"><h2>SD / Storage File Manager & Editor</h2>
<div style="display:flex;gap:6px;align-items:center"><input id="cwd" value="/" style="margin:0"><button class="sec" onclick="ls(cwd.value)">Go</button><button class="sec" onclick="up()">Up</button><button class="sec" onclick="mkd()">+Dir</button></div>
<div class="files" id="flist" style="margin-top:8px"></div>
<form id="uf" style="margin-top:8px;display:flex;gap:6px;flex-wrap:wrap" onsubmit="upFile(event)">
<input type="file" id="fpick" style="margin:0;flex:1"><button type="submit">Upload Here</button></form>
</div>
<div class="card" style="grid-column:1/-1"><h2>File / Lua Editor</h2>
<div style="display:flex;gap:6px"><input id="epath" placeholder="/store/myapp.lua or /notes/note.txt"><button onclick="saveFile()">Save to Cardputer</button></div>
<textarea id="ebody" rows="9" placeholder="Click any file above to view/edit, or enter a new path and Save..."></textarea>
</div></div>
<script>
let C='/';
async function refreshSys(){try{let r=await fetch('/api/sys');let j=await r.json();
sysline.innerText='CardOS v'+j.ver+' | Active: '+j.app+' | Clients: '+j.clients;
stats.innerHTML='<div class="stat">Battery<b>'+j.batt+'%</b></div><div class="stat">Free Heap<b>'+j.heap+' KB</b></div><div class="stat">Storage ('+j.fs+')<b>'+j.sdFree+'/'+j.sdTot+' MB</b></div><div class="stat">Uptime<b>'+j.up+'s</b></div>';}catch(e){}}
async function ls(p){C=p||'/';cwd.value=C;let r=await fetch('/api/ls?path='+encodeURIComponent(C));let arr=await r.json();
flist.innerHTML=arr.map(f=>'<div class="frow"><span class="fname '+(f.dir?'dir':'')+'" onclick="'+(f.dir?`ls('${f.path}')`:`openF('${f.path}')`)+'">'+(f.dir?'[DIR] ':'')+f.name+(f.dir?'':' ('+f.size+'B)')+'</span><span>'+(f.dir?'':`<a href="/api/download?path=${encodeURIComponent(f.path)}"><button class="sec" type="button">DL</button></a>`)+`<button class="del" onclick="rmF('${f.path}')">X</button></span></div>`).join('')||'<div class="sub">(Empty)</div>';}
function up(){if(C==='/')return;let i=C.lastIndexOf('/');ls(i>0?C.slice(0,i):'/');}
async function openF(p){epath.value=p;let r=await fetch('/api/read?path='+encodeURIComponent(p));ebody.value=await r.text();}
async function saveFile(){if(!epath.value)return;let r=await fetch('/api/write?path='+encodeURIComponent(epath.value),{method:'POST',body:ebody.value});alert(await r.text());ls(C);}
async function rmF(p){if(!confirm('Delete '+p+'?'))return;await fetch('/api/rm?path='+encodeURIComponent(p),{method:'POST'});ls(C);}
async function mkd(){let n=prompt('New folder name:');if(!n)return;let p=(C==='/'?'':C)+'/'+n;await fetch('/api/mkdir?path='+encodeURIComponent(p),{method:'POST'});ls(C);}
async function upFile(e){e.preventDefault();let f=fpick.files[0];if(!f)return;let fd=new FormData();fd.append('file',f);
let r=await fetch('/api/upload?dir='+encodeURIComponent(C),{method:'POST',body:fd});alert(await r.text());ls(C);}
async function runCmd(){let c=cmd.value.trim();if(!c)return;let r=await fetch('/api/cmd',{method:'POST',body:c});out.innerText=await r.text();}
async function sendType(){let t=typ.value;if(!t)return;await fetch('/api/type',{method:'POST',body:t});typ.value='';}
refreshSys();ls('/');setInterval(refreshSys,3500);
</script></body></html>)rawliteral";

static void redirectPortal() {
    if (!s_web) return;
    s_web->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    s_web->sendHeader("Location", "http://192.168.4.1/", true);
    s_web->send(302, "text/plain", "");
}

static String execApCommand(String raw) {
    raw.trim();
    int sp = raw.indexOf(' ');
    String op = (sp < 0) ? raw : raw.substring(0, sp);
    String arg = (sp < 0) ? "" : raw.substring(sp + 1);
    op.toLowerCase();
    arg.trim();

    if (op == "help") return "Commands: status, free, ver, ls [path], open <app>, toast <msg>, reboot";
    if (op == "ver") return String("CardOS v") + CARDOS_VERSION + " (" + activeFSName() + ")";
    if (op == "free") return "Heap: " + String(ESP.getFreeHeap() / 1024) + " KB | MaxAlloc: " + String(ESP.getMaxAllocHeap() / 1024) + " KB";
    if (op == "status") {
        return "App: " + String(getAppName(g_sys.activeApp))
             + "\nBatt: " + String(g_sys.batteryPct) + "% (" + String(g_sys.batteryMv) + "mV)"
             + "\nHeap: " + String(ESP.getFreeHeap() / 1024) + "KB"
             + "\nSD: " + String(g_sys.sdFreeMB) + "/" + String(g_sys.sdTotalMB) + "MB"
             + "\nClients: " + String(WiFi.softAPgetStationNum());
    }
    if (op == "ls") {
        String p = cleanRelPath(arg);
        auto list = listDir(p.c_str());
        String out;
        for (const auto& e : list) {
            out += (e.isDir ? "[D] " : "    ") + e.name + (e.isDir ? "" : (" (" + formatBytes(e.size) + ")")) + "\n";
        }
        return out.isEmpty() ? "(empty)" : out;
    }
    if (op == "open" && !arg.isEmpty()) {
        return openAppByName(arg) ? ("Opened " + arg) : ("Unknown app: " + arg);
    }
    if (op == "toast" && !arg.isEmpty()) {
        showToast(arg.c_str(), 2200);
        return "Toast shown";
    }
    if (op == "reboot") {
        ESP.restart();
        return "Rebooting";
    }
    return "Unknown command (try 'help')";
}

static void startApOS() {
    if (s_apOn) return;
    const char* ssid = strlen(g_settings.apSsid) ? g_settings.apSsid : "CardOS-ApOS";
    const char* pass = (strlen(g_settings.apPass) >= 8) ? g_settings.apPass : nullptr;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(kApIp, kApIp, kApMask);
    if (!WiFi.softAP(ssid, pass)) {
        showToast("ApOS start failed", 1500);
        return;
    }

    s_dns = new DNSServer();
    s_dns->setErrorReplyCode(DNSReplyCode::NoError);
    s_dns->start(53, "*", kApIp);

    s_web = new WebServer(80);
    s_web->on("/", HTTP_GET, []() {
        s_web->sendHeader("Cache-Control", "no-store");
        s_web->send_P(200, "text/html", APOS_PORTAL_HTML);
    });
    // Captive portal OS probe endpoints (macOS, iOS, Windows, Android, Linux, ChromeOS)
    s_web->on("/hotspot-detect.html", HTTP_GET, redirectPortal);
    s_web->on("/library/test/success.html", HTTP_GET, redirectPortal);
    s_web->on("/generate_204", HTTP_GET, redirectPortal);
    s_web->on("/gen_204", HTTP_GET, redirectPortal);
    s_web->on("/connecttest.txt", HTTP_GET, redirectPortal);
    s_web->on("/ncsi.txt", HTTP_GET, redirectPortal);
    s_web->on("/fwlink", HTTP_GET, redirectPortal);
    s_web->on("/canonical.html", HTTP_GET, redirectPortal);
    s_web->onNotFound(redirectPortal);

    s_web->on("/api/sys", HTTP_GET, []() {
        JsonDocument doc;
        doc["ver"] = CARDOS_VERSION;
        doc["app"] = getAppName(g_sys.activeApp);
        doc["batt"] = g_sys.batteryPct;
        doc["heap"] = (uint32_t)(ESP.getFreeHeap() / 1024);
        doc["fs"] = activeFSName();
        doc["sdFree"] = g_sys.sdFreeMB;
        doc["sdTot"] = g_sys.sdTotalMB;
        doc["up"] = (uint32_t)(millis() / 1000);
        doc["clients"] = WiFi.softAPgetStationNum();
        String out;
        serializeJson(doc, out);
        s_web->send(200, "application/json", out);
    });

    s_web->on("/api/ls", HTTP_GET, []() {
        String p = cleanRelPath(s_web->arg("path"));
        auto list = listDir(p.c_str());
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& e : list) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = e.name;
            o["path"] = e.path;
            o["dir"] = e.isDir;
            o["size"] = e.size;
        }
        String out;
        serializeJson(doc, out);
        s_web->send(200, "application/json", out);
    });

    s_web->on("/api/read", HTTP_GET, []() {
        String p = cleanRelPath(s_web->arg("path"));
        s_web->send(200, "text/plain; charset=utf-8", readTextFile(p.c_str(), 16384));
    });

    s_web->on("/api/write", HTTP_POST, []() {
        String p = cleanRelPath(s_web->arg("path"));
        String body = s_web->arg("plain");
        bool ok = writeTextFile(p.c_str(), body);
        s_web->send(ok ? 200 : 500, "text/plain", ok ? "Saved!" : "Write failed");
    });

    s_web->on("/api/download", HTTP_GET, []() {
        String p = cleanRelPath(s_web->arg("path"));
        File f = activeFS().open(p.c_str(), FILE_READ);
        if (!f || f.isDirectory()) {
            if (f) f.close();
            s_web->send(404, "text/plain", "Not found");
            return;
        }
        s_web->streamFile(f, "application/octet-stream");
        f.close();
    });

    s_web->on("/api/mkdir", HTTP_POST, []() {
        String p = cleanRelPath(s_web->arg("path"));
        bool ok = ensureDir(p.c_str());
        s_web->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Failed");
    });

    s_web->on("/api/rm", HTTP_POST, []() {
        String p = cleanRelPath(s_web->arg("path"));
        bool ok = deletePathRecursive(p.c_str());
        s_web->send(ok ? 200 : 500, "text/plain", ok ? "Deleted" : "Failed");
    });

    s_web->on("/api/cmd", HTTP_POST, []() {
        s_web->send(200, "text/plain", execApCommand(s_web->arg("plain")));
    });

    s_web->on("/api/type", HTTP_POST, []() {
        String txt = s_web->arg("plain");
        for (size_t i = 0; i < txt.length(); ++i) {
            char c = txt[i];
            if (c == '\n') pushInputEvent({CK_ENTER, 0, false, false, true});
            else pushInputEvent({CK_CHAR, c, false, false, true});
        }
        s_web->send(200, "text/plain", "Typed");
    });

    s_web->on("/api/upload", HTTP_POST, []() {
        s_web->send(200, "text/plain", "Upload complete");
    }, []() {
        HTTPUpload& up = s_web->upload();
        if (up.status == UPLOAD_FILE_START) {
            s_upDir = cleanRelPath(s_web->arg("dir"));
            ensureDir(s_upDir.c_str());
            String fn = safeFilename(up.filename);
            String full = (s_upDir == "/" ? "" : s_upDir) + "/" + fn;
            activeFS().remove(full.c_str());
            s_upFile = activeFS().open(full.c_str(), FILE_WRITE);
        } else if (up.status == UPLOAD_FILE_WRITE) {
            if (s_upFile) s_upFile.write(up.buf, up.currentSize);
        } else if (up.status == UPLOAD_FILE_END) {
            if (s_upFile) s_upFile.close();
        }
    });

    s_web->begin();
    s_apOn = true;
    showToast("ApOS Portal Online!", 1600);
}

static void stopApOS() {
    if (!s_apOn) return;
    s_apOn = false;
    if (s_web) {
        s_web->stop();
        delete s_web;
        s_web = nullptr;
    }
    if (s_dns) {
        s_dns->stop();
        delete s_dns;
        s_dns = nullptr;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    showToast("ApOS Stopped", 1300);
}

class ApOsApp : public App {
public:
    AppId id() const override { return APP_APOS; }
    const char* name() const override { return "ApOS"; }

    void onEnter() override {
        m_editing = 0;
    }

    void onInput(const InputEvent& ev) override {
        if (m_editing != 0) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
                m_editing = 0;
                return;
            }
            if (ev.code == CK_BACKSPACE && !m_buf.isEmpty()) {
                m_buf.remove(m_buf.length() - 1);
                return;
            }
            if (ev.code == CK_ENTER) {
                m_buf.trim();
                if (m_editing == 1) {
                    if (m_buf.isEmpty()) {
                        showToast("SSID cannot be empty", 1400);
                        return;
                    }
                    strlcpy(g_settings.apSsid, m_buf.c_str(), sizeof(g_settings.apSsid));
                } else {
                    if (m_buf.length() > 0 && m_buf.length() < 8) {
                        showToast("Need >=8 chars or blank", 1600);
                        return;
                    }
                    strlcpy(g_settings.apPass, m_buf.c_str(), sizeof(g_settings.apPass));
                }
                saveSettings();
                m_editing = 0;
                if (s_apOn) {
                    stopApOS();
                    startApOS();
                } else {
                    showToast("Saved!", 1200);
                }
                return;
            }
            if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_buf.length() < 30) {
                m_buf += ev.ch;
            }
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'a' || ev.ch == 'A'))) {
            if (s_apOn) stopApOS();
            else startApOS();
        } else if (ev.code == CK_CHAR && (ev.ch == 's' || ev.ch == 'S')) {
            m_buf = g_settings.apSsid;
            m_editing = 1;
        } else if (ev.code == CK_CHAR && (ev.ch == 'p' || ev.ch == 'P')) {
            m_buf = g_settings.apPass;
            m_editing = 2;
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "ApOS Captive Portal", true);

        spr.fillRoundRect(6, 19, SCREEN_W - 12, 99, 5, th.surface);
        spr.drawRoundRect(6, 19, SCREEN_W - 12, 99, 5, s_apOn ? th.ok : th.border);

        spr.setTextSize(1);
        spr.setTextColor(s_apOn ? th.ok : th.warn);
        String hdr = s_apOn
            ? ("ONLINE  (" + String(WiFi.softAPgetStationNum()) + " device(s))")
            : "OFFLINE  ([Ent] to Start AP)";
        spr.drawString(hdr, 12, 24);

        spr.setTextColor(th.text);
        String body = String("SSID: ") + g_settings.apSsid
                    + "\nPass: " + (strlen(g_settings.apPass) >= 8 ? String(g_settings.apPass) : String("(Open)"))
                    + "\nURL:  http://192.168.4.1\n"
                    + "Connect any computer/phone: 'Sign in to Wi-Fi' popup opens ApOS workspace!";
        ui::drawWrappedText(spr, body, 12, 38, SCREEN_W - 24, 74, 11);

        if (m_editing != 0) {
            spr.fillRoundRect(16, 36, SCREEN_W - 32, 54, 5, th.surface2);
            spr.drawRoundRect(16, 36, SCREEN_W - 32, 54, 5, acc);
            spr.setTextColor(acc);
            spr.drawString(m_editing == 1 ? "Edit ApOS SSID:" : "Edit Password (blank=Open):", 24, 43);
            spr.setTextColor(th.text);
            spr.drawString(m_buf + "_", 24, 59);
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent] Save   [ESC] Cancel", 24, 74);
        }

        spr.setTextColor(acc);
        spr.drawString("[Ent]Start/Stop [S]SSID [P]Pass [ESC]Home", 6, 125);
        ui::drawToastOverlay(spr);
    }

private:
    int m_editing = 0; // 0=none, 1=SSID, 2=Pass
    String m_buf;
};

} // namespace

void tickApOS() {
    if (!s_apOn) return;
    if (s_dns) s_dns->processNextRequest();
    if (s_web) s_web->handleClient();
}

App* createApOsApp() { return new ApOsApp(); }

