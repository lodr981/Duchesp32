/*
   SoulCage - the digital trapped soul pendant
   Original project and artwork by Vishal Soni (YouTube: ElectroDonut), MIT license:
   https://github.com/vishalsoniindia/SoulCage---The-Digital-Trapped-Soul-Pendant

   This version adds compressed animations, OTA updates (GitHub Releases + upload
   from the phone), a web UI / REST API for iPhone (Safari, Shortcuts) and power saving.

   Board:   Waveshare ESP32-S3-LCD-1.28, esp32 core 3.3.x, TFT_eSPI 2.5.43
   Options: Flash Size 16MB, Partition Scheme "Custom" (partitions.csv), USB CDC On Boot enabled
*/

#include "animations.h"
#include "config.h"
#include "display.h"
#include "net.h"
#include "ota.h"
#include "power.h"
#include "settings.h"

// TLS for OTA downloads needs more than the default 8 KB loop stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static uint16_t frameIndex;
static uint32_t nextFrameAt;
static bool playingIntro;
static uint32_t lastInteraction;  // for the sleep timer
static uint8_t lowBatteryReadings;

static void selectSoul(int index) {
  settings.soul = index;
  settingsSave();
  playingIntro = false;
  frameIndex = 0;
  nextFrameAt = 0;
  lastInteraction = millis();
}

static void applySettings() {
  displaySetBrightness(settings.brightness);
  if (settings.wifiPolicy == WIFI_ALWAYS && !netActive()) {
    netStart(false);
  }
  lastInteraction = millis();
}

static void setDisplay(bool on) {
  settings.displayOn = on;
  settingsSave();
  displaySetPower(on);
  nextFrameAt = 0;
  lastInteraction = millis();
}

static void sleepNow() {
  netStop();
  displaySetPower(false);
  enterDeepSleep();
}

// Optional BOOT button: short press = next soul, long press = Wi-Fi hotspot on/off.
static void handleButton() {
  static bool down;
  static uint32_t since;
  static bool longHandled;
  bool pressed = digitalRead(PIN_BOOT_BUTTON) == LOW;
  if (pressed && !down) {
    down = true;
    since = millis();
    longHandled = false;
  } else if (pressed && down && !longHandled && millis() - since > 2000) {
    longHandled = true;
    if (netActive()) {
      netStop();
      displayMessage("Wi-Fi", "vypnuto");
    } else {
      netStart(true);
      displayMessage("Wi-Fi hotspot", "http://192.168.4.1");
    }
    nextFrameAt = millis() + 2000;
    lastInteraction = millis();
  } else if (!pressed && down) {
    down = false;
    if (!longHandled && millis() - since > 50) {
      selectSoul((settings.soul + 1) % soulCount());
    }
  }
}

static void checkBattery() {
  batteryUpdate();
  const BatteryState &bat = battery();
  static uint32_t lastCheck;
  if (millis() - lastCheck < 5000) {
    return;
  }
  lastCheck = millis();
  // Require several low readings in a row, Wi-Fi bursts dip the voltage.
  lowBatteryReadings = bat.present && bat.millivolts < BAT_CUTOFF_MV ? lowBatteryReadings + 1 : 0;
  if (lowBatteryReadings >= 3 && !otaBusy()) {
    displayMessage("Slaba baterie", "Nabij me prosim");
    delay(3000);
    sleepNow();
  }
}

static void showOtaProgress() {
  static uint32_t lastDraw;
  if (millis() - lastDraw < 200) {
    return;
  }
  lastDraw = millis();
  OtaStatus ota = otaStatus();
  const char *line = ota.state == OTA_DOWNLOADING ? "Stahuji firmware"
                     : ota.state == OTA_DONE      ? "Hotovo, restart"
                                                  : "Pripravuji";
  displayMessage("Aktualizace", line, ota.progress);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nSoulCage %s\n", FW_VERSION);
  otaBootCheck();
  settingsLoad();
  if (settings.soul >= soulCount()) {
    settings.soul = 0;
  }

  // Power-cycle gesture (works without any button in the case):
  // on -> off -> on within 20 s = next soul, three times = Wi-Fi rescue hotspot.
  bool rescue = false;
  uint8_t quick = quickBootCount();
  if (quick == 2) {
    settings.soul = (settings.soul + 1) % soulCount();
    settingsSave();
  } else if (quick == 3) {
    settings.soul = (settings.soul + soulCount() - 1) % soulCount();  // undo the soul change of boot 2
    settingsSave();
    rescue = true;
  }

  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  analogReadResolution(12);
  if (!displayInit()) {
    Serial.println("Display: out of memory");
  }
  displaySetBrightness(settings.brightness);

  batteryUpdate();
  if (battery().present && battery().millivolts < BAT_CUTOFF_MV) {
    displayMessage("Slaba baterie", "Nabij me prosim");
    delay(3000);
    sleepNow();
  }

  if (rescue) {
    displayMessage("Wi-Fi hotspot", "http://192.168.4.1");
    delay(2500);
  }

  netInit({selectSoul, applySettings, setDisplay, sleepNow});
  if (rescue || settings.wifiPolicy != WIFI_RESCUE_ONLY) {
    netStart(rescue);
  }

  playingIntro = settings.playIntro;
  if (!settings.displayOn) {
    displaySetPower(false);
  }
  lastInteraction = millis();
}

void loop() {
  otaLoop();
  quickBootTick();
  netLoop();
  handleButton();
  checkBattery();

  static bool wasBusy;
  if (otaBusy()) {
    wasBusy = true;
    showOtaProgress();
    delay(5);
    return;
  }
  if (wasBusy) {  // failed update: back to the animation (or dark display)
    wasBusy = false;
    displaySetPower(settings.displayOn);
    nextFrameAt = 0;
  }

  if (settings.sleepMinutes && millis() - lastInteraction > settings.sleepMinutes * 60000UL) {
    sleepNow();
  }

  uint32_t now = millis();
  if (!settings.displayOn || (int32_t)(now - nextFrameAt) < 0) {
    delay(2);
    return;
  }
  const Animation &anim = playingIntro ? introAnimation() : soulAt(settings.soul);
  uint8_t overlays = OVERLAY_NONE;
  if (battery().present && battery().millivolts < BAT_LOW_MV) {
    overlays |= OVERLAY_BATTERY_LOW;
  }
  if (netActive()) {
    overlays |= OVERLAY_WIFI;
  }
  if (!displayFrame(anim, frameIndex, overlays)) {
    Serial.printf("Frame %u of %s failed\n", frameIndex, anim.id);
  }
  nextFrameAt = now + settings.frameDelay;
  if (++frameIndex >= anim.frameCount) {
    frameIndex = 0;
    playingIntro = false;
  }
}
