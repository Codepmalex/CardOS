#pragma once
#include "app_base.h"

App* createHomeApp();
App* createNotesApp();
App* createCalendarApp();
App* createCalcApp();
App* createGyroApp();
App* createConverterApp();
App* createWeatherApp();
App* createFoldersApp();
App* createUsbMscApp();
App* createStoreApp();
App* createWifiApp();
App* createSettingsApp();
App* createSleepApp();
App* createMinerApp();
App* createSysInfoApp();
App* createSnakeApp();
App* createFlappyApp();

// Shared helper to install & boot a .bin app with live UI progress
bool installAndBootBinInteractive(const String& path, const String& displayName);

// Open Settings directly to the interactive World Map Timezone Selector
void openTimezoneMap();

