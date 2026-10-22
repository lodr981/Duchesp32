#pragma once
#include <Arduino.h>

enum OtaState : uint8_t {
  OTA_IDLE,
  OTA_CHECKING,
  OTA_DOWNLOADING,
  OTA_UPLOADING,
  OTA_DONE,   // restart pending
  OTA_ERROR,
};

struct OtaStatus {
  OtaState state = OTA_IDLE;
  uint8_t progress = 0;
  String message;
  String latestVersion;  // from the last manifest check
  String notes;
  bool updateAvailable = false;
};

// Call first thing in setup(): software rollback after repeated crashes.
void otaBootCheck();
// Call from loop(): marks the firmware healthy, restarts after a finished update.
void otaLoop();

OtaStatus otaStatus();  // thread-safe copy
bool otaBusy();
const char *otaStateName(OtaState state);

// Background tasks (need Wi-Fi with internet access).
bool otaStartCheck();
bool otaStartInstall();

// Firmware upload from the web UI, driven by the web server.
bool otaUploadBegin();
bool otaUploadWrite(uint8_t *data, size_t len);
bool otaUploadEnd(size_t total);
void otaUploadAbort();
