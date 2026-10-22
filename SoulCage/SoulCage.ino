/*
   SoulCage - the digital trapped soul pendant
   Original project and artwork by Vishal Soni (YouTube: ElectroDonut), MIT license:
   https://github.com/vishalsoniindia/SoulCage---The-Digital-Trapped-Soul-Pendant

   This version adds the panda ghost (live drawn, reacts to motion, Tamagotchi care),
   compressed animations, OTA updates (GitHub Releases + upload from the phone),
   a web UI / REST API for iPhone (Safari, Shortcuts) and power saving.

   Board:   Waveshare ESP32-S3-LCD-1.28, esp32 core 3.3.x, TFT_eSPI 2.5.43
   Options: Flash Size 16MB, Partition Scheme "Custom" (partitions.csv), PSRAM QSPI,
            USB CDC On Boot enabled
*/

#include <time.h>
#include "animations.h"
#include "app.h"
#include "config.h"
#include "display.h"
#include "ghost.h"
#include "imu.h"
#include "net.h"
#include "ota.h"
#include "pet.h"
#include "power.h"
#include "settings.h"

// TLS for OTA downloads needs more than the default 8 KB loop stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#define NAP_AFTER_STILL_MS 120000UL
#define PET_SAVE_INTERVAL_MS 300000UL

// Original souls mode
static uint16_t frameIndex;
static uint32_t nextFrameAt;
static bool playingIntro;

// Ghost mode
static Ghost ghost;
static Pet pet;
static GhostLook look;
static bool imuOk;
static uint32_t lastGhostFrame, pauseUntil;
static GhostAction queuedAction = GA_NONE;  // e.g. level-up after eating
static bool petDirty;
static uint32_t lastPetSave, petDirtySince, lastWokenAt;
static bool offlineApplied;

static uint32_t lastInteraction;  // for the sleep timer
static uint8_t lowBatteryReadings;

static bool timeValid() {
  return time(nullptr) > 1700000000;
}

static void savePetNow() {
  pet.d.savedEpoch = timeValid() ? (uint32_t)time(nullptr) : 0;
  petSave(pet.d);
  petDirty = false;
  lastPetSave = millis();
}

static void markPetDirty() {
  if (!petDirty) {
    petDirtySince = millis();
  }
  petDirty = true;
}

static void updateLook() {
  look.scene = settings.scene % SCENE_COUNT;
  look.eyeColor = settings.eyeColor % EYES_COUNT;
  look.spooky = settings.spooky > 2 ? 1 : settings.spooky;
  look.level = pet.d.level;
}

static void setNap(bool asleep) {
  if (ghost.sleeping() == asleep) {
    return;
  }
  ghost.setSleeping(asleep);
  displaySetBrightness(asleep ? max(5, settings.brightness / 4) : settings.brightness);
  if (!asleep) {
    lastWokenAt = millis();
  }
}

static void handlePetResult(PetResult r, GhostAction action, Food food = FOOD_BAMBOO) {
  if (r == PET_REFUSED) {
    ghost.trigger(GA_REFUSE);
    return;
  }
  ghost.trigger(action, food);
  if (r == PET_LEVEL_UP) {
    queuedAction = GA_LEVELUP;
    updateLook();
  }
  markPetDirty();
}

// ------------------------------------------------------------------ app.h (web API)

void appSelectSoul(int index) {
  settings.soul = index;
  settings.mode = MODE_SOULS;
  settingsSave();
  playingIntro = false;
  frameIndex = 0;
  nextFrameAt = 0;
  lastInteraction = millis();
}

void appApplySettings() {
  displaySetBrightness(ghost.sleeping() ? max(5, settings.brightness / 4) : settings.brightness);
  if (settings.wifiPolicy == WIFI_ALWAYS && !netActive()) {
    netStart(false);
  }
  imuConfigure(settings.tiltInvert, settings.motionSensitivity);
  updateLook();
  nextFrameAt = 0;
  lastInteraction = millis();
}

void appSetDisplay(bool on) {
  settings.displayOn = on;
  settingsSave();
  displaySetPower(on);
  nextFrameAt = 0;
  lastInteraction = millis();
}

