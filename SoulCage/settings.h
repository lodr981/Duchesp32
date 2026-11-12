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

enum DisplayMode : uint8_t {
  MODE_GHOST = 0,  // procedural panda ghost + pet
  MODE_SOULS = 1,  // the original trapped souls animation
};

struct Settings {
  uint8_t mode = MODE_GHOST;
  uint8_t scene = 0;
  uint8_t eyeColor = 0;
  uint8_t spooky = 1;          // 0 cute, 1 sometimes, 2 often
  bool tiltInvert = false;
  uint8_t motionSensitivity = 1;  // 0 low, 1 normal, 2 high
  float refX = 0, refY = 0, refZ = 0;  // gravity when hanging straight, 0 = not calibrated
  uint8_t soul = 0;            // index into souls
  uint8_t brightness = 100;    // 5..100 %
  uint16_t frameDelay = 100;   // ms per frame
  bool displayOn = true;
  uint16_t sleepMinutes = 0;   // 0 = never
  uint8_t wifiPolicy = WIFI_AFTER_BOOT;
  uint8_t wifiMinutes = 10;
  String hostname = DEFAULT_HOSTNAME;
  String deviceName = DEFAULT_DEVICE_NAME;  // hotspot name
  String greeting = DEFAULT_GREETING;
  String subtitle = DEFAULT_SUBTITLE;
  bool autoShow = true;
  bool qrOnBoot = true;
  bool soulsOnly = true;  // only the trapped souls (panda ghost and pet hidden)
  // Tilt maze: progress and how the accelerometer maps to the screen
  uint8_t mazeUnlocked = 1;
  uint8_t mazeStars[15] = {0};
  uint8_t mazeAxisX = 0, mazeAxisY = 1;
  int8_t mazeSignX = 1, mazeSignY = 1;
  bool mazeCalibrated = false;
  uint32_t wispBest = 0;   // Lovec světýlek high score
  uint32_t jarLights = 0;  // all lights ever caught (the soul's jar)
  // Radar screens (weather + aircraft) around home
  String homeName = "Jenišovice";
  float homeLat = 50.6170f, homeLon = 15.1410f;
  uint8_t radarRange = 50;     // km from the centre to the rim: 25, 50 or 100
  bool radarInShow = true;     // part of the surprise rotation (needs internet)
  bool rainAlert = true;       // show the weather radar when rain comes close  // QR codes for Wi-Fi and the app after switching on  // surprise mode: rotates scenes and souls
  String password = DEFAULT_PASSWORD;
  String manifestUrl = DEFAULT_MANIFEST_URL;
  Network networks[MAX_NETWORKS];
};

extern Settings settings;

void settingsLoad();
void settingsSave();

struct PetData;
bool petLoad(PetData &data);
void petSave(const PetData &data);

struct HuntData;
bool huntLoad(HuntData &data);
void huntSave(const HuntData &data);
// Hint 0 = where to find seal 1, hint i = shown after seal i was found.
String huntHint(int index);
void huntSetHint(int index, const String &text);

// Small persistent counters used outside of Settings.
uint8_t persistGetU8(const char *key, uint8_t def = 0);
void persistSetU8(const char *key, uint8_t value);
