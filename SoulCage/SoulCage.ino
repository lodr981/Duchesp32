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
#include "greeting.h"
#include "hunt.h"
#include "imu.h"
#include "maze.h"
#include "net.h"
#include "ota.h"
#include "pet.h"
#include "power.h"
#include "settings.h"
#include "soul.h"

// TLS for OTA downloads needs more than the default 8 KB loop stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#define NAP_AFTER_STILL_MS 120000UL
#define PET_SAVE_INTERVAL_MS 300000UL

// Original souls mode
static uint16_t frameIndex;
static uint32_t nextFrameAt;
static SoulDirector soulDirector;
static int directorSoul = -1;
static uint8_t *soulScratch;  // decoded frame (PSRAM), so the photo can be shifted and flipped
static uint32_t lastSoulFrame;

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

// After switching on: QR for the hotspot until a phone joins, then QR for the app.
enum QrStage : uint8_t { QR_NONE, QR_WIFI, QR_APP };
static QrStage qrStage = QR_NONE;
static uint32_t qrSince, qrLastDraw;

static MazeGame maze;
static HuntData hunt;
static HuntOverlay huntFx;
static uint32_t lastMazeFrame;

// Surprise mode: items 0..SCENE_COUNT-1 are ghost scenes, then the trapped souls.
static uint32_t showUntil, showSwitchAt;
static int showPending = -1;
static int8_t showRecent[3] = {-1, -1, -1};
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

static void wakeSouls() {
  if (soulDirector.sleepy()) {
    soulDirector.setSleepy(false);
    displaySetBrightness(settings.brightness);
  }
}

static void stopSurprise() {
  showPending = -1;
  if (settings.autoShow) {
    settings.autoShow = false;
    settingsSave();
  }
}

void appSelectSoul(int index) {
  wakeSouls();
  stopSurprise();
  settings.soul = index;
  settings.mode = MODE_SOULS;
  settingsSave();
  frameIndex = 0;
  nextFrameAt = 0;
  lastInteraction = millis();
}

bool appSoulAction(const String &action) {
  SoulEvent e;
  if (action == "bang") e = SOUL_SHAKE;
  else if (action == "knock") e = SOUL_KNOCK;
  else if (action == "escape") e = SOUL_STRONG_SHAKE;
  else return false;
  stopSurprise();
  if (settings.mode != MODE_SOULS) {
    settings.mode = MODE_SOULS;
    settingsSave();
  }
  wakeSouls();
  soulDirector.event(e);
  lastInteraction = millis();
  return true;
}

static void enforceSoulsOnly() {
  if ((settings.soulsOnly || hunt.active) && settings.mode != MODE_SOULS) {
    settings.mode = MODE_SOULS;
  }
}

