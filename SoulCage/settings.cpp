#include "settings.h"
#include <Preferences.h>
#include "hunt.h"
#include "pet.h"

Settings settings;

static const char *NS = "soulcage";

void settingsLoad() {
  Preferences p;
  if (!p.begin(NS, true)) {
    return;  // first boot, namespace does not exist yet - keep defaults
  }
  settings.mode = p.getUChar("mode", settings.mode);
  settings.scene = p.getUChar("scene", settings.scene);
  settings.eyeColor = p.getUChar("eyes", settings.eyeColor);
  settings.spooky = p.getUChar("spooky", settings.spooky);
  settings.tiltInvert = p.getBool("tiltinv", settings.tiltInvert);
  settings.motionSensitivity = p.getUChar("msens", settings.motionSensitivity);
  settings.refX = p.getFloat("refx", 0);
  settings.refY = p.getFloat("refy", 0);
  settings.refZ = p.getFloat("refz", 0);
  settings.soul = p.getUChar("soul", settings.soul);
  settings.brightness = p.getUChar("bright", settings.brightness);
  settings.frameDelay = p.getUShort("delay", settings.frameDelay);
  settings.displayOn = p.getBool("disp", settings.displayOn);
  settings.sleepMinutes = p.getUShort("sleep", settings.sleepMinutes);
  settings.wifiPolicy = p.getUChar("wpol", settings.wifiPolicy);
  settings.wifiMinutes = p.getUChar("wmin", settings.wifiMinutes);
  settings.hostname = p.getString("host", settings.hostname);
  settings.deviceName = p.getString("devname", settings.deviceName);
  settings.greeting = p.getString("greet", settings.greeting);
  settings.subtitle = p.getString("greet2", settings.subtitle);
  settings.autoShow = p.getBool("auto", settings.autoShow);
  settings.qrOnBoot = p.getBool("qr", settings.qrOnBoot);
  settings.soulsOnly = p.getBool("soulsonly", settings.soulsOnly);
  settings.mazeUnlocked = p.getUChar("mzlvl", settings.mazeUnlocked);
  if (p.getBytesLength("mzstars") == sizeof(settings.mazeStars)) {
    p.getBytes("mzstars", settings.mazeStars, sizeof(settings.mazeStars));
  }
  settings.mazeAxisX = p.getUChar("mzax", settings.mazeAxisX);
  settings.mazeAxisY = p.getUChar("mzay", settings.mazeAxisY);
  settings.mazeSignX = p.getChar("mzsx", settings.mazeSignX);
  settings.mazeSignY = p.getChar("mzsy", settings.mazeSignY);
  settings.mazeCalibrated = p.getBool("mzcal", settings.mazeCalibrated);
  settings.wispBest = p.getULong("wbest", settings.wispBest);
  settings.jarLights = p.getULong("jar", settings.jarLights);
  settings.homeName = p.getString("hname", settings.homeName);
  settings.homeLat = p.getFloat("hlat", settings.homeLat);
  settings.homeLon = p.getFloat("hlon", settings.homeLon);
  settings.radarRange = p.getUChar("rrange", settings.radarRange);
  settings.radarInShow = p.getBool("rshow", settings.radarInShow);
  settings.rainAlert = p.getBool("ralert", settings.rainAlert);
  settings.weatherFx = p.getBool("wfx", settings.weatherFx);
  settings.forecastAlert = p.getBool("falert", settings.forecastAlert);
  settings.planeAlert = p.getBool("palert", settings.planeAlert);
  settings.issAlert = p.getBool("ialert", settings.issAlert);
  settings.password = p.getString("pass", settings.password);
  settings.manifestUrl = p.getString("manifest", settings.manifestUrl);
  for (int i = 0; i < MAX_NETWORKS; i++) {
    settings.networks[i].ssid = p.getString(("ssid" + String(i)).c_str(), "");
    settings.networks[i].pass = p.getString(("wpass" + String(i)).c_str(), "");
  }
  p.end();
}