void appSleepNow() {
  savePetNow();
  netStop();
  displaySetPower(false);
  enterDeepSleep();
}

static void ensureGhostMode() {
  if (settings.mode != MODE_GHOST) {
    settings.mode = MODE_GHOST;
    settingsSave();
  }
}

bool appGhostAction(const String &id) {
  int a = ghostActionFromId(id.c_str());
  if (a < 0) {
    return false;
  }
  ensureGhostMode();
  lastInteraction = millis();
  lastWokenAt = millis();
  if (ghost.sleeping()) {
    setNap(false);
  }
  if (a == GA_LOVE) {
    handlePetResult(pet.pet(), GA_LOVE);
  } else {
    ghost.trigger((GhostAction)a);
  }
  return true;
}

// Keeps whole UTF-8 characters when shortening a name.
static void copyName(char *dst, size_t size, const String &src) {
  size_t n = src.length() < size - 1 ? src.length() : size - 1;
  while (n > 0 && n < src.length() && ((uint8_t)src[n] & 0xC0) == 0x80) {
    n--;
  }
  memcpy(dst, src.c_str(), n);
  dst[n] = 0;
}

bool appPetCommand(const String &cmd, const String &arg, String &message) {
  ensureGhostMode();
  lastInteraction = millis();
  lastWokenAt = millis();
  if (cmd == "sleep") {
    setNap(true);
    message = "Dobrou noc 💤";
    return true;
  }
  if (ghost.sleeping()) {
    setNap(false);
  }
  if (cmd == "wake") {
    message = "Vstávej!";
    return true;
  }
  if (cmd == "feed") {
    int food = foodFromId(arg.c_str());
    if (food < 0) {
      message = "Neznámé jídlo";
      return false;
    }
    PetResult r = pet.feed((Food)food);
    handlePetResult(r, GA_EAT, (Food)food);
    message = r == PET_REFUSED ? "Už nemůže, je plný 😊" : String("Mňam, ") + foodName((Food)food) + "!";
  } else if (cmd == "play") {
    PetResult r = pet.play();
    handlePetResult(r, GA_PLAY);
    message = r == PET_REFUSED ? "Je moc unavený, nech ho vyspat" : "Hurá, hrajeme si!";
  } else if (cmd == "pet") {
    handlePetResult(pet.pet(), GA_LOVE);
    message = "Má tě rád 💜";
  } else if (cmd == "name") {
    String name = arg;
    name.trim();
    if (name.isEmpty()) {
      message = "Zadej jméno";
      return false;
    }
    copyName(pet.d.name, sizeof(pet.d.name), name);
    markPetDirty();
    message = String("Ahoj, ") + pet.d.name + "!";
  } else if (cmd == "reset") {
    pet.reset();
    updateLook();
    savePetNow();
    ghost.trigger(GA_LEVELUP);
    message = "Narodil se nový duch!";
  } else {
    message = "Neznámý příkaz";
    return false;
  }
  return true;
}

void appSetScene(int scene) {
  if (scene >= SCENE_COUNT) {
    settings.mode = MODE_SOULS;
  } else {
    settings.mode = MODE_GHOST;
    settings.scene = scene < 0 ? 0 : scene;
  }
  settingsSave();
  updateLook();
  nextFrameAt = 0;
  lastInteraction = millis();
}

bool appCalibrate() {
  if (!imuCalibrate(settings.refX, settings.refY, settings.refZ)) {
    return false;
  }
  settingsSave();
  return true;
}

static String jsonText(const char *s) {
  String out = "\"";
  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\') out += '\\';
    out += *p;
  }
  return out + "\"";
}

