#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include "input.h"

// Token & GitHub submission helpers
bool hasGithubToken();
bool submitBugReportToGithub(const String& source, const String& errorMsg, String& statusOut);
bool submitFeatureRequestToGithub(const String& title, const String& details, String& statusOut);

// Global OS Error overlay helpers
void triggerOsError(const char* source, const char* msg);
bool hasActiveOsError();
void handleOsErrorInput(const InputEvent& ev);
void renderOsErrorOverlay(M5Canvas& spr);
