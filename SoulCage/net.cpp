#include "net.h"
#include "animations.h"
#include "app.h"
#include "config.h"
#include "display.h"
#include "ota.h"
#include "power.h"
#include "settings.h"
#include "web_icon.h"
#include "web_ui.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>

enum NetMode : uint8_t { NET_OFF, NET_AP };  // NET_AP: hotspot on (plus a saved network in the background)
enum Pending : uint8_t { PENDING_NONE, PENDING_RECONNECT, PENDING_SLEEP, PENDING_RESTART };

static NetMode mode = NET_OFF;
static WebServer server(80);
static DNSServer dns;
static bool serverStarted, rescueMode, uploadAuthorized, staConnected;
static int networkIndex;
static uint32_t connectStart, lastActivity;
static Pending pending;
static uint32_t pendingAt;

static String apSsid();

static void schedule(Pending what) {
  pending = what;
  pendingAt = millis() + 1000;  // let the HTTP response go out first
}

// ---------------------------------------------------------------- helpers

static String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 2);
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

static String q(const String &s) {
  return "\"" + jsonEscape(s) + "\"";
}

static void sendJson(int code, const String &body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void sendOk() {
  sendJson(200, "{\"ok\":true}");
}

static void sendError(int code, const String &message) {
  sendJson(code, "{\"ok\":false,\"error\":" + q(message) + "}");
}

// The UI sends "Authorization: Basic" itself, so no WWW-Authenticate header:
// Safari would otherwise pop up its own login dialog.
static bool authorized() {
  lastActivity = millis();
  if (server.authenticate(WEB_USER, settings.password.c_str())) {
    return true;
  }
  sendError(401, "unauthorized");
  return false;
}

static long argLong(const char *name, long def, long lo, long hi) {
  if (!server.hasArg(name)) {
    return def;
  }
  return constrain(server.arg(name).toInt(), lo, hi);
}

static String sanitizeHostname(String s) {
  s.toLowerCase();
  String out;
  for (char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      out += c;
    }
  }
  return out.substring(0, 31);
}

static long wifiSecondsLeft() {
  uint32_t minutes = rescueMode ? WIFI_RESCUE_MINUTES : settings.wifiPolicy == WIFI_ALWAYS ? 0 : settings.wifiMinutes;
  if (!minutes) {
    return -1;
  }
  long left = (long)minutes * 60 - (long)(millis() - lastActivity) / 1000;
  return left > 0 ? left : 0;
}

// ---------------------------------------------------------------- handlers

