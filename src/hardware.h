#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

struct NotifRecord {
    String timeStr;
    uint8_t app = 0;
    String msg;
};

void initHardware();
void tickHardware(bool hadInputEvent);

void applyBrightness(uint8_t val);
void applyPowerProfile();
void wakeScreen();
void sleepScreen();
void enterScreensaver();
void exitScreensaver();
void enterChargeSleepMode();
void exitChargeSleepMode();
void setFlashlight(bool on);

void playKeyClick();
void playNotifyChime();
void triggerNotification(NotifApp app, const char* msg);

void logNotificationToSd(NotifApp app, const char* msg);
std::vector<NotifRecord> loadNotificationsFromSd(int maxItems = 30);
void deleteNotificationOnSd(int newestFirstIdx);
void clearNotificationsOnSd();

void updateLedEngine();
void executeBtnAction(BtnAction act);
void updateChargingDetector(bool allowAnim);
void playChargingAnimation(bool fromSleep);
