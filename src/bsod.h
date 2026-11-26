#pragma once
#include <M5Cardputer.h>

// Update RTC breadcrumb so crashes/panics report where CardOS was running
void bsodRecordHeartbeat(const char* appName);

// Check esp_reset_reason() at boot; if previous boot crashed/panicked/brownout, show BSOD until keypress
void bsodCheckOnBoot(M5Canvas& spr);

// Display funny & informative Windows-style BSOD modal (can be triggered from Settings/Shell)
void showBsodScreen(M5Canvas& spr, const char* stopCode, const char* detail, bool waitKey = true);

