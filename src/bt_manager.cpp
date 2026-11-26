#include "bt_manager.h"
#include "config.h"
#include "state.h"
#include "hardware.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

namespace {

// Standard 63-byte Boot Keyboard HID Report Descriptor (recognized natively by iOS/Android/macOS/Windows)
static const uint8_t kHidKeyboardReportMap[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x05, 0x07,       //   Usage Page (Key Codes)
    0x19, 0xE0,       //   Usage Minimum (224)
    0x29, 0xE7,       //   Usage Maximum (231)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Constant)
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Key Codes)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data, Array)
    0xC0              // End Collection
};

static bool s_bleInited = false;
static bool s_bleActive = false;
static volatile bool s_peerConnected = false;
static volatile uint16_t s_connHandle = 0xFFFF;
static volatile bool s_evtConnected = false;
static volatile bool s_evtDisconnected = false;

static NimBLEServer*          s_srv = nullptr;
static NimBLEHIDDevice*       s_hid = nullptr;
static NimBLECharacteristic*  s_inputRep = nullptr;
static NimBLEClient*          s_client = nullptr;
static String                 s_connectedPeerName;

class CardBtCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        (void)pServer;
        s_peerConnected = true;
        if (desc) s_connHandle = desc->conn_handle;
        s_evtConnected = true;
    }
    void onDisconnect(NimBLEServer* pServer) override {
        (void)pServer;
        s_peerConnected = false;
        s_connHandle = 0xFFFF;
        s_evtDisconnected = true;
        if (s_bleActive) {
            NimBLEDevice::startAdvertising();
        }
    }
};

class CardBtClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* pClient) override {
        (void)pClient;
        s_peerConnected = false;
        s_connectedPeerName = "";
        s_evtDisconnected = true;
    }
};

static CardBtCallbacks       s_btCb;
static CardBtClientCallbacks s_cliCb;

void configureAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (!adv) return;
    adv->stop();

    const char* devName = strlen(g_settings.btName) ? g_settings.btName : "CardOS";

    // Put HID UUID + HID Keyboard Appearance + Name directly in primary ADV packet so iOS Settings lists it
    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setAppearance(HID_KEYBOARD);
    advData.setCompleteServices(NimBLEUUID((uint16_t)0x1812));
    advData.setName(devName);
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName(devName);
    scanData.setCompleteServices(NimBLEUUID((uint16_t)0x180F));
    adv->setScanResponseData(scanData);
    adv->setScanResponse(true);
}

void ensureBleInit() {
    if (s_bleInited) return;
    const char* nm = strlen(g_settings.btName) ? g_settings.btName : "CardOS";
    NimBLEDevice::init(nm);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    s_srv = NimBLEDevice::createServer();
    s_srv->setCallbacks(&s_btCb);

    s_hid = new NimBLEHIDDevice(s_srv);
    s_inputRep = s_hid->inputReport(1);
    s_hid->manufacturer("M5Stack");
    s_hid->pnp(0x02, 0x05AC, 0x022C, 0x0110);
    s_hid->hidInfo(0x00, 0x01);
    s_hid->reportMap((uint8_t*)kHidKeyboardReportMap, sizeof(kHidKeyboardReportMap));
    s_hid->setBatteryLevel((uint8_t)constrain(g_sys.batteryPct, 0, 100));
    s_hid->startServices();

    configureAdvertising();
    s_bleInited = true;
}

} // namespace

void btSetEnabled(bool on) {
    g_settings.btEnabled = on;
    g_sys.btEnabled = on;
    if (on) {
        ensureBleInit();
        s_bleActive = true;
        if (s_hid) s_hid->setBatteryLevel((uint8_t)constrain(g_sys.batteryPct, 0, 100));
        configureAdvertising();
        NimBLEDevice::startAdvertising();
    } else {
        s_bleActive = false;
        if (s_bleInited) {
            NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
            if (adv) adv->stop();
            if (s_srv && s_peerConnected && s_connHandle != 0xFFFF) {
                s_srv->disconnect(s_connHandle);
            }
            if (s_client && s_client->isConnected()) {
                s_client->disconnect();
            }
        }
        s_peerConnected = false;
        s_connectedPeerName = "";
        g_sys.btConnected = false;
    }
    saveSettings();
}

bool btIsEnabled() {
    return s_bleActive;
}

bool btIsConnected() {
    return s_peerConnected || (s_client && s_client->isConnected());
}

void btDisconnect() {
    if (s_bleInited) {
        if (s_srv && s_peerConnected && s_connHandle != 0xFFFF) {
            s_srv->disconnect(s_connHandle);
        }
        if (s_client && s_client->isConnected()) {
            s_client->disconnect();
        }
    }
    s_peerConnected = false;
    s_connectedPeerName = "";
    g_sys.btConnected = false;
}

const char* btStatusText() {
    if (!g_sys.btEnabled) return "Off";
    if (s_client && s_client->isConnected()) return "Paired (BLE)";
    if (s_peerConnected) return "Connected";
    return "Discoverable";
}

void btApplyName() {
    if (!s_bleInited) return;
    configureAdvertising();
    if (s_bleActive) NimBLEDevice::startAdvertising();
}

std::vector<BtPeerInfo> btScanNearby(uint32_t durationSec) {
    std::vector<BtPeerInfo> out;
    if (!g_sys.btEnabled) btSetEnabled(true);
    ensureBleInit();

    NimBLEScan* scan = NimBLEDevice::getScan();
    if (!scan) return out;
    scan->setActiveScan(true);
    scan->setInterval(45);
    scan->setWindow(15);
    NimBLEScanResults results = scan->start(durationSec, false);
    int cnt = results.getCount();
    for (int i = 0; i < cnt && (int)out.size() < 24; ++i) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);
        BtPeerInfo p;
        p.addr = String(dev.getAddress().toString().c_str());
        p.name = dev.haveName() ? String(dev.getName().c_str()) : p.addr;
        p.rssi = dev.getRSSI();
        bool dup = false;
        for (const auto& ex : out) {
            if (ex.addr == p.addr) { dup = true; break; }
        }
        if (!dup) out.push_back(p);
    }
    scan->clearResults();
    if (s_bleActive) NimBLEDevice::startAdvertising();
    return out;
}

bool btConnectPeer(const String& addr) {
    if (addr.isEmpty()) return false;
    if (!g_sys.btEnabled) btSetEnabled(true);
    ensureBleInit();

    if (!s_client) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(&s_cliCb, false);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }

    NimBLEAddress target(addr.c_str());
    bool ok = s_client->connect(target);
    if (ok) {
        s_peerConnected = true;
        s_connectedPeerName = addr;
        g_sys.btConnected = true;
        triggerNotification(NOTIF_PHONE, "BLE Device Paired");
    }
    if (s_bleActive) NimBLEDevice::startAdvertising();
    return ok;
}

void tickBluetooth() {
    g_sys.btEnabled = s_bleActive;
    g_sys.btConnected = btIsConnected();
    if (s_evtConnected) {
        s_evtConnected = false;
        triggerNotification(NOTIF_PHONE, "Bluetooth Connected");
    }
    if (s_evtDisconnected) {
        s_evtDisconnected = false;
        if (s_bleActive) {
            triggerNotification(NOTIF_PHONE, "Bluetooth Disconnected");
        }
    }
}
