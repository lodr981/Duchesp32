#pragma once
// Actions the web API asks the main program to perform (implemented in SoulCage.ino).
#include <Arduino.h>

void appSelectSoul(int index);
void appApplySettings();  // settings changed through the API
void appSetDisplay(bool on);
void appSleepNow();
bool appGhostAction(const String &id);  // false = unknown action
// Pet care: cmd = feed|play|pet|sleep|wake|name|reset. Returns a message for the UI.
bool appPetCommand(const String &cmd, const String &arg, String &message);
void appSetScene(int scene);  // SCENE_COUNT = original trapped souls
bool appCalibrate();
void appStatusJson(String &body);  // appends ghost/pet fields to the status JSON