void appApplySettings() {
  displaySetBrightness(ghost.sleeping() ? max(5, settings.brightness / 4) : settings.brightness);
  if (settings.wifiPolicy == WIFI_ALWAYS && !netActive()) {
    netStart(false);
  }
  imuConfigure(settings.tiltInvert, settings.motionSensitivity);
  enforceSoulsOnly();
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
  wakeSouls();
  stopSurprise();
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

static MazeMapping mazeMapping() {
  MazeMapping m;
  m.axisX = settings.mazeAxisX;
  m.axisY = settings.mazeAxisY;
  m.signX = settings.mazeSignX;
  m.signY = settings.mazeSignY;
  m.calibrated = settings.mazeCalibrated;
  return m;
}

bool appGameCommand(const String &cmd, const String &arg, String &message) {
  lastInteraction = millis();
  if (cmd == "stop") {
    maze.stop();
    lastGhostFrame = lastSoulFrame = 0;
    message = "Hra ukončena";
    return true;
  }
  if (!imuOk) {
    message = "Pohybový senzor nenalezen, labyrint nejde hrát";
    return false;
  }
  if (cmd != "start" && cmd != "calibrate") {
    message = "Neznámý příkaz";
    return false;
  }
  int unlocked = settings.mazeUnlocked < 1 ? 1 : settings.mazeUnlocked;
  int level = arg.length() ? arg.toInt() : unlocked;
  if (level < 1 || level > unlocked) {
    message = "Tenhle level je ještě zamčený 🔒";
    return false;
  }
  wakeSouls();
  if (ghost.sleeping()) {
    setNap(false);
  }
  qrStage = QR_NONE;
  maze.start(level, mazeMapping());
  if (cmd == "calibrate") {
    maze.recalibrate();
    message = "Podívej se na přívěsek a nakloň ho, jak říká";
  } else {
    message = settings.mazeCalibrated ? String("Level ") + level + " – hodně štěstí!" : "Nejdřív nastavíme ovládání – koukni na přívěsek";
  }
  return true;
}

// "Ahoj Amelko!" -> "Děkuju, Amelko!"
static String thanksText() {
  String g = settings.greeting;
  g.trim();
  if (g.startsWith("Ahoj ")) {
    String name = g.substring(5);
    name.replace("!", "");
    name.trim();
    if (name.length()) return "Děkuju, " + name + "!";
  }
  return "Děkuju!";
}

static void showSouls() {
  if (settings.mode != MODE_SOULS) {
    settings.mode = MODE_SOULS;
    settingsSave();
  }
  wakeSouls();
  maze.stop();
  qrStage = QR_NONE;
}

bool appHuntCommand(const String &cmd, const String &arg, String &message) {
  lastInteraction = millis();
  if (cmd == "stop") {
    hunt.active = 0;
    huntSave(hunt);
    message = "Hra ukončena";
    return true;
  }
  if (cmd != "new") {
    message = "Neznámý příkaz";
    return false;
  }
  int count = arg.toInt();
  if (count < 3) count = 3;
  if (count > HUNT_MAX_SEALS) count = HUNT_MAX_SEALS;
  hunt.version = HUNT_DATA_VERSION;
  hunt.active = 1;
  hunt.count = count;
  hunt.found = 0;
  const char *alphabet = "abcdefghjkmnpqrstuvwxyz23456789";  // no look-alike characters
  for (int i = 0; i < HUNT_MAX_SEALS; i++) {
    for (int k = 0; k < HUNT_CODE_LEN; k++) hunt.codes[i][k] = alphabet[esp_random() % strlen(alphabet)];
    hunt.codes[i][HUNT_CODE_LEN] = 0;
  }
  huntSave(hunt);
  showSouls();
  message = String("Nová hra se ") + count + " pečetěmi. Vytiskni je a schovej!";
  return true;
}

static String scanPage(const char *emoji, const String &title, const String &text, const String &hint, bool party) {
  int found = huntFoundCount(hunt);
  String dots;
  for (int i = 0; i < hunt.count; i++) dots += ((hunt.found >> i) & 1) ? "🔓" : "🔒";
  String html = F("<!doctype html><html lang=cs><head><meta charset=utf-8>"
                  "<meta name=viewport content='width=device-width,initial-scale=1'><title>Osvoboď duši</title><style>"
                  "body{margin:0;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;"
                  "text-align:center;padding:24px;color:#f6f0ff;font:18px/1.45 ui-rounded,-apple-system,sans-serif;"
                  "background:radial-gradient(120% 70% at 50% 0%,#4a2280,#140b24 65%)}"
                  ".e{font-size:96px;animation:pop .9s cubic-bezier(.2,1.6,.4,1) both}"
                  "@keyframes pop{0%{transform:scale(0) rotate(-30deg)}100%{transform:scale(1)}}"
                  "h1{font-size:30px;margin:12px 0 4px}.dots{font-size:26px;letter-spacing:4px;margin:10px 0}"
                  ".hint{margin-top:18px;padding:16px;border-radius:20px;background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.15);max-width:420px}"
                  ".hint b{display:block;color:#ff7ac8;font-size:14px;letter-spacing:.08em;text-transform:uppercase;margin-bottom:6px}"
                  "a{color:#b79bff;margin-top:26px;display:inline-block}"
                  ".c{position:fixed;top:-20px;font-size:24px;animation:fall linear forwards}"
                  "@keyframes fall{to{transform:translateY(110vh) rotate(540deg)}}</style></head><body>");
  html += "<div class=e>" + String(emoji) + "</div><h1>" + title + "</h1><div>" + text + "</div>";
  if (hunt.count) html += "<div class=dots>" + dots + "</div><div>" + String(found) + " z " + String(hunt.count) + "</div>";
  if (hint.length()) {
    String safe = hint;
    safe.replace("&", "&amp;");
    safe.replace("<", "&lt;");
    html += "<div class=hint><b>Nápověda</b>" + safe + "</div>";
  }
  html += F("<a href='/'>Otevřít aplikaci</a>");
  if (party) {
    html += F("<script>for(let i=0;i<60;i++){const s=document.createElement('div');s.className='c';"
              "s.textContent=['✨','🎉','💜','⭐','👻'][i%5];s.style.left=Math.random()*100+'vw';"
              "s.style.animationDuration=(2+Math.random()*3)+'s';s.style.animationDelay=Math.random()*2+'s';"
              "document.body.appendChild(s)}</script>");
  }
  return html + F("</body></html>");
}

String appHuntScan(const String &code) {
  lastInteraction = millis();
  int seal = huntSealForCode(hunt, code.c_str());
  if (seal < 0) {
    return scanPage("🤔", "Tohle není pečeť", "Tenhle kód do současné hry nepatří.", "", false);
  }
  if (!hunt.active) {
    return scanPage("✨", "Duše už je volná", "Tahle hra skončila. Rodiče můžou v aplikaci připravit novou.", "", false);
  }
  if ((hunt.found >> seal) & 1) {
    return scanPage("😉", "Tuhle už máš", "Pečeť č. " + String(seal + 1) + " je zlomená už z dřívějška.", huntHint(seal + 1), false);
  }
  hunt.found |= 1 << seal;
  showSouls();
  if (huntFoundCount(hunt) >= hunt.count) {
    hunt.active = 0;
    hunt.freed++;
    huntSave(hunt);
    soulDirector.event(SOUL_FREE);
    huntFx.freed();
    PetResult r = pet.gameWin();
    if (r == PET_LEVEL_UP) queuedAction = GA_LEVELUP;
    markPetDirty();
    return scanPage("🕊️", "Duše je volná!", "Zlomila jsi všechny pečetě. " + thanksText(), "", true);
  }
  huntSave(hunt);
  soulDirector.event(SOUL_SEAL);
  huntFx.sealBroken(seal);
  return scanPage("🔓", "Pečeť zlomena!", "Podívej se na přívěsek – duše cítí svobodu!", huntHint(seal + 1), false);
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
  body += ",\"soulsOnly\":" + String(settings.soulsOnly ? "true" : "false");
  body += ",\"hunt\":{\"active\":" + String(hunt.active ? "true" : "false") + ",\"count\":" + String(hunt.count) +
          ",\"found\":" + String(huntFoundCount(hunt)) + ",\"mask\":" + String(hunt.found) +
          ",\"freed\":" + String(hunt.freed) + ",\"hints\":[";
  for (int i = 0; i <= HUNT_MAX_SEALS; i++) body += (i ? "," : "") + jsonText(huntHint(i).c_str());
  body += "]}";
  body += ",\"game\":{\"active\":" + String(maze.active() ? "true" : "false") + ",\"level\":" + String(maze.level()) +
          ",\"unlocked\":" + String(settings.mazeUnlocked) + ",\"calibrated\":" +
          String(settings.mazeCalibrated ? "true" : "false") + ",\"stars\":[";
  for (int i = 0; i < MAZE_LEVELS; i++) body += (i ? "," : "") + String(settings.mazeStars[i]);
  body += "]}";
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
      displayMessage(settings.deviceName.c_str(), "Wi-Fi zapnuto · 192.168.4.1");
    }
    nextFrameAt = millis() + 2000;
    pauseUntil = millis() + 2000;  // keep the message visible
    lastInteraction = millis();
  } else if (!pressed && down) {
    down = false;
    if (!longHandled && millis() - since > 50) {
      if (qrStage != QR_NONE) {
        qrStage = QR_NONE;
        return;
      }
      if (maze.active()) {
        maze.stop();
        lastGhostFrame = lastSoulFrame = 0;
        return;
      }
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
    if (maze.active()) {
      continue;  // tilting the maze shakes the pendant a lot, ignore gestures
    }
    if (qrStage != QR_NONE && (e == IMU_SHAKE || e == IMU_DOUBLE_TAP || e == IMU_STRONG_SHAKE)) {
      qrStage = QR_NONE;  // shake or tap skips the QR screen
      continue;
    }
    if (settings.mode == MODE_SOULS) {
      bool wasSleepy = soulDirector.sleepy();
      switch (e) {
        case IMU_SHAKE: soulDirector.event(SOUL_SHAKE); break;
        case IMU_STRONG_SHAKE: soulDirector.event(SOUL_STRONG_SHAKE); break;
        case IMU_TAP: soulDirector.event(SOUL_TAP); break;
        case IMU_DOUBLE_TAP: soulDirector.event(SOUL_DOUBLE_TAP); break;
        case IMU_MOTION:
          if (wasSleepy) soulDirector.event(SOUL_WAKE);
          break;
        default: break;
      }
      if (wasSleepy && !soulDirector.sleepy()) {
        displaySetBrightness(settings.brightness);
      }
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
      case IMU_TAP:
        if (!wasAsleep && !ghost.busy()) ghost.trigger(GA_WAVE);
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
    displayMessage("Slabá baterie", "Nabij mě prosím");
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
  const char *line = ota.state == OTA_DOWNLOADING ? "Stahuji nový firmware…"
                     : ota.state == OTA_DONE      ? "Hotovo, restartuji"
                                                  : "Připravuji…";
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

// The trapped souls: clips, reactions to motion and effects on the glass.
static void soulsDirectorLoop() {
  uint32_t now = millis();
  uint32_t interval = soulDirector.sleepy() ? 80 : 40;
  if (now - lastSoulFrame < interval || (int32_t)(now - pauseUntil) < 0) {
    delay(1);
    return;
  }
  float dt = lastSoulFrame ? (now - lastSoulFrame) / 1000.0f : 0.04f;
  if (dt > 0.25f) dt = 0.25f;
  lastSoulFrame = now;

  const Animation &anim = soulAt(settings.soul);
  if (directorSoul != settings.soul) {
    directorSoul = settings.soul;
    soulDirector.setSoul(anim.id, anim.frameCount);
  }
  ImuState imu = imuState();
  GhostInput in;
  if (imuOk) {
    in.lean = imu.lean;
    in.upsideDown = imu.upsideDown;
    if (!soulDirector.sleepy() && imu.stillMs > NAP_AFTER_STILL_MS) {
      soulDirector.setSleepy(true);
      displaySetBrightness(max(5, settings.brightness / 4));
    }
  }
  soulDirector.update(dt, settings.frameDelay / 1000.0f, in, look);
  if (!settings.displayOn) {
    return;
  }
  uint8_t *src = soulScratch ? soulScratch : (uint8_t *)displayBuffer();
  if (!decodeFrame(anim, soulDirector.frame(), src, SCREEN_W * SCREEN_H * 2)) {
    Serial.printf("Frame %u of %s failed\n", soulDirector.frame(), anim.id);
    return;
  }
  Canvas canvas(displayBuffer(), SCREEN_W, SCREEN_H);
  soulDirector.render(src, canvas, look);
  huntFx.update(dt);
  String thanks = thanksText();
  huntFx.render(canvas, hunt, thanks.c_str());
  drawStatusIcons(canvas);
  displayPushNative();
}

static void soulsLoop() {
  soulsDirectorLoop();
}

// ------------------------------------------------------------------ surprise mode

static int currentShowItem() {
  return settings.mode == MODE_SOULS ? SCENE_COUNT + settings.soul : settings.scene;
}

static uint32_t showDuration(int item) {
  uint32_t r = esp_random();
  return item < SCENE_COUNT ? 70000 + r % 80000 : 35000 + r % 45000;
}

// Weighted random pick: no immediate repeats, souls less often in cute mode,
// and the ghost gets more time when the pet needs care.
static int pickShowItem() {
  int n = SCENE_COUNT + soulCount();
  float weights[16];
  float total = 0;
  int current = currentShowItem();
  for (int i = 0; i < n && i < 16; i++) {
    float w = i < SCENE_COUNT ? 1.0f : settings.spooky == 0 ? 0.3f : settings.spooky == 1 ? 0.7f : 1.2f;
    if (settings.soulsOnly || hunt.active) w = i < SCENE_COUNT ? 0.0f : 1.0f;  // only the trapped souls
    if (i >= SCENE_COUNT && (pet.hungry() || pet.bored() || pet.tired())) w *= 0.3f;
    for (int8_t r : showRecent) {
      if (r == i) w *= 0.1f;
    }
    if (i == current) w = 0;
    weights[i] = w;
    total += w;
  }
  float pick = (esp_random() % 10000) / 10000.0f * total;
  for (int i = 0; i < n && i < 16; i++) {
    pick -= weights[i];
    if (pick <= 0 && weights[i] > 0) return i;
  }
  return settings.soulsOnly || hunt.active ? SCENE_COUNT : 0;
}

static void applyShowItem(int item) {
  showRecent[2] = showRecent[1];
  showRecent[1] = showRecent[0];
  showRecent[0] = item;
  if (item < SCENE_COUNT) {
    bool fromSouls = settings.mode == MODE_SOULS;
    settings.mode = MODE_GHOST;
    settings.scene = item;
    updateLook();
    if (fromSouls) ghost.appear();  // after "hide" it is already reappearing
  } else {
    settings.mode = MODE_SOULS;
    settings.soul = item - SCENE_COUNT;
      const Animation &anim = soulAt(settings.soul);
    directorSoul = settings.soul;
    soulDirector.setSoul(anim.id, anim.frameCount);
    soulDirector.event(SOUL_KNOCK);  // "hello, I'm in here"
  }
  showUntil = millis() + showDuration(item);
}

static void surpriseTick() {
  if (!settings.autoShow || otaBusy() || maze.active()) {
    return;
  }
  uint32_t now = millis();
  if (showPending >= 0) {
    if ((int32_t)(now - showSwitchAt) >= 0) {
      applyShowItem(showPending);
      showPending = -1;
    }
    return;
  }
  if (!showUntil) {
    showUntil = now + showDuration(currentShowItem());
  }
  if ((int32_t)(now - showUntil) < 0) {
    return;
  }
  // Do not interrupt a nap or a running action.
  bool busy = settings.mode == MODE_GHOST ? ghost.busy() || ghost.sleeping() : soulDirector.sleepy();
  if (busy) {
    showUntil = now + 5000;
    return;
  }
  showPending = pickShowItem();
  if (settings.mode == MODE_GHOST) {
    ghost.trigger(GA_HIDE);  // vanishes, the scene changes while it is invisible
    showSwitchAt = now + 1000;
  } else {
    soulDirector.event(SOUL_VANISH);
    showSwitchAt = now + 1100;
  }
}

static void mazeRecordWin(bool allDone) {
  int lvl = maze.level();
  if (lvl >= 1 && lvl <= MAZE_LEVELS && maze.lastStars() > settings.mazeStars[lvl - 1]) {
    settings.mazeStars[lvl - 1] = maze.lastStars();
  }
  if (!allDone && lvl + 1 > settings.mazeUnlocked) {
    settings.mazeUnlocked = lvl + 1;
  }
  settingsSave();
  PetResult r = pet.gameWin();
  if (r == PET_LEVEL_UP) {
    queuedAction = GA_LEVELUP;
    updateLook();
  }
  markPetDirty();
}

// The tilt maze replaces the animation while it runs.
static bool mazeLoop() {
  if (!maze.active()) {
    return false;
  }
  uint32_t now = millis();
  if (now - lastMazeFrame < 33) {
    delay(1);
    return true;
  }
  float dt = lastMazeFrame ? (now - lastMazeFrame) / 1000.0f : 0.033f;
  if (dt > 0.1f) dt = 0.1f;
  lastMazeFrame = now;
  lastInteraction = now;
  ImuState imu = imuState();
  switch (maze.update(dt, imu.gx, imu.gy, imu.gz, ghost, look)) {
    case MAZE_EV_CALIBRATED: {
      const MazeMapping &m = maze.mapping();
      settings.mazeAxisX = m.axisX;
      settings.mazeAxisY = m.axisY;
      settings.mazeSignX = m.signX;
      settings.mazeSignY = m.signY;
      settings.mazeCalibrated = true;
      settingsSave();
      break;
    }
    case MAZE_EV_LEVEL_DONE: mazeRecordWin(false); break;
    case MAZE_EV_ALL_DONE: mazeRecordWin(true); break;
    default: break;
  }
  if (maze.playing() && imu.stillMs > 120000) {
    maze.stop();  // forgotten game
  }
  if (!maze.active()) {
    lastGhostFrame = lastSoulFrame = 0;
    return false;
  }
  if (settings.displayOn) {
    Canvas canvas(displayBuffer(), SCREEN_W, SCREEN_H);
    maze.render(canvas, ghost, look);
    displayPushNative();
  }
  return true;
}

// Returns true while a QR screen is shown instead of the animation.
static bool qrLoop() {
  if (qrStage == QR_NONE) {
    return false;
  }
  uint32_t now = millis();
  float t = (now - qrSince) / 1000.0f;
  if (!netActive() || !settings.displayOn) {
    qrStage = QR_NONE;
  } else if (qrStage == QR_WIFI && netClients() > 0) {
    qrStage = QR_APP;
    qrSince = now;
    t = 0;
  } else if (qrStage == QR_WIFI && t > 90) {
    qrStage = QR_NONE;
  } else if (qrStage == QR_APP && ((int32_t)(netUiServedAt() - qrSince) > 0 || t > 45)) {
    qrStage = QR_NONE;
  }
  if (qrStage == QR_NONE) {
    lastGhostFrame = lastSoulFrame = 0;
    return false;
  }
  if (now - qrLastDraw >= 200) {
    qrLastDraw = now;
    Canvas canvas(displayBuffer(), SCREEN_W, SCREEN_H);
    if (qrStage == QR_WIFI) {
      char payload[160];
      String ssid = netHotspotName();
      wifiQrPayload(payload, sizeof(payload), ssid.c_str(), settings.password.length() >= 8 ? settings.password.c_str() : "");
      drawQr(canvas, payload, "Připoj se ke mně", "Namiř foťák iPhonu", t);
    } else {
      drawQr(canvas, "http://192.168.4.1/", "Otevři aplikaci", "Ťukni na odkaz", t);
    }
    displayPushNative();
  }
  delay(2);
  return true;
}

static void playGreeting() {
  if (settings.greeting.isEmpty() || !settings.displayOn || !displayBuffer()) {
    return;
  }
  GhostLook greetLook = look;
  greetLook.spooky = 0;
  GhostInput none;
  Canvas canvas(displayBuffer(), SCREEN_W, SCREEN_H);
  if (settings.mode == MODE_SOULS) {
    // The trapped soul comes to the glass while the greeting appears.
    const Animation &anim = soulAt(settings.soul);
    directorSoul = settings.soul;
    soulDirector.setSoul(anim.id, anim.frameCount);
    soulDirector.event(SOUL_KNOCK);
    uint8_t *src = soulScratch ? soulScratch : (uint8_t *)displayBuffer();
    uint32_t start = millis(), last = start;
    for (;;) {
      uint32_t now = millis();
      float t = (now - start) / 1000.0f;
      if (t > GREETING_SECONDS) break;
      soulDirector.update((now - last) / 1000.0f, settings.frameDelay / 1000.0f, none, greetLook);
      last = now;
      if (decodeFrame(anim, soulDirector.frame(), src, SCREEN_W * SCREEN_H * 2)) {
        soulDirector.render(src, canvas, greetLook);
        drawGreetingText(canvas, settings.greeting.c_str(), settings.subtitle.c_str(), t);
        displayPushNative();
      }
      delay(5);
    }
    return;
  }
  ghost.trigger(GA_WAVE);
  uint32_t start = millis(), last = start;
  bool loved = false;
  for (;;) {
    uint32_t now = millis();
    float t = (now - start) / 1000.0f;
    if (t > GREETING_SECONDS) break;
    if (!loved && t > 2.0f) {
      ghost.trigger(GA_LOVE);
      loved = true;
    }
    ghost.update((now - last) / 1000.0f, none, greetLook);
    last = now;
    drawGreeting(canvas, ghost, greetLook, settings.greeting.c_str(), settings.subtitle.c_str(), t);
    displayPushNative();
    delay(5);
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
    displayMessage("Slabá baterie", "Nabij mě prosím");
    delay(3000);
    appSleepNow();
  }

  if (rescue) {
    displayMessage(settings.deviceName.c_str(), "Wi-Fi zapnuto · 192.168.4.1");
    delay(2500);
  }

  soulScratch = (uint8_t *)heap_caps_malloc(SCREEN_W * SCREEN_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!huntLoad(hunt)) {
    memset(&hunt, 0, sizeof(hunt));
    hunt.version = HUNT_DATA_VERSION;
  }
  enforceSoulsOnly();
  updateLook();
  playGreeting();

  imuOk = imuInit(settings.refX, settings.refY, settings.refZ, settings.tiltInvert, settings.motionSensitivity);
  updateLook();
  netInit();
  if (rescue || settings.wifiPolicy != WIFI_RESCUE_ONLY) {
    netStart(rescue);
  }

  if (!settings.displayOn) {
    displaySetPower(false);
  }
  lastInteraction = millis();
  lastPetSave = millis();
  if (settings.qrOnBoot && netActive() && displayBuffer()) {
    qrStage = QR_WIFI;
    qrSince = millis();
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
  surpriseTick();

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

  if (mazeLoop()) {
    return;
  }
  if (qrLoop()) {
    return;
  }
  if (settings.mode == MODE_GHOST) {
    ghostLoop();
  } else {
    soulsLoop();
  }
}
