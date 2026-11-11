#include "greeting.h"
#include <math.h>

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
