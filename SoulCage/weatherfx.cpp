#include "weatherfx.h"
#include <math.h>

WeatherFxKind weatherFxKind(int code, int radarLevel) {
  if (code >= 95 || radarLevel >= 4) return FX_STORM;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return FX_SNOW;
  if (code == 65 || code == 67 || code == 82 || radarLevel == 3) return FX_HEAVY_RAIN;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 81) || radarLevel >= 1) return FX_RAIN;
  if (code == 45 || code == 48) return FX_FOG;
  return FX_NONE;
}

const char *weatherFxName(WeatherFxKind k) {
  switch (k) {
    case FX_RAIN: return "déšť";
    case FX_HEAVY_RAIN: return "liják";
    case FX_SNOW: return "sníh";
    case FX_STORM: return "bouřka";
    case FX_FOG: return "mlha";
    default: return "";
  }
}

float WeatherFx::rand01() {
  rnd = rnd * 1664525u + 1013904223u;
  return (rnd >> 8) / 16777216.0f;
}

void WeatherFx::respawn(Drop &d, bool top) {
  d.x = rand01() * 260 - 10;
  d.y = top ? -10 - rand01() * 40 : rand01() * 240;
  d.phase = rand01() * 6.28f;
  if (kind_ == FX_SNOW) {
    d.v = 18 + rand01() * 22;
    d.len = 1.6f + rand01() * 1.8f;  // flake radius
  } else {
    d.v = 260 + rand01() * 160;
    d.len = 8 + rand01() * 10;
  }
}

void WeatherFx::set(WeatherFxKind kind, bool frost) {
  frost_ = frost;
  if (kind == kind_) return;
  kind_ = kind;
  dropCount = kind == FX_RAIN ? 22 : kind == FX_HEAVY_RAIN || kind == FX_STORM ? MAX_DROPS : kind == FX_SNOW ? 34 : 0;
  for (int i = 0; i < dropCount; i++) respawn(drops[i], false);
  for (Bead &b : beads) {
    b.x = 40 + rand01() * 160;
    b.y = 30 + rand01() * 150;
    b.r = 2.5f + rand01() * 3;
    b.v = 0;
    b.wait = rand01() * 6;
  }
  strength = 0;
  nextFlash = 1.2f;
}

void WeatherFx::update(float dt) {
  clock += dt;
  float target = kind_ == FX_NONE && !frost_ ? 0 : 1;
  strength += (target - strength) * (dt * 0.8f > 1 ? 1 : dt * 0.8f);
  float wind = kind_ == FX_STORM ? 70 : kind_ == FX_HEAVY_RAIN ? 40 : 18;
  for (int i = 0; i < dropCount; i++) {
    Drop &d = drops[i];
    if (kind_ == FX_SNOW) {
      d.y += d.v * dt;
      d.x += sinf(clock * 1.3f + d.phase) * 12 * dt;
    } else {
      d.y += d.v * dt;
      d.x += wind * dt;
    }
    if (d.y > 250 || d.x > 260) respawn(d, true);
  }
  bool wet = kind_ == FX_RAIN || kind_ == FX_HEAVY_RAIN || kind_ == FX_STORM;
  if (wet) {
    for (Bead &b : beads) {
      if (b.wait > 0) {
        b.wait -= dt;
        continue;
      }
      b.v += 30 * dt;
      if (b.v > 45) b.v = 45;
      b.y += b.v * dt;
      if (b.y > 230) {
        b.x = 40 + rand01() * 160;
        b.y = 20 + rand01() * 80;
        b.v = 0;
        b.wait = 1 + rand01() * 5;
      }
    }
  }
  if (kind_ == FX_STORM) {
    nextFlash -= dt;
    if (nextFlash <= 0) {
      flash = 1;
      boltX = 50 + rand01() * 140;
      nextFlash = 3 + rand01() * 7;
    }
  }
  flash -= dt * 2.5f;
  if (flash < 0) flash = 0;
}

void WeatherFx::render(Canvas &c) {
  if (strength < 0.02f) return;
  float s = strength;
  if (kind_ == FX_FOG) {
    for (int i = 0; i < 3; i++) {
      float x = fmodf(clock * (8 + i * 4) + i * 90, 360) - 60;
      c.glow(x, 70 + i * 55, 120, 45, {200, 205, 215}, (uint8_t)(70 * s));
    }
  }
  for (int i = 0; i < dropCount; i++) {
    const Drop &d = drops[i];
    if (kind_ == FX_SNOW) {
      c.circle(d.x, d.y, d.len, {245, 248, 255}, (uint8_t)(220 * s));
    } else {
      float k = kind_ == FX_RAIN ? 0.06f : kind_ == FX_HEAVY_RAIN ? 0.15f : 0.25f;  // slant
      c.line(d.x, d.y, d.x - d.len * k, d.y - d.len, 1.3f, {170, 200, 255}, (uint8_t)(150 * s));
    }
  }
  if (kind_ == FX_RAIN || kind_ == FX_HEAVY_RAIN || kind_ == FX_STORM) {
    for (const Bead &b : beads) {
      if (b.v > 1) c.line(b.x, b.y - b.r, b.x, b.y - b.r - 14, b.r * 0.6f, {190, 215, 255}, (uint8_t)(50 * s));
      c.circle(b.x, b.y, b.r, {200, 220, 255}, (uint8_t)(110 * s));
      c.circle(b.x - b.r * 0.35f, b.y - b.r * 0.35f, b.r * 0.35f, {255, 255, 255}, (uint8_t)(170 * s));
    }
  }
  if (flash > 0) {
    c.overlay({235, 235, 255}, (uint8_t)(110 * flash * s));
    if (flash > 0.55f) {
      float x = boltX, y = 0;
      float pts[16];
      uint32_t r = (uint32_t)(boltX * 977);
      for (int i = 0; i < 8; i++) {
        pts[i * 2] = x;
        pts[i * 2 + 1] = y;
        r = r * 1664525u + 1013904223u;
        x += ((r >> 9) % 30) - 15.0f;
        y += 18;
      }
      c.polyline(pts, 8, 3, {255, 250, 200}, (uint8_t)(255 * s));
    }
  }
  if (frost_) {
    // ice on the rim of the glass: crystals and a white haze
    c.ring(120, 120, 116, 10, {225, 240, 255}, (uint8_t)(35 * s));
    for (int i = 0; i < 28; i++) {
      float an = i * 12.857f * 0.0174533f + sinf(i * 3.1f) * 0.1f;
      float r = 104 + (i % 3) * 4, len = 5 + (i * 7 % 5);
      float x = 120 + cosf(an) * r, y = 120 + sinf(an) * r;
      for (int k = 0; k < 3; k++) {
        float a2 = an + k * 1.047f;
        c.line(x - cosf(a2) * len, y - sinf(a2) * len, x + cosf(a2) * len, y + sinf(a2) * len, 1, {240, 248, 255},
               (uint8_t)(150 * s));
      }
    }
  }
}
