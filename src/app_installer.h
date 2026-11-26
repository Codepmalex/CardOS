#pragma once
#include <Arduino.h>
#include <functional>

struct BinImageInfo {
    bool valid = false;
    bool isMerged = false;
    uint32_t fileSize = 0;
    uint32_t appOffset = 0;
    uint32_t appSize = 0;
    String error;
};

using FlashProgressCb = std::function<void(uint32_t written, uint32_t total, const char* stage)>;

// Ensure otadata points to app0 (CardOS) on normal boot
void ensureBootToCardOS();

// Inspect a .bin file on SD to see if it's a merged image or bare app image
BinImageInfo inspectBinFile(const char* path);

// Check if this exact .bin is already installed in app1
bool isBinAlreadyInstalled(const char* path, uint32_t fileSize);

// Flash the .bin file (app portion) into app1 partition, save NVS metadata
bool installBinToApp1(const char* path, FlashProgressCb cb, String& errOut);

// Set boot partition to app1 and restart into the installed app
bool launchInstalledApp1(String& errOut);

// Reboot via DEEPSLEEP_RESET (5) so bootloader_cardos.bin returns to app0 (CardOS)
void rebootToCardOS();

