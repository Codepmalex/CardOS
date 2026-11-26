#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include "../hardware.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <WebServer.h>
#include <NimBLEDevice.h>
#include <vector>

extern void switchApp(AppId id);

namespace {

static constexpr const char* kBleSvcUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char* kBleRxUuid  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char* kBleTxUuid  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

struct PhoneMsg {
    String kind;   // "SMS", "CALL", "NOTIF"
    String from;
    String title;
    String body;
    bool unread = true;
};

static std::vector<PhoneMsg> s_inbox;
static SemaphoreHandle_t s_phoneMux = nullptr;
static volatile bool s_hasNewAlert = false;
static char s_pendingAlertTxt[64] = "";

static bool s_bleOn = false;
static volatile bool s_bleClientConnected = false;
static NimBLEServer* s_bleServer = nullptr;
static NimBLECharacteristic* s_txChar = nullptr;

static WebServer* s_phoneHttp = nullptr;
static String s_lastReply = "";

static void ensurePhoneMux() {
    if (!s_phoneMux) s_phoneMux = xSemaphoreCreateMutex();
}

static void pushPhoneMessage(const String& kind, const String& from, const String& title, const String& body) {
    ensurePhoneMux();
    if (s_phoneMux && xSemaphoreTake(s_phoneMux, pdMS_TO_TICKS(100))) {
        PhoneMsg m;
        m.kind = kind.length() ? kind : "NOTIF";
        m.kind.toUpperCase();
        m.from = from;
        m.title = title;
        m.body = body;
        m.unread = true;
        s_inbox.insert(s_inbox.begin(), m);
        if (s_inbox.size() > 25) s_inbox.pop_back();
        String toast = (m.kind == "CALL" ? "Call: " : (m.kind == "SMS" ? "Text: " : ""))
                     + (from.length() ? from : title);
        strlcpy(s_pendingAlertTxt, toast.c_str(), sizeof(s_pendingAlertTxt));
        s_hasNewAlert = true;
        xSemaphoreGive(s_phoneMux);
    }
}

static void sendPhoneReply(const String& reply) {
    s_lastReply = reply;
    if (s_bleOn && s_txChar) {
        std::string v(reply.c_str());
        s_txChar->setValue(v);
        if (s_bleClientConnected) {
            s_txChar->notify();
        }
    }
}

class PhoneServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        (void)pServer;
        s_bleClientConnected = true;
    }
    void onDisconnect(NimBLEServer* pServer) override {
        s_bleClientConnected = false;
        if (s_bleOn && pServer) NimBLEDevice::startAdvertising();
    }
};

class PhoneRxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        if (!pChar) return;
        std::string raw = pChar->getValue();
        if (raw.empty()) return;
        String s(raw.c_str());
        s.trim();
        // Format: TYPE|FROM|TITLE|BODY (or plain text)
        int p1 = s.indexOf('|');
        int p2 = (p1 >= 0) ? s.indexOf('|', p1 + 1) : -1;
        int p3 = (p2 >= 0) ? s.indexOf('|', p2 + 1) : -1;
        if (p1 >= 0 && p2 > p1) {
            String k = s.substring(0, p1);
            String fr = s.substring(p1 + 1, p2);
            String ti = (p3 >= 0) ? s.substring(p2 + 1, p3) : s.substring(p2 + 1);
            String bd = (p3 >= 0) ? s.substring(p3 + 1) : "";
            pushPhoneMessage(k, fr, ti, bd);
        } else {
            pushPhoneMessage("NOTIF", "Phone", "Message", s);
        }
    }
};

static PhoneServerCallbacks s_srvCb;
static PhoneRxCallbacks     s_rxCb;

static void startPhoneBle() {
    if (s_bleOn) return;
    ensurePhoneMux();
    NimBLEDevice::init("CardOS-Phone");
    NimBLEDevice::setPower(ESP_PWR_LVL_P6);
    s_bleServer = NimBLEDevice::createServer();
    s_bleServer->setCallbacks(&s_srvCb);
    NimBLEService* svc = s_bleServer->createService(kBleSvcUuid);
    NimBLECharacteristic* rx = svc->createCharacteristic(kBleRxUuid, NIMBLE_PROPERTY::WRITE);
    rx->setCallbacks(&s_rxCb);
    s_txChar = svc->createCharacteristic(kBleTxUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_txChar->setValue("READY");
    svc->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kBleSvcUuid);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    s_bleOn = true;
}

