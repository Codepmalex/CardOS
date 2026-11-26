#include "bt_manager.h"
#include "config.h"
#include "state.h"
#include "hardware.h"
#include <NimBLEDevice.h>

namespace {

static bool s_bleInited = false;
static bool s_bleActive = false;
static volatile bool s_peerConnected = false;
static volatile uint16_t s_connHandle = 0xFFFF;
static volatile bool s_evtConnected = false;
static volatile bool s_evtDisconnected = false;
static NimBLEServer* s_srv = nullptr;
static NimBLECharacteristic* s_nameChar = nullptr;

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

static CardBtCallbacks s_btCb;

void configureAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (!adv) return;
    adv->stop();

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06); // General Discoverable + BLE only
    advData.setName(strlen(g_settings.btName) ? g_settings.btName : "CardOS");
    advData.setAppearance(0x0080); // Generic Computer
    advData.setCompleteServices(NimBLEUUID((uint16_t)0x180A)); // Device Information
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName(strlen(g_settings.btName) ? g_settings.btName : "CardOS");
    adv->setScanResponseData(scanData);
    adv->setScanResponse(true);
}

void ensureBleInit() {
    if (s_bleInited) return;
    const char* nm = strlen(g_settings.btName) ? g_settings.btName : "CardOS";
    NimBLEDevice::init(nm);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6);
    NimBLEDevice::setSecurityAuth(true, true, true);

    s_srv = NimBLEDevice::createServer();
    s_srv->setCallbacks(&s_btCb);

    // Standard Device Information Service (0x180A) for broad phone visibility
    NimBLEService* dis = s_srv->createService((uint16_t)0x180A);
    s_nameChar = dis->createCharacteristic((uint16_t)0x2A24, NIMBLE_PROPERTY::READ);
    s_nameChar->setValue("CardOS-ADV");
    auto* mfg = dis->createCharacteristic((uint16_t)0x2A29, NIMBLE_PROPERTY::READ);
    mfg->setValue("M5Stack");
    auto* fw = dis->createCharacteristic((uint16_t)0x2A26, NIMBLE_PROPERTY::READ);
    fw->setValue(CARDOS_VERSION);
    dis->start();

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
        }
        s_peerConnected = false;
        g_sys.btConnected = false;
    }
    saveSettings();
}

bool btIsEnabled() {
    return s_bleActive;
}

bool btIsConnected() {
    return s_peerConnected;
}

void btDisconnect() {
    if (s_bleInited && s_srv && s_peerConnected && s_connHandle != 0xFFFF) {
        s_srv->disconnect(s_connHandle);
    }
    s_peerConnected = false;
    g_sys.btConnected = false;
}

const char* btStatusText() {
    if (!g_sys.btEnabled) return "Off";
    if (s_peerConnected) return "Connected";
    return "Discoverable";
}

void btApplyName() {
    if (!s_bleInited) return;
    configureAdvertising();
    if (s_bleActive) NimBLEDevice::startAdvertising();
}

void tickBluetooth() {
    g_sys.btEnabled = s_bleActive;
    g_sys.btConnected = s_peerConnected;
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
