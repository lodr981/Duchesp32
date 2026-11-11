#include "hunt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Color SPARK_COLORS[] = {{255, 122, 200}, {183, 155, 255}, {126, 232, 196}, {255, 220, 120}, {255, 255, 255}};

int huntFoundCount(const HuntData &d) {
  int n = 0;
  for (int i = 0; i < d.count && i < HUNT_MAX_SEALS; i++) n += (d.found >> i) & 1;
  return n;
}

int huntSealForCode(const HuntData &d, const char *code) {
  for (int i = 0; i < d.count && i < HUNT_MAX_SEALS; i++) {
    if (strncmp(d.codes[i], code, HUNT_CODE_LEN) == 0 && strlen(code) == HUNT_CODE_LEN) return i;
  }
  return -1;
}

float HuntOverlay::random01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng & 0xFFFFFF) / 16777216.0f;
}

void HuntOverlay::burst(float x, float y, int n, float speed) {
  for (int i = 0; i < n; i++) {
    if (sparkCount == (int)(sizeof(sparks) / sizeof(sparks[0]))) {
      memmove(sparks, sparks + 1, sizeof(Spark) * (sparkCount - 1));
      sparkCount--;
    }
    float a = random01() * 6.2832f, s = speed * (0.35f + 0.65f * random01());
    sparks[sparkCount++] = {x, y, cosf(a) * s, sinf(a) * s, 1.2f + random01() * 0.8f, SPARK_COLORS[(int)(random01() * 5) % 5]};
  }
}

static void sealPos(int i, int count, float &x, float &y) {
  float a = -1.5708f + i * 6.2832f / count;
  x = 120 + 104 * cosf(a);
  y = 120 + 104 * sinf(a);
}

void HuntOverlay::sealBroken(int index) {
  flashSeal = index;
  flashTime = 0;
}

void HuntOverlay::freed() {
  freedTime = 0;
  sparkCount = 0;
}

void HuntOverlay::update(float dt) {
  time += dt;
  flashTime += dt;
  int alive = 0;
  for (int i = 0; i < sparkCount; i++) {
    Spark s = sparks[i];
    s.life -= dt;
    if (s.life <= 0) continue;
    s.vy += 120 * dt;
    s.vx *= 1 - dt;
    s.x += s.vx * dt;
    s.y += s.vy * dt;
    sparks[alive++] = s;
  }
  sparkCount = alive;
  if (freedTime >= 0) {
    float before = freedTime;
    freedTime += dt;
    // a firework every 0.5 s
    if ((int)(freedTime / 0.5f) != (int)(before / 0.5f)) {
      burst(50 + random01() * 140, 40 + random01() * 100, 26, 120);
    }
    if (freedTime > 11) freedTime = -1;
  }
}

static void drawLock(Canvas &c, float x, float y, bool open, Color col, uint8_t a) {
  // shackle
  if (open) {
    c.arc(x + 3, y - 4, 4.5f, 5, 180, 360, 2, col, a);
  } else {
    c.arc(x, y - 3, 4.5f, 5, 180, 360, 2, col, a);
  }
  c.roundRect(x - 6, y - 2, 12, 9, 2, col, a);
}

void HuntOverlay::render(Canvas &c, const HuntData &d, const char *thanks) {
  if (d.count && (d.active || flashTime < 3 || celebrating())) {
    for (int i = 0; i < d.count; i++) {
      float x, y;
      sealPos(i, d.count, x, y);
      bool broken = (d.found >> i) & 1;
      bool flashing = i == flashSeal && flashTime < 2.5f;
      if (broken) {
        float pulse = flashing ? 1 + 0.6f * sinf(flashTime * 12) * (1 - flashTime / 2.5f) : 1;
        c.glow(x, y, 16 * pulse, 16 * pulse, {255, 210, 120}, 150);
        c.circle(x, y, 10, {60, 30, 80}, 200);
        drawLock(c, x, y, true, {255, 220, 140}, 255);
      } else {
        c.circle(x, y, 10, {20, 10, 30}, 210);
        drawLock(c, x, y, false, {150, 120, 190}, 230);
      }
    }
  }
  if (flashTime < 3 && flashSeal >= 0 && !celebrating()) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Pečeť %d/%d!", huntFoundCount(d), d.count);
    float a = flashTime < 0.3f ? flashTime / 0.3f : flashTime > 2.5f ? (3 - flashTime) / 0.5f : 1;
    c.glow(120, 185, 90, 22, {30, 10, 50}, (uint8_t)(170 * a));
    c.text(buf, 120, 170, FONT_BIG, {255, 230, 150}, (uint8_t)(255 * a));
  }
  if (celebrating()) {
    float t = freedTime;
    for (int i = 0; i < sparkCount; i++) {
      c.circle(sparks[i].x, sparks[i].y, 2.2f, sparks[i].c, (uint8_t)(255 * fminf(1, sparks[i].life)));
    }
    float a = t < 1 ? t : t > 10 ? 11 - t : 1;
    if (a > 0) {
      c.glow(120, 150, 115, 45, {30, 10, 50}, (uint8_t)(160 * a));
      c.text("Duše je volná!", 120, 124, FONT_BIG, {255, 230, 150}, (uint8_t)(255 * a));
      if (t > 1.5f && thanks && *thanks) {
        c.text(thanks, 120, 162, FONT_SMALL, {255, 190, 230}, (uint8_t)(255 * a * fminf(1, (t - 1.5f) * 2)));
      }
    }
  }
}