void appStatusJson(String &body) {
  ImuState imu = imuState();
  body += ",\"displayMode\":" + String(settings.mode);
  body += ",\"scene\":" + String(settings.scene);
  body += ",\"scenes\":[";
  for (int i = 0; i < SCENE_COUNT; i++) body += (i ? "," : "") + jsonText(sceneName(i));
  body += "],\"eyeColor\":" + String(settings.eyeColor);
  body += ",\"eyeColors\":[";
  for (int i = 0; i < EYES_COUNT; i++) body += (i ? "," : "") + jsonText(eyeColorName(i));
  body += "],\"spooky\":" + String(settings.spooky);
  body += ",\"tiltInvert\":" + String(settings.tiltInvert ? "true" : "false");
  body += ",\"motionSensitivity\":" + String(settings.motionSensitivity);
  body += ",\"imu\":{\"ok\":" + String(imuOk ? "true" : "false") + ",\"lean\":" + String(imu.lean, 2) +
          ",\"calibrated\":" + String(settings.refX || settings.refY || settings.refZ ? "true" : "false") + "}";
  const PetData &d = pet.d;
  body += ",\"pet\":{\"name\":" + jsonText(d.name);
  body += ",\"level\":" + String(d.level) + ",\"title\":" + jsonText(levelTitle(d.level));
  body += ",\"xp\":" + String(d.xp) + ",\"xpLevel\":" + String(pet.xpForLevel(d.level)) +
          ",\"xpNext\":" + String(d.level < PET_MAX_LEVEL ? pet.xpForLevel(d.level + 1) : 0);
  body += ",\"food\":" + String((int)d.food) + ",\"fun\":" + String((int)d.fun) + ",\"energy\":" + String((int)d.energy);
  body += ",\"mood\":" + jsonText(moodName(pet.mood(ghost.sleeping())));
  body += ",\"asleep\":" + String(ghost.sleeping() ? "true" : "false");
  body += ",\"ageMinutes\":" + String(d.ageMinutes) + "}";
}

// ------------------------------------------------------------------ input

// Optional BOOT button: short press = next soul / pet the ghost, long press = Wi-Fi hotspot.
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
    pauseUntil = millis() + 2000;  // keep the message visible
    lastInteraction = millis();
  } else if (!pressed && down) {
    down = false;
    if (!longHandled && millis() - since > 50) {
      if (settings.mode == MODE_SOULS) {
        appSelectSoul((settings.soul + 1) % soulCount());
      } else {
        String msg;
        appPetCommand("pet", "", msg);
      }
    }
  }
}