static void handleStatus() {
  if (!authorized()) {
    return;
  }
  const BatteryState &bat = battery();
  OtaStatus ota = otaStatus();
  String souls;
  for (size_t i = 0; i < soulCount(); i++) {
    souls += (i ? "," : "") + q(soulAt(i).name);
  }
  String networks;
  for (int i = 0; i < MAX_NETWORKS; i++) {
    networks += (i ? "," : "") + q(settings.networks[i].ssid);
  }
  String body = "{";
  body += "\"fw\":" + q(FW_VERSION);
  body += ",\"soul\":" + String(settings.soul);
  body += ",\"souls\":[" + souls + "]";
  body += ",\"brightness\":" + String(settings.brightness);
  body += ",\"frameDelay\":" + String(settings.frameDelay);
  body += ",\"intro\":" + String(settings.playIntro ? "true" : "false");
  body += ",\"display\":" + String(settings.displayOn ? "true" : "false");
  body += ",\"sleepMinutes\":" + String(settings.sleepMinutes);
  body += ",\"wifiPolicy\":" + String(settings.wifiPolicy);
  body += ",\"wifiMinutes\":" + String(settings.wifiMinutes);
  body += ",\"wifiOffIn\":" + String(wifiSecondsLeft());
  body += ",\"rescue\":" + String(rescueMode ? "true" : "false");
  body += ",\"apSsid\":" + q(apSsid());
  body += ",\"apIp\":" + q(WiFi.softAPIP().toString());
  body += ",\"clients\":" + String(mode == NET_OFF ? 0 : WiFi.softAPgetStationNum());
  body += ",\"online\":" + String(staConnected ? "true" : "false");
  body += ",\"ssid\":" + q(staConnected ? WiFi.SSID() : "");
  body += ",\"ip\":" + q(staConnected ? WiFi.localIP().toString() : "");
  body += ",\"rssi\":" + String(staConnected ? WiFi.RSSI() : 0);
  body += ",\"deviceName\":" + q(settings.deviceName);
  body += ",\"greeting\":" + q(settings.greeting);
  body += ",\"subtitle\":" + q(settings.subtitle);
  body += ",\"autoShow\":" + String(settings.autoShow ? "true" : "false");
  body += ",\"hostname\":" + q(settings.hostname);
  body += ",\"networks\":[" + networks + "]";
  body += ",\"battery\":{\"present\":" + String(bat.present ? "true" : "false") + ",\"mv\":" + String(bat.millivolts) +
          ",\"percent\":" + String(bat.percent) + "}";
  body += ",\"uptime\":" + String(millis() / 1000);
  body += ",\"manifestUrl\":" + q(settings.manifestUrl);
  body += ",\"defaultPassword\":" + String(settings.password == DEFAULT_PASSWORD ? "true" : "false");
  body += ",\"ota\":{\"state\":" + q(otaStateName(ota.state)) + ",\"progress\":" + String(ota.progress) +
          ",\"message\":" + q(ota.message) + ",\"latest\":" + q(ota.latestVersion) + ",\"notes\":" + q(ota.notes) +
          ",\"available\":" + String(ota.updateAvailable ? "true" : "false") + "}";
  appStatusJson(body);
  body += "}";
  sendJson(200, body);
}

static void handleSoul() {
  if (!authorized()) {
    return;
  }
  if (server.hasArg("action")) {
    appSoulAction(server.arg("action")) ? sendOk() : sendError(400, "Neznámá akce");
    return;
  }
  int count = soulCount();
  int index = settings.soul;
  String step = server.arg("step");
  if (server.hasArg("id")) {
    index = server.arg("id").toInt();
  } else if (step == "next") {
    index = (index + 1) % count;
  } else if (step == "prev") {
    index = (index + count - 1) % count;
  } else if (step == "random" && count > 1) {
    index = (index + 1 + esp_random() % (count - 1)) % count;  // always a different soul
  }
  if (index < 0 || index >= count) {
    sendError(400, "invalid soul");
    return;
  }
  appSelectSoul(index);
  sendJson(200, "{\"ok\":true,\"soul\":" + String(index) + "}");
}

static void handleSettings() {
  if (!authorized()) {
    return;
  }
  settings.brightness = argLong("brightness", settings.brightness, 5, 100);
  settings.frameDelay = argLong("frameDelay", settings.frameDelay, 30, 300);
  settings.playIntro = argLong("intro", settings.playIntro, 0, 1);
  settings.sleepMinutes = argLong("sleepMinutes", settings.sleepMinutes, 0, 1440);
  settings.wifiPolicy = argLong("wifiPolicy", settings.wifiPolicy, 0, 2);
  settings.wifiMinutes = argLong("wifiMinutes", settings.wifiMinutes, 1, 120);
  settings.mode = argLong("displayMode", settings.mode, 0, 1);
  settings.scene = argLong("scene", settings.scene, 0, 4);
  settings.eyeColor = argLong("eyeColor", settings.eyeColor, 0, 3);
  settings.spooky = argLong("spooky", settings.spooky, 0, 2);
  settings.tiltInvert = argLong("tiltInvert", settings.tiltInvert, 0, 1);
  settings.motionSensitivity = argLong("motionSensitivity", settings.motionSensitivity, 0, 2);
  if (server.hasArg("hostname")) {
    String host = sanitizeHostname(server.arg("hostname"));
    if (host.length()) {
      settings.hostname = host;
    }
  }
  if (server.hasArg("deviceName")) {
    String name = server.arg("deviceName");
    name.trim();
    if (name.length() && name != settings.deviceName) {
      settings.deviceName = name;
      schedule(PENDING_RECONNECT);  // new hotspot name
    }
  }
  if (server.hasArg("greeting")) {
    settings.greeting = server.arg("greeting").substring(0, 40);
  }
  if (server.hasArg("subtitle")) {
    settings.subtitle = server.arg("subtitle").substring(0, 40);
  }
  settings.autoShow = argLong("autoShow", settings.autoShow, 0, 1);
  if (server.hasArg("manifestUrl")) {
    String url = server.arg("manifestUrl");
    settings.manifestUrl = url.startsWith("https://") ? url : DEFAULT_MANIFEST_URL;
  }
  settingsSave();
  appApplySettings();
  sendOk();
}

