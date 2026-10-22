#pragma once
#include "animations.h"

enum Overlay : uint8_t {
  OVERLAY_NONE = 0,
  OVERLAY_BATTERY_LOW = 1,
  OVERLAY_WIFI = 2,
};

bool displayInit();
void displaySetBrightness(uint8_t percent);
void displaySetPower(bool on);
bool displayIsOn();

// Decodes and shows one frame with optional status overlays.
bool displayFrame(const Animation &anim, uint16_t frame, uint8_t overlays);

// Native RGB565 frame buffer for procedural drawing (ghost mode) and its push.
uint16_t *displayBuffer();
void displayPushNative();

// Full-screen text message, optional progress bar (0..100, <0 = none).
void displayMessage(const char *title, const char *line = nullptr, int progress = -1);
