#pragma once
#include <Arduino.h>
#include "config.h"

enum WifiPolicy : uint8_t {
  WIFI_AFTER_BOOT = 0,  // on for `wifiMinutes` after boot / last web request
  WIFI_ALWAYS = 1,
  WIFI_RESCUE_ONLY = 2,  // only after 3 quick power cycles
};

struct Network {
  String ssid;
  String pass;
};

struct Settings {
  uint8_t soul = 0;            // index into souls (not counting intro)
  uint8_t brightness = 100;    // 5..100 %
  uint16_t frameDelay = 100;   // ms per frame
  bool playIntro = true;
  bool displayOn = true;
  uint16_t sleepMinutes = 0;   // 0 = never
  uint8_t wifiPolicy = WIFI_AFTER_BOOT;
  uint8_t wifiMinutes = 5;
  String hostname = DEFAULT_HOSTNAME;
  String password = DEFAULT_PASSWORD;
  String manifestUrl = DEFAULT_MANIFEST_URL;
  Network networks[MAX_NETWORKS];
};

extern Settings settings;

void settingsLoad();
void settingsSave();

// Small persistent counters used outside of Settings.
uint8_t persistGetU8(const char *key, uint8_t def = 0);
void persistSetU8(const char *key, uint8_t value);