static void handleDisplay() {
  if (!authorized()) {
    return;
  }
  bool on = server.hasArg("on") ? server.arg("on") == "1" : !settings.displayOn;
  appSetDisplay(on);
  sendJson(200, String("{\"ok\":true,\"display\":") + (on ? "true" : "false") + "}");
}

static void handleWifi() {
  if (!authorized()) {
    return;
  }
  for (int i = 0; i < MAX_NETWORKS; i++) {
    String ssidKey = "ssid" + String(i), passKey = "pass" + String(i);
    if (!server.hasArg(ssidKey)) {
      continue;
    }
    String ssid = server.arg(ssidKey);
    ssid.trim();
    // An empty password field keeps the stored one for an unchanged network.
    if (ssid != settings.networks[i].ssid || server.arg(passKey).length()) {
      settings.networks[i].pass = server.arg(passKey);
    }
    settings.networks[i].ssid = ssid;
  }
  settingsSave();
  sendOk();
  schedule(PENDING_RECONNECT);
}

static void handlePassword() {
  if (!authorized()) {
    return;
  }
  String pass = server.arg("password");
  if (pass.length() < 8 || pass.length() > 63) {
    sendError(400, "Heslo musí mít 8–63 znaků");
    return;
  }
  settings.password = pass;
  settingsSave();
  sendOk();
  schedule(PENDING_RECONNECT);  // the hotspot uses the same password
}

static void handleOtaCheck() {
  if (!authorized()) {
    return;
  }
  if (!staConnected) {
    sendError(409, "Přívěsek není na internetu. V Nastavení → Wi-Fi přidej domácí Wi-Fi nebo hotspot iPhonu.");
    return;
  }
  otaStartCheck() ? sendOk() : sendError(409, "Probíhá jiná operace");
}

static void handleOtaInstall() {
  if (!authorized()) {
    return;
  }
  if (!staConnected) {
    sendError(409, "Přívěsek není na internetu. V Nastavení → Wi-Fi přidej domácí Wi-Fi nebo hotspot iPhonu.");
    return;
  }
  otaStartInstall() ? sendOk() : sendError(409, "Probíhá jiná operace");
}

static void handleUploadDone() {
  if (!uploadAuthorized) {
    sendError(401, "unauthorized");
    return;
  }
  OtaStatus ota = otaStatus();
  if (ota.state == OTA_DONE) {
    sendOk();
  } else {
    sendError(500, ota.message);
  }
}

