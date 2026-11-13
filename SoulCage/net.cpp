#include "net.h"
#include "radarnet.h"
#include "animations.h"
#include "app.h"
#include "config.h"
#include "display.h"
#include "ota.h"
#include "power.h"
#include "settings.h"
#include "web_icon.h"
#include "web_ui.h"
#include "hunt.h"
#include "qrcodegen.h"
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
static uint32_t uiServedAt;  // when the app was last used

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
  uiServedAt = millis();  // the app is in use
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
  body += ",\"qrOnBoot\":" + String(settings.qrOnBoot ? "true" : "false");
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
  settings.qrOnBoot = argLong("qrOnBoot", settings.qrOnBoot, 0, 1);
  settings.soulsOnly = argLong("soulsOnly", settings.soulsOnly, 0, 1);
  if (server.hasArg("homeName")) settings.homeName = server.arg("homeName").substring(0, 30);
  if (server.hasArg("homeLat") && server.hasArg("homeLon")) {
    float lat = server.arg("homeLat").toFloat(), lon = server.arg("homeLon").toFloat();
    if (lat > 35 && lat < 70 && lon > -25 && lon < 45) {  // Europe
      settings.homeLat = lat;
      settings.homeLon = lon;
    }
  }
  long range = argLong("radarRange", settings.radarRange, 10, 150);
  settings.radarRange = range <= 35 ? 25 : range <= 75 ? 50 : 100;
  settings.radarInShow = argLong("radarInShow", settings.radarInShow, 0, 1);
  settings.rainAlert = argLong("rainAlert", settings.rainAlert, 0, 1);
  settings.weatherFx = argLong("weatherFx", settings.weatherFx, 0, 1);
  settings.forecastAlert = argLong("forecastAlert", settings.forecastAlert, 0, 1);
  settings.planeAlert = argLong("planeAlert", settings.planeAlert, 0, 1);
  settings.issAlert = argLong("issAlert", settings.issAlert, 0, 1);
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

static void handleGame() {
  if (!authorized()) {
    return;
  }
  String message;
  bool ok = appGameCommand(server.arg("cmd"), server.arg("level"), message);
  sendJson(ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":" + q(message) + "}");
}

static void handleHunt() {
  if (!authorized()) {
    return;
  }
  String cmd = server.arg("cmd");
  if (cmd == "hints") {
    for (int i = 0; i <= HUNT_MAX_SEALS; i++) {
      String key = "h" + String(i);
      if (server.hasArg(key)) huntSetHint(i, server.arg(key));
    }
    sendJson(200, "{\"ok\":true,\"message\":\"Nápovědy uloženy\"}");
    return;
  }
  String message;
  bool ok = appHuntCommand(cmd, server.arg("count"), message);
  sendJson(ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":" + q(message) + "}");
}

// Scanned seal (opened by the phone camera, so no login).
static void handleScan() {
  lastActivity = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", appHuntScan(server.arg("c")));
}

static String qrSvg(const String &text) {
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)], temp[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
  if (!qrcodegen_encodeText(text.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM, 1, 8, qrcodegen_Mask_AUTO, true)) {
    return "";
  }
  int size = qrcodegen_getSize(qr);
  String svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='-2 -2 " + String(size + 4) + " " + String(size + 4) +
               "' shape-rendering='crispEdges'><rect x='-2' y='-2' width='" + String(size + 4) + "' height='" +
               String(size + 4) + "' fill='#fff'/><path fill='#140b24' d='";
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (qrcodegen_getModule(qr, x, y)) svg += "M" + String(x) + " " + String(y) + "h1v1h-1z";
    }
  }
  return svg + "'/></svg>";
}

// Printable seals: open in Safari, Share -> Print (or save as PDF).
static void handlePrint() {
  HuntData d;
  if (!huntLoad(d) || !d.count) {
    server.send(200, "text/html; charset=utf-8", "<p>Nejdřív v aplikaci založ novou hru.</p>");
    return;
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent(F("<!doctype html><html lang=cs><head><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                       "<title>Pečetě – Osvoboď duši</title><style>"
                       "body{font-family:-apple-system,sans-serif;margin:0;padding:12px;color:#140b24}"
                       ".grid{display:grid;grid-template-columns:1fr 1fr;gap:10mm}"
                       ".card{border:2px dashed #7a5aa8;border-radius:8mm;padding:6mm;text-align:center;break-inside:avoid}"
                       ".card h2{margin:0 0 2mm;font-size:20pt}.card svg{width:55mm;height:55mm}"
                       ".card p{margin:2mm 0 0;font-size:11pt;color:#555}.tip{font-size:12pt;margin:0 0 6mm}"
                       "@media print{.tip{display:none}}</style></head><body>"
                       "<p class=tip>Vytiskni (Sdílet → Tisk, nebo ulož jako PDF), rozstříhej a schovej po bytě. "
                       "Kdo pečeť najde, naskenuje ji foťákem iPhonu připojeného k Wi-Fi přívěsku.</p><div class=grid>"));
  for (int i = 0; i < d.count; i++) {
    String url = "http://192.168.4.1/s?c=" + String(d.codes[i]);
    server.sendContent("<div class=card><h2>🔒 Pečeť " + String(i + 1) + "</h2>" + qrSvg(url) +
                       "<p>Naskenuj mě a osvoboď duši 👻</p></div>");
  }
  server.sendContent(F("</div></body></html>"));
  server.sendContent("");
}

static void handleRadar() {
  if (!authorized()) {
    return;
  }
  appRadar(server.arg("show")) ? sendOk() : sendError(400, "Neznámý radar");
}

static void handlePlanes() {
  if (!authorized()) {
    return;
  }
  radarNetDemand(false, true);  // the list is open in the app: keep aircraft fresh
  String body;
  radarPlanesJson(body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
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
    uiServedAt = millis();
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
  server.on("/api/game", HTTP_POST, handleGame);
  server.on("/api/hunt", HTTP_POST, handleHunt);
  server.on("/api/radar", HTTP_POST, handleRadar);
  server.on("/api/planes", HTTP_GET, handlePlanes);
  server.on("/s", HTTP_GET, handleScan);
  server.on("/hunt/print", HTTP_GET, handlePrint);
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

int netClients() {
  return mode == NET_OFF ? 0 : WiFi.softAPgetStationNum();
}

uint32_t netUiServedAt() {
  return uiServedAt;
}

String netHotspotName() {
  return apSsid();
}

String netDnsServer() {
  return staConnected ? WiFi.dnsIP(0).toString() : String("-");
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
  if (staConnected && (settings.rainAlert || settings.weatherFx || settings.forecastAlert || settings.planeAlert ||
                       settings.issAlert)) {
    lastActivity = millis();  // radar features need the internet: stay on while the home Wi-Fi is there
  }
  if (staWanted()) {
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !staConnected) {
      staConnected = true;
      configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");  // pet ageing, local time for the radar
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
