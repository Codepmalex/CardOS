#pragma once
#include <Arduino.h>
#include "config.h"

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

void updateLedEngine();
void executeBtnAction(BtnAction act);
void updateChargingDetector(bool allowAnim);
void playChargingAnimation(bool fromSleep);
