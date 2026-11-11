#pragma once
// Actions the web API asks the main program to perform (implemented in SoulCage.ino).
#include <Arduino.h>

void appSelectSoul(int index);
bool appSoulAction(const String &action);  // bang|knock|escape for the trapped souls
void appApplySettings();  // settings changed through the API
void appSetDisplay(bool on);
void appSleepNow();
bool appGhostAction(const String &id);  // false = unknown action
// Pet care: cmd = feed|play|pet|sleep|wake|name|reset. Returns a message for the UI.
bool appPetCommand(const String &cmd, const String &arg, String &message);
void appSetScene(int scene);  // SCENE_COUNT = original trapped souls
bool appCalibrate();
// Maze game: cmd = start|stop|calibrate, arg = level for start. Returns a message for the UI.
bool appGameCommand(const String &cmd, const String &arg, String &message);
void appStatusJson(String &body);  // appends ghost/pet fields to the status JSON
