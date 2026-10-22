#pragma once

// Firmware version. Release tags must match (v1.0.0 <-> "1.0.0"), CI checks it.
#define FW_VERSION "1.0.0"

// Default OTA manifest (GitHub Releases of this repository).
#define DEFAULT_MANIFEST_URL "https://github.com/lodr981/Duchesp32/releases/latest/download/manifest.json"

// Waveshare ESP32-S3-LCD-1.28 (GC9A01 240x240). Display pins live in the TFT_eSPI setup.
#define PIN_BACKLIGHT 40
#define PIN_BAT_ADC 1      // battery through 200k/100k divider
#define BAT_DIVIDER 3.0f
#define PIN_BOOT_BUTTON 0  // optional: BOOT button, if reachable in your case

#define SCREEN_W 240
#define SCREEN_H 240

// Power-cycle gesture: boots started within this window after the previous
// boot count as "quick". 2 quick boots = next soul, 3 = Wi-Fi rescue hotspot.
#define QUICK_BOOT_WINDOW_MS 20000

// The running firmware is considered healthy after this time (OTA rollback).
#define HEALTHY_AFTER_MS 30000

// Battery thresholds (mV). Below BAT_VALID_MV no battery is assumed (USB only).
#define BAT_VALID_MV 2500
#define BAT_LOW_MV 3500
#define BAT_CUTOFF_MV 3300

#define WIFI_RESCUE_MINUTES 10
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define MAX_NETWORKS 3

#define DEFAULT_HOSTNAME "soulcage"
#define DEFAULT_PASSWORD "soulcage"  // AP + web login, min. 8 chars (WPA2)
#define WEB_USER "admin"
