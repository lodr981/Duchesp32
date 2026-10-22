#include "power.h"
#include "config.h"
#include "settings.h"
#include <esp_sleep.h>
#include <esp_system.h>

static bool quickBootCleared;
static BatteryState bat;
static uint32_t lastBatteryRead;

uint8_t quickBootCount() {
  // Only real power cycles count - not OTA restarts, crashes or deep sleep wakeups.
  if (esp_reset_reason() != ESP_RST_POWERON) {
    persistSetU8("qboot", 1);
    return 1;
  }
  uint8_t n = persistGetU8("qboot", 0);
  n = n >= 3 ? 1 : n + 1;
  persistSetU8("qboot", n);
  return n;
}

void quickBootTick() {
  if (!quickBootCleared && millis() > QUICK_BOOT_WINDOW_MS) {
    quickBootCleared = true;
    persistSetU8("qboot", 0);
  }
}

// Rough Li-Po discharge curve, mV -> %.
static uint8_t percentFromMv(uint16_t mv) {
  static const uint16_t mvTable[] = {3300, 3500, 3600, 3700, 3750, 3800, 3850, 3900, 4000, 4100, 4200};
  static const uint8_t pctTable[] = {0, 5, 10, 20, 30, 40, 50, 60, 75, 90, 100};
  if (mv <= mvTable[0]) {
    return 0;
  }
  for (size_t i = 1; i < sizeof(mvTable) / sizeof(mvTable[0]); i++) {
    if (mv <= mvTable[i]) {
      return pctTable[i - 1] + (pctTable[i] - pctTable[i - 1]) * (mv - mvTable[i - 1]) / (mvTable[i] - mvTable[i - 1]);
    }
  }
  return 100;
}

void batteryUpdate() {
  if (lastBatteryRead && millis() - lastBatteryRead < 5000) {
    return;
  }
  lastBatteryRead = millis();
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(PIN_BAT_ADC);
  }
  uint16_t mv = (sum / 16) * BAT_DIVIDER;
  // Smooth readings, the voltage dips while Wi-Fi transmits.
  bat.millivolts = bat.millivolts ? (bat.millivolts * 3 + mv) / 4 : mv;
  bat.present = bat.millivolts >= BAT_VALID_MV;
  bat.percent = bat.present ? percentFromMv(bat.millivolts) : 0;
}

const BatteryState &battery() {
  return bat;
}

void enterDeepSleep() {
  Serial.println("Deep sleep");
  Serial.flush();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BUTTON, 0);
  esp_deep_sleep_start();
}

void restartClean() {
  persistSetU8("qboot", 0);
  delay(100);
  esp_restart();
}