static const char PHONE_BRIDGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1"><title>CardOS Phone</title>
<style>body{background:#0b111c;color:#e2e8f0;font-family:system-ui;padding:14px}
.c{background:#152033;border:1px solid #293e5c;border-radius:12px;padding:14px;margin-bottom:12px}
input,select,textarea{width:100%;box-sizing:border-box;background:#0e1726;color:#fff;border:1px solid #2d4363;border-radius:8px;padding:9px;margin:6px 0}
button{background:#38bdf8;color:#08101c;border:0;border-radius:8px;padding:10px 14px;font-weight:700}</style></head><body>
<div class="c"><h3>Send Notification / SMS / Call to CardOS</h3>
<select id="t"><option value="NOTIF">Notification</option><option value="SMS">Text / SMS</option><option value="CALL">Incoming Call</option></select>
<input id="f" placeholder="From / App (e.g. Mom, Messages)">
<input id="ti" placeholder="Title">
<textarea id="m" rows="3" placeholder="Message body"></textarea>
<button onclick="send()">Push to Cardputer</button>
<div id="st"></div></div>
<div class="c"><b>Last Cardputer Reply:</b> <div id="rep">-</div></div>
<script>
async function send(){let p=new URLSearchParams({type:t.value,from:f.value,title:ti.value,msg:m.value});
let r=await fetch('/api/notify?'+p,{method:'POST'});st.innerText=await r.text();}
setInterval(async()=>{let r=await fetch('/api/reply');rep.innerText=(await r.text())||'-';},2000);
</script></body></html>)rawliteral";

static void startPhoneHttp() {
    if (s_phoneHttp) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        showToast("Connect Wi-Fi or AP first", 1500);
        return;
    }
    s_phoneHttp = new WebServer(8081);
    s_phoneHttp->on("/", HTTP_GET, []() {
        s_phoneHttp->send_P(200, "text/html", PHONE_BRIDGE_HTML);
    });
    s_phoneHttp->on("/api/notify", HTTP_POST, []() {
        String tp = s_phoneHttp->arg("type");
        String fr = s_phoneHttp->arg("from");
        String ti = s_phoneHttp->arg("title");
        String ms = s_phoneHttp->arg("msg");
        pushPhoneMessage(tp, fr, ti, ms);
        s_phoneHttp->send(200, "text/plain", "Delivered to CardOS");
    });
    s_phoneHttp->on("/api/reply", HTTP_GET, []() {
        s_phoneHttp->send(200, "text/plain", s_lastReply);
    });
    s_phoneHttp->begin();
}

static void stopPhoneHttp() {
    if (!s_phoneHttp) return;
    s_phoneHttp->stop();
    delete s_phoneHttp;
    s_phoneHttp = nullptr;
}

class PhoneApp : public App {
public:
    AppId id() const override { return APP_PHONE; }
    const char* name() const override { return "Phone"; }

    void onEnter() override {
        ensurePhoneMux();
        m_viewModal = false;
        m_replying = false;
    }

    void onInput(const InputEvent& ev) override {
        if (m_viewModal) {
            if (isNavUp(ev) && m_modalScroll > 0) m_modalScroll--;
            else if (isNavDown(ev) && m_modalScroll < max(0, m_modalLines - 8)) m_modalScroll++;
            else if (isNavBack(ev) || ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) {
                m_viewModal = false;
            }
            return;
        }
        if (m_replying) {
            if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) { m_replying = false; return; }
            if (ev.code == CK_BACKSPACE && !m_replyText.isEmpty()) { m_replyText.remove(m_replyText.length() - 1); return; }
            if (ev.code == CK_ENTER) {
                if (!m_replyText.isEmpty()) {
                    sendPhoneReply("REPLY:" + m_replyText);
                    showToast("Reply sent!", 1400);
                }
                m_replying = false;
                return;
            }
            if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126 && m_replyText.length() < 80) {
                m_replyText += ev.ch;
            }
            return;
        }

        if (isNavBack(ev)) { switchApp(APP_HOME); return; }
        if (ev.code == CK_TAB) { m_tab = (m_tab + 1) % 2; return; }

        if (ev.code == CK_CHAR) {
            if (ev.ch == 'b' || ev.ch == 'B') {
                if (s_bleOn) { stopPhoneBleIfRunning(); showToast("BLE Phone OFF", 1200); }
                else { startPhoneBle(); showToast("BLE CardOS-Phone ON", 1400); }
                return;
            }
            if (ev.ch == 'w' || ev.ch == 'W') {
                if (s_phoneHttp) { stopPhoneHttp(); showToast("Wi-Fi Bridge OFF", 1200); }
                else { startPhoneHttp(); }
                return;
            }
        }

        if (m_tab == 0) {
            int cnt = 0;
            if (s_phoneMux && xSemaphoreTake(s_phoneMux, pdMS_TO_TICKS(50))) {
                cnt = (int)s_inbox.size();
                if (isNavUp(ev) && m_sel > 0) {
                    m_sel--;
                    if (m_sel < m_scroll) m_scroll = m_sel;
                } else if (isNavDown(ev) && m_sel < cnt - 1) {
                    m_sel++;
                    if (m_sel >= m_scroll + 4) m_scroll = m_sel - 3;
                } else if ((ev.code == CK_ENTER || (ev.code == CK_CHAR && (ev.ch == 'i' || ev.ch == 'I'))) && cnt > 0) {
                    auto& m = s_inbox[m_sel];
                    m.unread = false;
                    m_modalTitle = "[" + m.kind + "] " + m.from;
                    m_modalBody = (m.title.length() ? (m.title + "\n\n") : "") + m.body;
                    if (m_modalBody.isEmpty()) m_modalBody = "(No message body)";
                    m_modalScroll = 0;
                    m_viewModal = true;
                } else if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R') && cnt > 0) {
                    m_replyText = "";
                    m_replying = true;
                } else if (ev.code == CK_CHAR && (ev.ch == 'y' || ev.ch == 'Y') && cnt > 0) {
                    sendPhoneReply("CALL:ACCEPT");
                    showToast("Sent: ACCEPT", 1200);
                } else if (ev.code == CK_CHAR && (ev.ch == 'n' || ev.ch == 'N') && cnt > 0) {
                    sendPhoneReply("CALL:DECLINE");
                    showToast("Sent: DECLINE", 1200);
                } else if (ev.code == CK_CHAR && (ev.ch == 'x' || ev.ch == 'X') && cnt > 0) {
                    s_inbox.erase(s_inbox.begin() + m_sel);
                    if (m_sel >= (int)s_inbox.size()) m_sel = max(0, (int)s_inbox.size() - 1);
                }
                xSemaphoreGive(s_phoneMux);
            }
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Phone Link", true);

        // Tabs
        const char* tabs[2] = {"Inbox", "Connect"};
        for (int i = 0; i < 2; ++i) {
            int x = 6 + i * 74;
            bool act = (m_tab == i);
            spr.fillRoundRect(x, 18, 70, 12, 3, act ? acc : th.surface);
            spr.setTextSize(1);
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(act ? th.bg : th.textDim);
            spr.drawString(tabs[i], x + 35, 24);
        }
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(s_bleClientConnected ? th.ok : (s_bleOn ? acc : th.textDim));
        spr.drawString(s_bleClientConnected ? "BLE:Paired" : (s_bleOn ? "BLE:Adv" : "BLE:Off"), SCREEN_W - 6, 20);
        spr.setTextDatum(TL_DATUM);

        if (m_tab == 0) {
            renderInbox(spr, th, acc);
        } else {
            renderConnect(spr, th, acc);
        }

        if (m_replying) {
            spr.fillRoundRect(12, 38, SCREEN_W - 24, 56, 5, th.surface2);
            spr.drawRoundRect(12, 38, SCREEN_W - 24, 56, 5, acc);
            spr.setTextColor(acc);
            spr.drawString("Quick Reply to Phone:", 20, 46);
            spr.setTextColor(th.text);
            String d = m_replyText + "_";
            ui::drawWrappedText(spr, d, 20, 60, SCREEN_W - 40, 22, 10);
            spr.setTextColor(th.textDim);
            spr.drawString("[Ent] Send   [ESC] Cancel", 20, 82);
        }

        ui::drawToastOverlay(spr);
        if (m_viewModal) {
            ui::drawTextModal(spr, m_modalTitle.c_str(), m_modalBody, m_modalScroll, &m_modalLines);
        }
    }

private:
    int m_tab = 0;
    int m_sel = 0;
    int m_scroll = 0;
    bool m_viewModal = false;
    String m_modalTitle;
    String m_modalBody;
    int m_modalScroll = 0;
    int m_modalLines = 0;
    bool m_replying = false;
    String m_replyText;

    void renderInbox(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        std::vector<PhoneMsg> snap;
        if (s_phoneMux && xSemaphoreTake(s_phoneMux, pdMS_TO_TICKS(30))) {
            snap = s_inbox;
            xSemaphoreGive(s_phoneMux);
        }

        if (snap.empty()) {
            spr.fillRoundRect(8, 36, SCREEN_W - 16, 78, 5, th.surface);
            spr.setTextColor(th.textDim);
            ui::drawWrappedText(spr,
                "No phone notifications yet.\nPress [Tab] Connect or [B] to turn on BLE ('CardOS-Phone') or [W] for Wi-Fi (:8081).",
                14, 46, SCREEN_W - 28, 60, 11);
        } else {
            for (int r = 0; r < 4; ++r) {
                int idx = m_scroll + r;
                if (idx >= (int)snap.size()) break;
                const auto& m = snap[idx];
                bool sel = (idx == m_sel);
                int y = 34 + r * 22;
                spr.fillRoundRect(6, y, SCREEN_W - 16, 20, 3, sel ? th.surface2 : th.surface);
                if (sel) spr.drawRoundRect(6, y, SCREEN_W - 16, 20, 3, acc);

                uint16_t kc = (m.kind == "CALL") ? th.err : (m.kind == "SMS" ? th.ok : acc);
                spr.setTextColor(kc);
                spr.drawString("[" + m.kind + "]", 10, y + 2);
                String hdr = m.from.length() ? m.from : m.title;
                if ((int)hdr.length() > 19) hdr = hdr.substring(0, 18) + "~";
                spr.setTextColor(m.unread ? th.text : th.textDim);
                spr.drawString(hdr, 52, y + 2);

                String prev = m.title.length() ? m.title : m.body;
                if ((int)prev.length() > 30) prev = prev.substring(0, 29) + "~";
                spr.setTextColor(th.textDim);
                spr.drawString(prev, 10, y + 11);
            }
            ui::drawScrollbar(spr, SCREEN_W - 6, 34, 86, (int)snap.size(), 4, m_scroll);
        }

        spr.setTextColor(th.textDim);
        spr.drawString("[Ent/I]Read [R]Reply [Y/N]Call [X]Del", 6, 125);
    }

    void renderConnect(M5Canvas& spr, const ThemePalette& th, uint16_t acc) {
        spr.fillRoundRect(6, 34, SCREEN_W - 12, 84, 5, th.surface);
        spr.drawRoundRect(6, 34, SCREEN_W - 12, 84, 5, th.border);
        String info = String("[B] BLE GATT (CardOS-Phone): ") + (s_bleOn ? "ON" : "OFF")
                    + "\n[W] Wi-Fi Bridge (:8081): " + (s_phoneHttp ? ("http://" + WiFi.localIP().toString() + ":8081") : "OFF")
                    + "\nNote: ESP32-S3 has BLE only (no BT voice audio). Sends texts, calls, alerts & replies.";
        spr.setTextColor(th.text);
        ui::drawWrappedText(spr, info, 12, 40, SCREEN_W - 24, 74, 11);
        spr.setTextColor(acc);
        spr.drawString("[B]BLE [W]Wi-Fi [Tab]Inbox [ESC]Home", 6, 125);
    }
};

} // namespace

void tickPhoneLink() {
    if (s_phoneHttp) {
        s_phoneHttp->handleClient();
    }
    if (s_hasNewAlert) {
        char buf[64];
        strlcpy(buf, s_pendingAlertTxt, sizeof(buf));
        s_hasNewAlert = false;
        triggerNotification(NOTIF_PHONE, buf);
    }
}

void stopPhoneBleIfRunning() {
    if (!s_bleOn) return;
    s_bleOn = false;
    s_bleClientConnected = false;
    NimBLEDevice::deinit(true);
    s_bleServer = nullptr;
    s_txChar = nullptr;
}

App* createPhoneApp() { return new PhoneApp(); }

