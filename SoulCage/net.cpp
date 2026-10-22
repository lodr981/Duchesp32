#include "net.h"
#include "animations.h"
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

enum NetMode : uint8_t { NET_OFF, NET_CONNECTING, NET_STA, NET_AP };
enum Pending : uint8_t { PENDING_NONE, PENDING_RECONNECT, PENDING_SLEEP, PENDING_RESTART };

static NetMode mode = NET_OFF;
static WebServer server(80);
static DNSServer dns;
static NetActions actions;
static bool serverStarted, rescueMode, uploadAuthorized;
static int networkIndex;
static uint32_t connectStart, lastActivity;
static Pending pending;
static uint32_t pendingAt;

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
  String ip = mode == NET_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
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
  body += ",\"mode\":" + q(mode == NET_AP ? "ap" : "sta");
  body += ",\"ssid\":" + q(mode == NET_AP ? WiFi.softAPSSID() : WiFi.SSID());
  body += ",\"ip\":" + q(ip);
  body += ",\"rssi\":" + String(mode == NET_STA ? WiFi.RSSI() : 0);
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
  body += "}";
  sendJson(200, body);
}

static void handleSoul() {
  if (!authorized()) {
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
  actions.selectSoul(index);
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
  if (server.hasArg("hostname")) {
    String host = sanitizeHostname(server.arg("hostname"));
    if (host.length()) {
      settings.hostname = host;
    }
  }
  if (server.hasArg("manifestUrl")) {
    String url = server.arg("manifestUrl");
    settings.manifestUrl = url.startsWith("https://") ? url : DEFAULT_MANIFEST_URL;
  }
  settingsSave();
  actions.applySettings();
  sendOk();
}

static void handleDisplay() {
  if (!authorized()) {
    return;
  }
  bool on = server.hasArg("on") ? server.arg("on") == "1" : !settings.displayOn;
  actions.setDisplay(on);
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
  if (mode == NET_AP) {
    schedule(PENDING_RECONNECT);  // hotspot uses the same password
  }
}

static void handleOtaCheck() {
  if (!authorized()) {
    return;
  }
  if (mode != NET_STA) {
    sendError(409, "Aktualizace z internetu vyžaduje připojení k Wi-Fi s internetem");
    return;
  }
  otaStartCheck() ? sendOk() : sendError(409, "Probíhá jiná operace");
}

static void handleOtaInstall() {
  if (!authorized()) {
    return;
  }
  if (mode != NET_STA) {
    sendError(409, "Aktualizace z internetu vyžaduje připojení k Wi-Fi s internetem");
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
          displayMessage("Aktualizace", "Nahravam firmware", percent);  // TFT fonts have no diacritics
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
  String body = "{\"name\":\"SoulCage\",\"short_name\":\"SoulCage\",\"start_url\":\"/\",\"display\":\"standalone\","
                "\"background_color\":\"#0b0b10\",\"theme_color\":\"#0b0b10\","
                "\"icons\":[{\"src\":\"/icon.png\",\"sizes\":\"180x180\",\"type\":\"image/png\"}]}";
  server.send(200, "application/manifest+json", body);
}

static void handleNotFound() {
  // Captive portal: any foreign host in hotspot mode lands on the UI.
  if (mode == NET_AP) {
    String host = server.hostHeader();
    if (host != WiFi.softAPIP().toString() && host != settings.hostname + ".local") {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "");
      return;
    }
  }
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
  for (int i = from; i < MAX_NETWORKS; i++) {
    if (settings.networks[i].ssid.length()) {
      return i;
    }
  }
  return -1;
}

static void startAp() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "SoulCage-%02X%02X", mac[4], mac[5]);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, settings.password.length() >= 8 ? settings.password.c_str() : nullptr);
  dns.start(53, "*", WiFi.softAPIP());
  mode = NET_AP;
  startServices();
  Serial.printf("Wi-Fi: hotspot %s, http://%s/\n", ssid, WiFi.softAPIP().toString().c_str());
}

static void startSta(int index) {
  networkIndex = index;
  connectStart = millis();
  mode = NET_CONNECTING;
  WiFi.setHostname(settings.hostname.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.networks[index].ssid.c_str(), settings.networks[index].pass.c_str());
  Serial.printf("Wi-Fi: connecting to %s\n", settings.networks[index].ssid.c_str());
}

void netInit(const NetActions &a) {
  actions = a;
  WiFi.persistent(false);  // credentials live in our own settings
}

void netStart(bool rescue) {
  rescueMode = rescue;
  lastActivity = millis();
  dns.stop();
  int first = rescue ? -1 : nextNetwork(0);
  if (first < 0) {
    startAp();
  } else {
    startSta(first);
  }
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
  rescueMode = false;
  Serial.println("Wi-Fi: off");
}

bool netActive() {
  return mode != NET_OFF;
}

void netLoop() {
  if (pending && (int32_t)(millis() - pendingAt) >= 0) {
    Pending what = pending;
    pending = PENDING_NONE;
    switch (what) {
      case PENDING_RECONNECT: netStart(false); break;
      case PENDING_SLEEP: actions.sleepNow(); break;
      case PENDING_RESTART: restartClean(); break;
      default: break;
    }
  }
  if (mode == NET_OFF) {
    return;
  }
  if (mode == NET_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      mode = NET_STA;
      startServices();
      Serial.printf("Wi-Fi: connected, http://%s.local/ (%s)\n", settings.hostname.c_str(),
                    WiFi.localIP().toString().c_str());
    } else if (millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
      int next = nextNetwork(networkIndex + 1);
      next >= 0 ? startSta(next) : startAp();
    }
  }
  if (mode == NET_AP) {
    dns.processNextRequest();
  }
  if (serverStarted) {
    server.handleClient();
  }
  if (wifiSecondsLeft() == 0 && !otaBusy()) {
    netStop();
  }
}