static void handleImuEvents() {
  ImuEvent e;
  while ((e = imuPollEvent()) != IMU_NONE) {
    lastInteraction = millis();
    if (settings.mode == MODE_SOULS) {
      if (e == IMU_DOUBLE_TAP) appSelectSoul((settings.soul + 1) % soulCount());
      continue;
    }
    bool wasAsleep = ghost.sleeping();
    if (wasAsleep) {
      setNap(false);  // any movement wakes the ghost (it yawns)
    }
    switch (e) {
      case IMU_SHAKE:
        if (!wasAsleep) handlePetResult(pet.shake(), GA_SCARED);
        break;
      case IMU_STRONG_SHAKE:
        handlePetResult(pet.shake(), GA_DIZZY);
        break;
      case IMU_DOUBLE_TAP:
        if (!wasAsleep) handlePetResult(pet.pet(), GA_LOVE);
        break;
      default:
        break;
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
    appSleepNow();
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

// ------------------------------------------------------------------ modes

static void drawStatusIcons(Canvas &c) {
  if (battery().present && battery().millivolts < BAT_LOW_MV) {
    c.roundRect(104, 204, 28, 14, 3, {255, 255, 255});
    c.rect(106, 206, 24, 10, {0, 0, 0});
    c.rect(132, 208, 3, 6, {255, 255, 255});
    c.rect(108, 208, 5, 6, {255, 60, 60});
  }
  if (netActive()) {
    c.circle(120, 12, 3.5f, {80, 230, 255});
  }
}

static void ghostLoop() {
  uint32_t now = millis();
  uint32_t interval = ghost.sleeping() ? 100 : 40;
  if (now - lastGhostFrame < interval || (int32_t)(now - pauseUntil) < 0) {
    delay(1);
    return;
  }
  float dt = lastGhostFrame ? (now - lastGhostFrame) / 1000.0f : 0.04f;
  if (dt > 0.25f) dt = 0.25f;  // after a pause (OTA message, Wi-Fi) do not jump
  lastGhostFrame = now;

  ImuState imu = imuState();
  if (imuOk) {
    // Naps when lying still, wakes up through IMU events.
    if (!ghost.sleeping() && imu.stillMs > NAP_AFTER_STILL_MS && !ghost.busy()) {
      setNap(true);
    }
  }
  if (!ghost.sleeping() && pet.exhausted() && !ghost.busy() && now - lastWokenAt > 60000) {
    setNap(true);  // too tired, falls asleep on its own
  }
  if (queuedAction != GA_NONE && !ghost.busy()) {
    ghost.trigger(queuedAction);
    queuedAction = GA_NONE;
  }

  pet.update(dt, ghost.sleeping());
  ghost.setNeeds(pet.hungry(), pet.bored(), pet.tired());
  GhostInput in;
  if (imuOk) {
    in.lean = imu.lean;
    in.upsideDown = imu.upsideDown;
    in.lyingFlat = imu.lyingFlat;
  }
  ghost.update(dt, in, look);

  if (!settings.displayOn) {
    return;
  }
  Canvas canvas(displayBuffer(), SCREEN_W, SCREEN_H);
  ghost.render(canvas, look);
  drawStatusIcons(canvas);
  displayPushNative();
}

static void soulsLoop() {
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

static void petHousekeeping() {
  float rx, ry, rz;
  if (imuReferenceChanged(rx, ry, rz)) {
    settings.refX = rx, settings.refY = ry, settings.refZ = rz;
    settingsSave();
  }
  if (!offlineApplied && timeValid()) {
    offlineApplied = true;
    uint32_t now = time(nullptr);
    if (pet.d.savedEpoch && now > pet.d.savedEpoch) {
      pet.applyOffline(now - pet.d.savedEpoch);
      markPetDirty();
    }
  }
  uint32_t now = millis();
  if ((petDirty && now - petDirtySince > 5000) || now - lastPetSave > PET_SAVE_INTERVAL_MS) {
    savePetNow();
  }
}

// ------------------------------------------------------------------ setup / loop

void setup() {
  Serial.begin(115200);
  Serial.printf("\nSoulCage %s\n", FW_VERSION);
  otaBootCheck();
  settingsLoad();
  if (settings.soul >= soulCount()) {
    settings.soul = 0;
  }
  if (!petLoad(pet.d) || !pet.valid()) {
    pet.reset();
  }

  // Power-cycle gesture (works without any button in the case):
  // on -> off -> on within 20 s = next soul / scene, three times = Wi-Fi rescue hotspot.
  bool rescue = false;
  uint8_t quick = quickBootCount();
  if (quick == 2) {
    if (settings.mode == MODE_SOULS) {
      settings.soul = (settings.soul + 1) % soulCount();
    } else {
      settings.scene = (settings.scene + 1) % SCENE_COUNT;
    }
    settingsSave();
  } else if (quick == 3) {
    // undo the change of boot 2
    if (settings.mode == MODE_SOULS) {
      settings.soul = (settings.soul + soulCount() - 1) % soulCount();
    } else {
      settings.scene = (settings.scene + SCENE_COUNT - 1) % SCENE_COUNT;
    }
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
    appSleepNow();
  }

  if (rescue) {
    displayMessage("Wi-Fi hotspot", "http://192.168.4.1");
    delay(2500);
  }

  imuOk = imuInit(settings.refX, settings.refY, settings.refZ, settings.tiltInvert, settings.motionSensitivity);
  updateLook();
  netInit();
  if (rescue || settings.wifiPolicy != WIFI_RESCUE_ONLY) {
    netStart(rescue);
  }

  playingIntro = settings.playIntro;
  if (!settings.displayOn) {
    displaySetPower(false);
  }
  lastInteraction = millis();
  lastPetSave = millis();
  if (settings.mode == MODE_GHOST) {
    ghost.trigger(GA_WAVE);  // say hello
  }
}

void loop() {
  otaLoop();
  quickBootTick();
  netLoop();
  handleButton();
  handleImuEvents();
  checkBattery();
  petHousekeeping();

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
    appSleepNow();
  }

  if (settings.mode == MODE_GHOST) {
    ghostLoop();
  } else {
    soulsLoop();
  }
}
