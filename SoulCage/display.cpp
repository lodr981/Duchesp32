#include "display.h"
#include "config.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft;
static uint8_t *frameBuf;
static const size_t FRAME_BYTES = SCREEN_W * SCREEN_H * 2;
static bool powered = true;
static uint8_t brightness = 100;
static String lastTitle, lastLine;  // what displayMessage() shows now

// Frame buffer is big-endian RGB565 (wire order), so write colors byte by byte.
static void bufFillRect(int x, int y, int w, int h, uint16_t color) {
  for (int yy = y; yy < y + h; yy++) {
    uint8_t *p = frameBuf + (yy * SCREEN_W + x) * 2;
    for (int xx = 0; xx < w; xx++) {
      *p++ = color >> 8;
      *p++ = color & 0xFF;
    }
  }
}

static void drawBatteryLow() {
  const int x = 104, y = 200;  // bottom centre, inside the round area
  bufFillRect(x, y, 28, 14, TFT_WHITE);
  bufFillRect(x + 2, y + 2, 24, 10, TFT_BLACK);
  bufFillRect(x + 28, y + 4, 3, 6, TFT_WHITE);
  bufFillRect(x + 4, y + 4, 5, 6, TFT_RED);
}

static void drawWifiDot() {
  bufFillRect(116, 10, 8, 8, TFT_CYAN);
}

bool displayInit() {
  tft.begin();
  tft.setRotation(0);
  tft.setSwapBytes(false);  // frames are already in wire byte order
  tft.fillScreen(TFT_BLACK);

  // TFT_eSPI switches the backlight fully on, take it over with PWM.
  ledcAttach(PIN_BACKLIGHT, 5000, 8);
  displaySetBrightness(brightness);

  frameBuf = (uint8_t *)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!frameBuf) {
    frameBuf = (uint8_t *)malloc(FRAME_BYTES);
  }
  return frameBuf != nullptr;
}

void displaySetBrightness(uint8_t percent) {
  brightness = constrain(percent, 5, 100);
  if (powered) {
    ledcWrite(PIN_BACKLIGHT, brightness * 255 / 100);
  }
}

void displaySetPower(bool on) {
  if (on == powered) {
    return;
  }
  powered = on;
  if (on) {
    tft.writecommand(0x11);  // sleep out
    delay(120);
    tft.writecommand(0x29);  // display on
    ledcWrite(PIN_BACKLIGHT, brightness * 255 / 100);
  } else {
    ledcWrite(PIN_BACKLIGHT, 0);
    tft.writecommand(0x28);  // display off
    tft.writecommand(0x10);  // sleep in
  }
}

bool displayIsOn() {
  return powered;
}

bool displayFrame(const Animation &anim, uint16_t frame, uint8_t overlays) {
  if (!frameBuf || !powered) {
    return false;
  }
  if (!decodeFrame(anim, frame, frameBuf, FRAME_BYTES)) {
    return false;
  }
  if (overlays & OVERLAY_BATTERY_LOW) {
    drawBatteryLow();
  }
  if (overlays & OVERLAY_WIFI) {
    drawWifiDot();
  }
  tft.pushImage(0, 0, SCREEN_W, SCREEN_H, (uint16_t *)frameBuf);
  lastTitle = "";
  return true;
}

uint16_t *displayBuffer() {
  return (uint16_t *)frameBuf;
}

void displayPushNative() {
  if (!frameBuf || !powered) {
    return;
  }
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, SCREEN_W, SCREEN_H, (uint16_t *)frameBuf);
  tft.setSwapBytes(false);
  lastTitle = "";
}

void displayMessage(const char *title, const char *line, int progress) {
  if (!powered) {
    displaySetPower(true);
    lastTitle = "";
  }
  // Redraw text only when it changed, progress updates must not flicker.
  if (lastTitle != title || lastLine != (line ? line : "")) {
    lastTitle = title;
    lastLine = line ? line : "";
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, SCREEN_W / 2, 95, 4);
    if (line) {
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.drawString(line, SCREEN_W / 2, 130, 2);
    }
  }
  if (progress >= 0) {
    const int w = 150, x = (SCREEN_W - w) / 2, y = 155;
    tft.drawRect(x, y, w, 12, TFT_WHITE);
    tft.fillRect(x + 2, y + 2, (w - 4) * constrain(progress, 0, 100) / 100, 8, TFT_CYAN);
  }
}
