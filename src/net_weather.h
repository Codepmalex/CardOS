#pragma once
#include <Arduino.h>
#include <vector>

struct WifiNetInfo {
    String ssid;
    int rssi = 0;
    bool open = false;
};

void initNetWeather();
void requestWifiConnect();
void requestWifiDisconnect();
void requestWeatherRefresh();
void requestMinerRefresh();
void applyTimezoneConfig();

// Synchronous Wi-Fi scan (called from Core 0 worker or Serial)
std::vector<WifiNetInfo> scanWifiNetworks();

// Non-blocking background Wi-Fi scan API for UI
void requestWifiScan();
bool isWifiScanInProgress();
uint32_t getWifiScanSeq();
std::vector<WifiNetInfo> getWifiScanResults();

// Weather WMO code helpers
const char* weatherCodeToShort(int code);
const char* weatherCodeToLong(int code);

