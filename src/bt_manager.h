#pragma once
#include <Arduino.h>

void btSetEnabled(bool on);
bool btIsEnabled();
bool btIsConnected();
void btDisconnect();
const char* btStatusText();
void btApplyName();
void tickBluetooth();

