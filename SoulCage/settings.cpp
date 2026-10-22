#include "settings.h"
#include <Preferences.h>

Settings settings;

static const char *NS = "soulcage";

void settingsLoad() {
  Preferences p;
  if (!p.begin(NS, true)) {
    return;  // first boot, namespace does not exist yet - keep defaults
  }
  settings.soul = p.getUChar("soul", settings.soul);
  settings.brightness = p.getUChar("bright", settings.brightness);
  settings.frameDelay = p.getUShort("delay", settings.frameDelay);
  settings.playIntro = p.getBool("intro", settings.playIntro);
  settings.displayOn = p.getBool("disp", settings.displayOn);
  settings.sleepMinutes = p.getUShort("sleep", settings.sleepMinutes);
  settings.wifiPolicy = p.getUChar("wpol", settings.wifiPolicy);
  settings.wifiMinutes = p.getUChar("wmin", settings.wifiMinutes);
  settings.hostname = p.getString("host", settings.hostname);
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
  p.putUChar("soul", settings.soul);
  p.putUChar("bright", settings.brightness);
  p.putUShort("delay", settings.frameDelay);
  p.putBool("intro", settings.playIntro);
  p.putBool("disp", settings.displayOn);
  p.putUShort("sleep", settings.sleepMinutes);
  p.putUChar("wpol", settings.wifiPolicy);
  p.putUChar("wmin", settings.wifiMinutes);
  p.putString("host", settings.hostname);
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
