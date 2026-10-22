#pragma once
#include <Arduino.h>

// Callbacks the web API uses to drive the pendant (implemented in SoulCage.ino).
struct NetActions {
  void (*selectSoul)(int index);
  void (*applySettings)();
  void (*setDisplay)(bool on);
  void (*sleepNow)();
};

void netInit(const NetActions &actions);
// rescue = hotspot mode regardless of saved networks (3 quick power cycles).
void netStart(bool rescue);
void netStop();
void netLoop();
bool netActive();
