#include "greeting.h"
#include <math.h>
#include <string.h>
#include "qrcodegen.h"

static float clamp01(float v) {
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

// Bounce in from above (0..1 -> offset factor 1..0 with a little overshoot).
static float bounce(float x) {
  x = clamp01(x);
  return 1 - (1 - powf(1 - x, 3)) * (1 + 0.25f * sinf(x * 3.14159f));
}

static void shadowText(Canvas &c, const char *s, float x, float y, FontId f, Color col, uint8_t a) {
  c.text(s, x + 2, y + 2, f, {20, 8, 40}, (uint8_t)(a * 0.7f));
  c.text(s, x, y, f, col, a);
}

void drawGreeting(Canvas &c, Ghost &ghost, const GhostLook &look, const char *text, const char *subtitle, float t) {
  ghost.render(c, look);
  drawGreetingText(c, text, subtitle, t);
}

void drawGreetingText(Canvas &c, const char *text, const char *subtitle, float t) {
  FontId font = Canvas::textWidth(text, FONT_BIG) <= 190 ? FONT_BIG : FONT_SMALL;
  float a = clamp01((t - 0.3f) / 0.4f) * clamp01((GREETING_SECONDS - t) / 0.5f);
  if (a <= 0) return;
  float y = 146 - 60 * bounce((t - 0.3f) / 0.9f);
  // Soft dark plate so the text reads over the ghost's tail.
  c.glow(120, y + 18, 120, 26, {30, 10, 50}, (uint8_t)(150 * a));
  Color pink = {255, 150, 210};
  shadowText(c, text, 120, y, font, {255, 255, 255}, (uint8_t)(255 * a));
  if (subtitle && *subtitle) {
    float sa = clamp01((t - 1.2f) / 0.4f) * a;
    shadowText(c, subtitle, 120, y + Canvas::lineHeight(font) + 2, FONT_SMALL, pink, (uint8_t)(255 * sa));
  }
  // Sparkles around the text
  for (int i = 0; i < 5; i++) {
    float tw = fabsf(sinf(t * 5 + i * 1.3f));
    float sx = 120 + (i - 2) * 46 + 6 * sinf(t * 2 + i), sy = y - 8 + (i % 2) * 44;
    c.star(sx, sy, 3 + 4 * tw, {255, 236, 150}, (uint8_t)(220 * tw * a));
  }
}

void drawMessage(Canvas &c, const char *title, const char *line, int progress) {
  c.gradient({28, 14, 48}, {8, 6, 18});
  c.glow(120, 120, 120, 100, {120, 70, 200}, 50);
  c.text(title, 120, 82, FONT_BIG, {255, 255, 255});
  if (line && *line) {
    c.text(line, 120, 122, FONT_SMALL, {220, 200, 255});
  }
  if (progress >= 0) {
    float p = progress > 100 ? 1 : progress / 100.0f;
    c.roundRect(45, 154, 150, 12, 6, {60, 45, 90});
    if (p > 0) c.roundRect(45, 154, 150 * p < 12 ? 12 : 150 * p, 12, 6, {255, 140, 210});
  }
}

void wifiQrPayload(char *out, int size, const char *ssid, const char *password) {
  int n = 0;
  auto put = [&](char ch) {
    if (n < size - 1) out[n++] = ch;
  };
  auto putEscaped = [&](const char *s) {
    for (; *s; s++) {
      if (strchr("\\;,:\"", *s)) put('\\');
      put(*s);
    }
  };
  for (const char *p = password && *password ? "WIFI:T:WPA;S:" : "WIFI:T:nopass;S:"; *p; p++) put(*p);
  putEscaped(ssid);
  if (password && *password) {
    for (const char *p = ";P:"; *p; p++) put(*p);
    putEscaped(password);
  }
  put(';');
  put(';');
  out[n] = 0;
}

void drawQr(Canvas &c, const char *payload, const char *title, const char *line, float t) {
  // The code is generated once per payload.
  static char cached[160];
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  static bool ok;
  if (strncmp(cached, payload, sizeof(cached)) != 0) {
    strncpy(cached, payload, sizeof(cached) - 1);
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    ok = qrcodegen_encodeText(payload, temp, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO, true);
  }
  c.gradient({58, 26, 96}, {20, 10, 36});
  c.text(title, 120, 18, FONT_SMALL, {255, 255, 255});
  if (!ok) {
    c.text(line, 120, 110, FONT_SMALL, {220, 200, 255});
    return;
  }
  int size = qrcodegen_getSize(qr);
  int module = 150 / (size + 2);  // with a one-module quiet zone on each side
  if (module < 2) module = 2;
  int box = module * (size + 2);
  int x0 = 120 - box / 2, y0 = 124 - box / 2;
  float pulse = 0.5f + 0.5f * sinf(t * 3);
  c.glow(120, 124, box * 0.75f, box * 0.75f, {255, 122, 200}, (uint8_t)(70 + 50 * pulse));
  c.roundRect(x0 - 4, y0 - 4, box + 8, box + 8, 10, {255, 255, 255});
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (qrcodegen_getModule(qr, x, y)) {
        c.rect(x0 + (x + 1) * module, y0 + (y + 1) * module, module, module, {20, 10, 36});
      }
    }
  }
  c.text(line, 120, y0 + box + 10, FONT_SMALL, {255, 190, 230});
}
