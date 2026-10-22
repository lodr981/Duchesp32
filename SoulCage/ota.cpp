#include "ota.h"
#include "certs.h"
#include "config.h"
#include "power.h"
#include "settings.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <freertos/semphr.h>

static OtaStatus status;
static String manifestUrl, firmwareUrl, firmwareMd5;
static uint32_t restartAt;
static bool healthy;
// Status strings are written by the OTA task and read by the web server.
static SemaphoreHandle_t statusLock = xSemaphoreCreateMutex();

static void setStatus(OtaState state, const String &message) {
  xSemaphoreTake(statusLock, portMAX_DELAY);
  status.state = state;
  status.message = message;
  xSemaphoreGive(statusLock);
}

#ifdef CONFIG_APP_ROLLBACK_ENABLE
// Keep a freshly updated image in "pending verify" until it ran for a while (see otaLoop).
extern "C" bool verifyRollbackLater() {
  return true;
}
#endif

static bool isCrashReset(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

void otaBootCheck() {
  // Fallback for bootloaders without rollback support: after an update, the
  // new image gets a few crash resets before we boot the previous slot again.
  uint8_t trial = persistGetU8("ota_trial", 0);
  if (!trial) {
    return;
  }
  if (isCrashReset(esp_reset_reason())) {
    trial++;
  }
  if (trial > 3) {
    persistSetU8("ota_trial", 0);
    const esp_partition_t *previous = esp_ota_get_next_update_partition(nullptr);
    if (previous && esp_ota_set_boot_partition(previous) == ESP_OK) {
      Serial.println("OTA: new firmware keeps crashing, rolling back");
      esp_restart();
    }
    return;
  }
  persistSetU8("ota_trial", trial);
}

static void markHealthy() {
  healthy = true;
#ifdef CONFIG_APP_ROLLBACK_ENABLE
  esp_ota_mark_app_valid_cancel_rollback();
#endif
  if (persistGetU8("ota_trial", 0)) {
    persistSetU8("ota_trial", 0);
  }
}

static void finishUpdate() {
  persistSetU8("ota_trial", 1);
  status.progress = 100;
  setStatus(OTA_DONE, "Hotovo, restartuji…");
  restartAt = millis() + 1500;
}

void otaLoop() {
  if (!healthy && millis() > HEALTHY_AFTER_MS) {
    markHealthy();
  }
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) {
    restartClean();
  }
}

OtaStatus otaStatus() {
  xSemaphoreTake(statusLock, portMAX_DELAY);
  OtaStatus copy = status;
  xSemaphoreGive(statusLock);
  return copy;
}

bool otaBusy() {
  return status.state == OTA_CHECKING || status.state == OTA_DOWNLOADING || status.state == OTA_UPLOADING ||
         status.state == OTA_DONE;
}

const char *otaStateName(OtaState state) {
  switch (state) {
    case OTA_CHECKING: return "checking";
    case OTA_DOWNLOADING: return "downloading";
    case OTA_UPLOADING: return "uploading";
    case OTA_DONE: return "done";
    case OTA_ERROR: return "error";
    default: return "idle";
  }
}

static void fail(const String &message) {
  Serial.println("OTA: " + message);
  setStatus(OTA_ERROR, message);
}

// Our own manifest is flat JSON, a tiny extractor is enough.
static String jsonField(const String &json, const char *key) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) {
    return "";
  }
  int colon = json.indexOf(':', k + needle.length());
  int start = json.indexOf('"', colon + 1);
  if (colon < 0 || start < 0) {
    return "";
  }
  String out;
  for (int i = start + 1; i < (int)json.length(); i++) {
    char c = json[i];
    if (c == '\\' && i + 1 < (int)json.length()) {
      char n = json[++i];
      out += n == 'n' ? '\n' : n;
    } else if (c == '"') {
      break;
    } else {
      out += c;
    }
  }
  return out;
}

// Returns <0, 0, >0 like strcmp for "major.minor.patch".
static int compareVersions(const String &a, const String &b) {
  int pa[3] = {0}, pb[3] = {0};
  sscanf(a.c_str(), "%d.%d.%d", &pa[0], &pa[1], &pa[2]);
  sscanf(b.c_str(), "%d.%d.%d", &pb[0], &pb[1], &pb[2]);
  for (int i = 0; i < 3; i++) {
    if (pa[i] != pb[i]) {
      return pa[i] < pb[i] ? -1 : 1;
    }
  }
  return 0;
}