void settingsSave() {
  Preferences p;
  if (!p.begin(NS, false)) {
    Serial.println("NVS: cannot open for writing");
    return;
  }
  p.putUChar("mode", settings.mode);
  p.putUChar("scene", settings.scene);
  p.putUChar("eyes", settings.eyeColor);
  p.putUChar("spooky", settings.spooky);
  p.putBool("tiltinv", settings.tiltInvert);
  p.putUChar("msens", settings.motionSensitivity);
  p.putFloat("refx", settings.refX);
  p.putFloat("refy", settings.refY);
  p.putFloat("refz", settings.refZ);
  p.putUChar("soul", settings.soul);
  p.putUChar("bright", settings.brightness);
  p.putUShort("delay", settings.frameDelay);
  p.putBool("disp", settings.displayOn);
  p.putUShort("sleep", settings.sleepMinutes);
  p.putUChar("wpol", settings.wifiPolicy);
  p.putUChar("wmin", settings.wifiMinutes);
  p.putString("host", settings.hostname);
  p.putString("devname", settings.deviceName);
  p.putString("greet", settings.greeting);
  p.putString("greet2", settings.subtitle);
  p.putBool("auto", settings.autoShow);
  p.putBool("qr", settings.qrOnBoot);
  p.putBool("soulsonly", settings.soulsOnly);
  p.putUChar("mzlvl", settings.mazeUnlocked);
  p.putBytes("mzstars", settings.mazeStars, sizeof(settings.mazeStars));
  p.putUChar("mzax", settings.mazeAxisX);
  p.putUChar("mzay", settings.mazeAxisY);
  p.putChar("mzsx", settings.mazeSignX);
  p.putChar("mzsy", settings.mazeSignY);
  p.putBool("mzcal", settings.mazeCalibrated);
  p.putULong("wbest", settings.wispBest);
  p.putULong("jar", settings.jarLights);
  p.putString("hname", settings.homeName);
  p.putFloat("hlat", settings.homeLat);
  p.putFloat("hlon", settings.homeLon);
  p.putUChar("rrange", settings.radarRange);
  p.putBool("rshow", settings.radarInShow);
  p.putBool("ralert", settings.rainAlert);
  p.putBool("wfx", settings.weatherFx);
  p.putBool("falert", settings.forecastAlert);
  p.putBool("palert", settings.planeAlert);
  p.putBool("ialert", settings.issAlert);
  p.putString("pass", settings.password);
  p.putString("manifest", settings.manifestUrl);
  for (int i = 0; i < MAX_NETWORKS; i++) {
    p.putString(("ssid" + String(i)).c_str(), settings.networks[i].ssid);
    p.putString(("wpass" + String(i)).c_str(), settings.networks[i].pass);
  }
  p.end();
}

uint8_t persistGetU8(const char *key, uint8_t def) {
  Preferences p;
  if (!p.begin(NS, true)) {
    return def;
  }
  uint8_t v = p.getUChar(key, def);
  p.end();
  return v;
}

void persistSetU8(const char *key, uint8_t value) {
  Preferences p;
  if (p.begin(NS, false)) {
    p.putUChar(key, value);
    p.end();
  }
}

bool petLoad(PetData &data) {
  Preferences p;
  if (!p.begin(NS, true)) {
    return false;
  }
  bool ok = p.getBytesLength("pet") == sizeof(PetData) && p.getBytes("pet", &data, sizeof(PetData)) == sizeof(PetData);
  p.end();
  return ok;
}

void petSave(const PetData &data) {
  Preferences p;
  if (p.begin(NS, false)) {
    p.putBytes("pet", &data, sizeof(PetData));
    p.end();
  }
}

bool huntLoad(HuntData &data) {
  Preferences p;
  if (!p.begin(NS, true)) {
    return false;
  }
  bool ok = p.getBytesLength("hunt") == sizeof(HuntData) && p.getBytes("hunt", &data, sizeof(HuntData)) == sizeof(HuntData);
  p.end();
  return ok && data.version == HUNT_DATA_VERSION;
}

void huntSave(const HuntData &data) {
  Preferences p;
  if (p.begin(NS, false)) {
    p.putBytes("hunt", &data, sizeof(HuntData));
    p.end();
  }
}

String huntHint(int index) {
  Preferences p;
  if (!p.begin("hunt", true)) {
    return "";
  }
  String s = p.getString(("h" + String(index)).c_str(), "");
  p.end();
  return s;
}

void huntSetHint(int index, const String &text) {
  Preferences p;
  if (p.begin("hunt", false)) {
    p.putString(("h" + String(index)).c_str(), text.substring(0, 120));
    p.end();
  }
}
