#pragma once
#include <Arduino.h>
#include <vector>

struct BtPeerInfo {
    String name;
    String addr;
    int rssi = 0;
};

void btSetEnabled(bool on);
bool btIsEnabled();
bool btIsConnected();
void btDisconnect();
const char* btStatusText();
void btApplyName();
std::vector<BtPeerInfo> btScanNearby(uint32_t durationSec = 3);
bool btConnectPeer(const String& addr);
void tickBluetooth();