static void handleUploadChunk() {
  HTTPUpload &up = server.upload();
  static size_t expected;
  static int lastPercent;
  switch (up.status) {
    case UPLOAD_FILE_START:
      lastActivity = millis();
      uploadAuthorized = server.authenticate(WEB_USER, settings.password.c_str());
      expected = server.clientContentLength();
      lastPercent = -1;
      if (uploadAuthorized) {
        otaUploadBegin();
      }
      break;
    case UPLOAD_FILE_WRITE:
      if (uploadAuthorized && otaUploadWrite(up.buf, up.currentSize) && expected) {
        int percent = up.totalSize >= expected ? 100 : up.totalSize * 100 / expected;
        if (percent != lastPercent) {
          lastPercent = percent;
          displayMessage("Aktualizace", "Nahrávám firmware…", percent);
        }
      }
      break;
    case UPLOAD_FILE_END:
      if (uploadAuthorized) {
        otaUploadEnd(up.totalSize);
      }
      break;
    case UPLOAD_FILE_ABORTED:
      otaUploadAbort();
      break;
  }
}

static void handleGhost() {
  if (!authorized()) {
    return;
  }
  if (!appGhostAction(server.arg("action"))) {
    sendError(400, "Neznámá akce");
    return;
  }
  sendOk();
}

static void handlePet() {
  if (!authorized()) {
    return;
  }
  String message;
  String arg = server.hasArg("food") ? server.arg("food") : server.arg("name");
  bool ok = appPetCommand(server.arg("cmd"), arg, message);
  sendJson(ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":" + q(message) + "}");
}

static void handleScene() {
  if (!authorized()) {
    return;
  }
  appSetScene(server.arg("id").toInt());
  sendOk();
}

static void handleCalibrate() {
  if (!authorized()) {
    return;
  }
  appCalibrate() ? sendOk() : sendError(409, "Pohybový senzor nenalezen");
}

static void handleReboot() {
  if (authorized()) {
    sendOk();
    schedule(PENDING_RESTART);
  }
}

static void handleSleep() {
  if (authorized()) {
    sendOk();
    schedule(PENDING_SLEEP);
  }
}

static void handleWebManifest() {
  String body = "{\"name\":\"Můj duch\",\"short_name\":\"Můj duch\",\"start_url\":\"/\",\"display\":\"standalone\","
                "\"background_color\":\"#140b24\",\"theme_color\":\"#140b24\","
                "\"icons\":[{\"src\":\"/icon.png\",\"sizes\":\"180x180\",\"type\":\"image/png\"}]}";
  server.send(200, "application/manifest+json", body);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

static void setupRoutes() {
  server.on("/", HTTP_GET, [] {
    lastActivity = millis();
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/icon.png", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "image/png", (const char *)WEB_ICON_PNG, sizeof(WEB_ICON_PNG));
  });
  server.on("/manifest.webmanifest", HTTP_GET, handleWebManifest);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/soul", HTTP_POST, handleSoul);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/display", HTTP_POST, handleDisplay);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/password", HTTP_POST, handlePassword);
  server.on("/api/ota/check", HTTP_POST, handleOtaCheck);
  server.on("/api/ota/install", HTTP_POST, handleOtaInstall);
  server.on("/api/ota/upload", HTTP_POST, handleUploadDone, handleUploadChunk);
  server.on("/api/ghost", HTTP_POST, handleGhost);
  server.on("/api/pet", HTTP_POST, handlePet);
  server.on("/api/scene", HTTP_POST, handleScene);
  server.on("/api/imu/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/sleep", HTTP_POST, handleSleep);
  server.onNotFound(handleNotFound);
}

// ---------------------------------------------------------------- Wi-Fi

static void startServices() {
  MDNS.end();
  if (MDNS.begin(settings.hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }
  if (!serverStarted) {
    setupRoutes();
    server.begin();
    serverStarted = true;
  }
}

static int nextNetwork(int from) {
  for (int k = 0; k < MAX_NETWORKS; k++) {
    int i = (from + k) % MAX_NETWORKS;
    if (settings.networks[i].ssid.length()) {
      return i;
    }
  }
  return -1;
}

// Hotspot name = the pendant's name (UTF-8, at most 32 bytes, whole characters).
static String apSsid() {
  String name = settings.deviceName;
  name.trim();
  if (name.isEmpty()) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char buf[16];
    snprintf(buf, sizeof(buf), "Duch-%02X%02X", mac[4], mac[5]);
    return buf;
  }
  if (name.length() > 32) {
    int n = 32;
    while (n > 0 && ((uint8_t)name[n] & 0xC0) == 0x80) n--;
    name = name.substring(0, n);
  }
  return name;
}

