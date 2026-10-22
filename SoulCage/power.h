#pragma once
#include <Arduino.h>

struct BatteryState {
  bool present;
  uint16_t millivolts;
  uint8_t percent;
};

// Counts power-on boots that follow each other within QUICK_BOOT_WINDOW_MS.
// Returns 1 for a normal boot, 2, 3... for quick power cycles.
uint8_t quickBootCount();
void quickBootTick();

void batteryUpdate();
const BatteryState &battery();

// Deep sleep; wakes on BOOT button or power cycle.
[[noreturn]] void enterDeepSleep();

// Restart without the next boot counting as a quick power cycle.
[[noreturn]] void restartClean();