static void prepareClient(NetworkClientSecure &client, HTTPClient &http) {
  client.setCACert(OTA_ROOT_CAS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.useHTTP10(true);  // no chunked encoding, plain stream for Update
}

static bool fetchManifest() {
  NetworkClientSecure client;
  HTTPClient http;
  prepareClient(client, http);
  if (!http.begin(client, manifestUrl)) {
    fail("Neplatná adresa manifestu");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    fail("Manifest: HTTP " + String(code) + " " + http.errorToString(code));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  String version = jsonField(body, "version");
  firmwareUrl = jsonField(body, "url");
  firmwareMd5 = jsonField(body, "md5");
  if (version.isEmpty() || firmwareUrl.isEmpty()) {
    fail("Manifest neobsahuje version/url");
    return false;
  }
  xSemaphoreTake(statusLock, portMAX_DELAY);
  status.latestVersion = version;
  status.notes = jsonField(body, "notes");
  status.updateAvailable = compareVersions(version, FW_VERSION) > 0;
  xSemaphoreGive(statusLock);
  return true;
}

static void checkTask(void *) {
  if (fetchManifest()) {
    OtaStatus s = otaStatus();
    setStatus(OTA_IDLE, s.updateAvailable ? "K dispozici je verze " + s.latestVersion : "Máš nejnovější verzi");
  }
  vTaskDelete(nullptr);
}

static void installTask(void *) {
  if (!fetchManifest()) {
    vTaskDelete(nullptr);
    return;
  }
  setStatus(OTA_DOWNLOADING, "Stahuji " + otaStatus().latestVersion);

  NetworkClientSecure client;
  HTTPClient http;
  prepareClient(client, http);
  http.begin(client, firmwareUrl);
  int code = http.GET();
  int size = http.getSize();
  if (code != HTTP_CODE_OK || size <= 0) {
    fail("Firmware: HTTP " + String(code));
    http.end();
    vTaskDelete(nullptr);
    return;
  }
  if (!Update.begin(size)) {
    fail(String("Nelze začít: ") + Update.errorString());
    http.end();
    vTaskDelete(nullptr);
    return;
  }
  if (firmwareMd5.length() == 32) {
    Update.setMD5(firmwareMd5.c_str());
  }
  Update.onProgress([](size_t done, size_t total) {
    status.progress = total ? done * 100 / total : 0;
  });
  size_t written = Update.writeStream(*http.getStreamPtr());
  http.end();
  if (written != (size_t)size || !Update.end()) {
    fail(String("Chyba zápisu: ") + Update.errorString());
    Update.abort();
  } else {
    finishUpdate();
  }
  vTaskDelete(nullptr);
}

static bool startTask(TaskFunction_t fn, const char *name, OtaState state, const char *message) {
  if (otaBusy()) {
    return false;
  }
  manifestUrl = settings.manifestUrl;
  status.progress = 0;
  setStatus(state, message);
  // TLS needs a generous stack; run on core 0 next to Wi-Fi, animation stays on core 1.
  if (xTaskCreatePinnedToCore(fn, name, 16384, nullptr, 1, nullptr, 0) != pdPASS) {
    fail("Nelze spustit úlohu");
    return false;
  }
  return true;
}

bool otaStartCheck() {
  return startTask(checkTask, "ota_check", OTA_CHECKING, "Kontroluji aktualizace…");
}

bool otaStartInstall() {
  return startTask(installTask, "ota_install", OTA_CHECKING, "Připravuji aktualizaci…");
}

bool otaUploadBegin() {
  if (otaBusy()) {
    return false;
  }
  status.progress = 0;
  setStatus(OTA_UPLOADING, "Nahrávám firmware…");
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    fail(String("Nelze začít: ") + Update.errorString());
    return false;
  }
  return true;
}

bool otaUploadWrite(uint8_t *data, size_t len) {
  if (status.state != OTA_UPLOADING) {
    return false;
  }
  if (Update.write(data, len) != len) {
    fail(String("Chyba zápisu: ") + Update.errorString());
    Update.abort();
    return false;
  }
  return true;
}

bool otaUploadEnd(size_t total) {
  if (status.state != OTA_UPLOADING) {
    return false;
  }
  if (!Update.end(true)) {
    fail(String("Neplatný firmware: ") + Update.errorString());
    return false;
  }
  Serial.printf("OTA: uploaded %u bytes\n", (unsigned)total);
  finishUpdate();
  return true;
}

void otaUploadAbort() {
  if (status.state == OTA_UPLOADING) {
    Update.abort();
    fail("Nahrávání přerušeno");
  }
}