static bool staWanted() {
  return !rescueMode && nextNetwork(0) >= 0;
}

static void tryNextNetwork() {
  networkIndex = nextNetwork(networkIndex + 1);
  if (networkIndex < 0) {
    return;
  }
  connectStart = millis();
  WiFi.begin(settings.networks[networkIndex].ssid.c_str(), settings.networks[networkIndex].pass.c_str());
  Serial.printf("Wi-Fi: connecting to %s\n", settings.networks[networkIndex].ssid.c_str());
}

void netInit() {
  WiFi.persistent(false);  // credentials live in our own settings
}

// The hotspot always runs while Wi-Fi is on, so the phone can always connect
// directly. Saved networks are joined in the background (internet for updates).
void netStart(bool rescue) {
  rescueMode = rescue;
  lastActivity = millis();
  dns.stop();
  WiFi.disconnect(true);
  WiFi.setHostname(settings.hostname.c_str());
  WiFi.mode(staWanted() ? WIFI_AP_STA : WIFI_AP);
  WiFi.setAutoReconnect(false);  // reconnects are paced by netLoop, scanning disturbs the hotspot
  String ssid = apSsid();
  WiFi.softAP(ssid.c_str(), settings.password.length() >= 8 ? settings.password.c_str() : nullptr);
  WiFi.setTxPower(WIFI_POWER_13dBm);  // plenty for a phone nearby, kinder to a small battery
  // Only our own name resolves; other lookups fail fast, so the phone does not
  // show a captive-portal popup and keeps using mobile data for the internet.
  dns.setErrorReplyCode(DNSReplyCode::NonExistentDomain);
  dns.start(53, settings.hostname, WiFi.softAPIP());
  mode = NET_AP;
  staConnected = false;
  networkIndex = -1;
  connectStart = 0;
  startServices();
  Serial.printf("Wi-Fi: hotspot \"%s\", http://%s/\n", ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

void netStop() {
  if (mode == NET_OFF) {
    return;
  }
  dns.stop();
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  mode = NET_OFF;
  staConnected = false;
  rescueMode = false;
  Serial.println("Wi-Fi: off");
}

bool netActive() {
  return mode != NET_OFF;
}

bool netOnline() {
  return mode != NET_OFF && staConnected;
}

void netLoop() {
  if (pending && (int32_t)(millis() - pendingAt) >= 0) {
    Pending what = pending;
    pending = PENDING_NONE;
    switch (what) {
      case PENDING_RECONNECT: netStart(false); break;
      case PENDING_SLEEP: appSleepNow(); break;
      case PENDING_RESTART: restartClean(); break;
      default: break;
    }
  }
  if (mode == NET_OFF) {
    return;
  }
  int clients = WiFi.softAPgetStationNum();
  if (clients > 0) {
    lastActivity = millis();  // a phone is connected to the hotspot: stay on
  }
  if (staWanted()) {
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !staConnected) {
      staConnected = true;
      configTime(0, 0, "pool.ntp.org", "time.google.com");  // pet ageing while switched off
      Serial.printf("Wi-Fi: connected to %s (%s)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else if (!connected) {
      staConnected = false;
      // Searching for a network hops channels and drops phones from the hotspot,
      // so only search while nobody is connected (or rarely).
      uint32_t every = clients > 0 ? 180000 : WIFI_CONNECT_TIMEOUT_MS;
      if (!connectStart || millis() - connectStart > every) {
        tryNextNetwork();
      }
    }
  }
  dns.processNextRequest();
  if (serverStarted) {
    server.handleClient();
  }
  if (wifiSecondsLeft() == 0 && !otaBusy()) {
    netStop();
  }
}
