#include "soul.h"
#include <math.h>
#include <string.h>

// Clips and hand positions measured on the original frames (240x240).
static const SoulDirector::Script SCRIPTS[] = {
    {"soul_m",
     {0, 59, 1.0f, false},    // idle: the original loop
     {22, 26, 0.45f, true},   // bang: both hands on the glass
     {0, 2, 0.5f, true},      // knock
     {9, 15, 2.5f, true},     // calm (sleepy)
     {35, 38, 0.45f, true},   // second banging pose
     false,
     {{24, 40, 115}, {24, 174, 112}, {36, 40, 114}, {36, 176, 104}, {1, 21, 129}, {1, 199, 130}},
     6},
    {"soul_f",
     {0, 50, 1.0f, false},    // idle: fades in, comes close, fades away
     {22, 30, 0.45f, true},   // pressing on the glass
     {18, 24, 0.5f, true},    // knock
     {0, 6, 2.5f, true},      // calm: far away in the fog
     {8, 22, 0.4f, false},    // rushes to the glass
     true,
     {{24, 26, 70}, {28, 24, 69}, {24, 222, 118}},
     3},
};

static const Color GLASS = {236, 238, 246};
static const Color EYE_TINTS[EYES_COUNT] = {{120, 255, 160}, {200, 120, 255}, {255, 70, 70}, {110, 190, 255}};

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

SoulDirector::SoulDirector() {
  setSoul("", 1);
}

float SoulDirector::random01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng & 0xFFFFFF) / 16777216.0f;
}

void SoulDirector::setSoul(const char *id, uint16_t count) {
  frameCount = count ? count : 1;
  script = nullptr;
  for (const Script &s : SCRIPTS) {
    if (strcmp(s.id, id) == 0 && frameCount > s.idle.to) {
      script = &s;
    }
  }
  if (!script) {
    // Unknown animation: whole loop as idle, faster loop as "banging".
    uint8_t last = frameCount - 1 > 255 ? 255 : frameCount - 1;
    generic = {"", {0, last, 1.0f, false}, {0, last, 0.4f, false}, {0, (uint8_t)(last / 4), 0.5f, true},
               {0, last, 2.5f, false}, {0, last, 0.4f, false}, false, {}, 0};
    script = &generic;
  }
  cursor = 0;
  direction = 1;
  crackCount = smudgeCount = 0;
  fade = glitch = shake = 0;
  play(isSleepy ? M_SLEEP : M_IDLE, isSleepy ? script->calm : script->idle, -1);
}

void SoulDirector::play(Mode m, const Clip &c, int cycles) {
  mode = m;
  clip = c;
  cyclesLeft = cycles;
  modeTime = 0;
  if (cursor < c.from || cursor > c.to) {
    cursor = c.from;
    direction = 1;
    frameTimer = 0;
  }
}

void SoulDirector::setSleepy(bool s) {
  if (s == isSleepy) {
    return;
  }
  isSleepy = s;
  if (s) {
    play(M_SLEEP, script->calm, -1);
  } else if (mode == M_SLEEP) {
    play(M_IDLE, script->idle, -1);
  }
}

void SoulDirector::event(SoulEvent e) {
  if (isSleepy) {
    setSleepy(false);
  }
  switch (e) {
    case SOUL_SHAKE:
      if (mode == M_ESCAPE) break;
      if (script->hasApproach && mode != M_BANG) {
        play(M_APPROACH, script->approach, 1);
      } else {
        play(M_BANG, random01() < 0.5f || script->hasApproach ? script->bang : script->approach, 3);
      }
      shake = fmaxf(shake, 0.5f);
      flash = fmaxf(flash, 0.25f);
      break;
    case SOUL_STRONG_SHAKE: {
      play(M_ESCAPE, script->idle, -1);
      bool gentle = spooky == 0;
      addCrackBurst(100 + random01() * 40, 95 + random01() * 40, gentle ? 4 : 8, gentle ? 50 : 115);
      shake = 1;
      flash = 1;
      glitch = gentle ? 0.4f : 1;
      break;
    }
    case SOUL_KNOCK:
      if (mode == M_ESCAPE) break;
      play(M_KNOCK, script->knock, 2);
      break;
    case SOUL_WAKE:
      play(M_KNOCK, script->knock, 1);
      shake = fmaxf(shake, 0.3f);
      break;
  }
}

void SoulDirector::addSmudge(float x, float y) {
  if (smudgeCount == MAX_SMUDGES) {
    memmove(smudges, smudges + 1, sizeof(Smudge) * (MAX_SMUDGES - 1));
    smudgeCount--;
  }
  smudges[smudgeCount++] = {x, y, time};
}

void SoulDirector::addCrackBurst(float x, float y, int rays, float maxLen) {
  auto push = [&](float x0, float y0, float x1, float y1, float w) {
    if (crackCount == MAX_CRACKS) {
      memmove(cracks, cracks + 1, sizeof(Crack) * (MAX_CRACKS - 1));
      crackCount--;
    }
    cracks[crackCount++] = {x0, y0, x1, y1, w, time};
  };
  float ringX[12], ringY[12];
  float base = random01() * 6.283f;
  for (int r = 0; r < rays && r < 12; r++) {
    float ang = base + r * 6.283f / rays + (random01() - 0.5f) * 0.5f;
    float len = maxLen * (0.45f + 0.55f * random01());
    float px = x, py = y, w = 1.8f, done = 0;
    float ringR = 7 + random01() * 6;
    ringX[r] = x + cosf(ang) * ringR;
    ringY[r] = y + sinf(ang) * ringR;
    while (done < len) {
      ang += (random01() - 0.5f) * 0.6f;
      float step = 6 + random01() * 6;
      float nx = px + cosf(ang) * step, ny = py + sinf(ang) * step;
      push(px, py, nx, ny, w);
      if (random01() < 0.22f) {  // short side branch
        float ba = ang + (random01() < 0.5f ? -0.9f : 0.9f), bx = nx, by = ny;
        for (int k = 0; k < 2 + (int)(random01() * 3); k++) {
          ba += (random01() - 0.5f) * 0.5f;
          float ex = bx + cosf(ba) * 6, ey = by + sinf(ba) * 6;
          push(bx, by, ex, ey, w * 0.6f);
          bx = ex, by = ey;
        }
      }
      px = nx, py = ny;
      w = fmaxf(0.7f, w * 0.9f);
      done += step;
    }
  }
  // Spider-web ring around the impact.
  for (int r = 0; r < rays && r < 12; r++) {
    int n = (r + 1) % rays;
    push(ringX[r], ringY[r], ringX[n], ringY[n], 1.0f);
  }
}

void SoulDirector::onFrameShown() {
  bool impact = false;
  for (int i = 0; i < script->impactCount; i++) {
    const Impact &im = script->impacts[i];
    if (im.frame != cursor) continue;
    impact = true;
    if (cursor == lastImpactFrame || (mode != M_BANG && mode != M_KNOCK && mode != M_RETURN)) continue;
    addSmudge(im.x, im.y);
    float chance = spooky == 0 ? 0.0f : spooky == 1 ? (mode == M_KNOCK ? 0.12f : 0.3f) : (mode == M_KNOCK ? 0.3f : 0.6f);
    if (random01() < chance) {
      addCrackBurst(im.x, im.y, 3 + (int)(random01() * 3), 35 + random01() * 25);
    }
  }
  if (impact && cursor != lastImpactFrame && (mode == M_BANG || mode == M_KNOCK || mode == M_RETURN)) {
    shake = fmaxf(shake, mode == M_KNOCK ? 0.25f : 0.55f);
    tintPulse = 1;
  }
  lastImpactFrame = impact ? cursor : -1;
}

void SoulDirector::advance() {
  int next = cursor + direction;
  bool cycleDone = false;
  if (clip.pingpong) {
    if (next > clip.to) {
      direction = -1;
      next = clip.to > clip.from ? clip.to - 1 : clip.from;
    } else if (next < clip.from) {
      direction = 1;
      next = clip.to > clip.from ? clip.from + 1 : clip.from;
      cycleDone = true;
    }
  } else if (next > clip.to) {
    next = clip.from;
    cycleDone = true;
  }
  cursor = next;
  if (cycleDone && cyclesLeft > 0 && --cyclesLeft == 0) {
    switch (mode) {
      case M_APPROACH: play(M_BANG, script->bang, 3); break;
      case M_RETURN:
      case M_BANG:
      case M_KNOCK: play(isSleepy ? M_SLEEP : M_IDLE, isSleepy ? script->calm : script->idle, -1); break;
      default: break;
    }
  }
  onFrameShown();
}

void SoulDirector::update(float dt, float baseDelay, const GhostInput &in, const GhostLook &look) {
  time += dt;
  modeTime += dt;
  spooky = look.spooky;
  lean += (clampf(in.lean, -1, 1) - lean) * clampf(dt * 4, 0, 1);
  flip = in.upsideDown;

  float delay = fmaxf(0.02f, baseDelay * clip.delay);
  frameTimer += dt;
  int guard = 0;
  while (frameTimer >= delay && guard++ < 4) {
    frameTimer -= delay;
    advance();
  }
  if (frameTimer > delay) frameTimer = 0;

  shake = fmaxf(0, shake - dt * 1.4f);
  glitch = fmaxf(0, glitch - dt * 0.7f);
  flash = fmaxf(0, flash - dt * 2.5f);
  tintPulse = fmaxf(0, tintPulse - dt * 2);

  if (mode == M_ESCAPE) {
    // The soul breaks out: glitch, vanish, then come back to the glass.
    if (modeTime < 0.6f) {
      fade = modeTime / 0.6f;
      glitch = fmaxf(glitch, 0.8f);
    } else {
      fade = 1;
    }
    if (modeTime > 2.4f) {
      if (script->hasApproach) {
        play(M_APPROACH, script->approach, 1);
      } else {
        play(M_RETURN, script->bang, 2);
      }
      flickerUntil = time + 0.8f;
    }
  } else {
    float target = isSleepy ? 0.35f : 0;
    fade += (target - fade) * clampf(dt * 1.5f, 0, 1);
  }

  if (mode == M_IDLE && !isSleepy && time > nextEvent) {
    float r = random01();
    if (r < 0.55f) {
      event(SOUL_SHAKE);
      shake = 0.2f;  // a spontaneous attempt is gentler than a real shake
    } else if (r < 0.85f) {
      event(SOUL_KNOCK);
    } else {
      flickerUntil = time + 0.5f;
    }
    nextEvent = time + (spooky == 2 ? 10 + random01() * 15 : spooky == 1 ? 18 + random01() * 27 : 30 + random01() * 40);
  }
}

void SoulDirector::pixelPass(const uint8_t *src, uint16_t *dst, bool inPlace, Color tint, float tintAmount) {
  int sx = inPlace ? 0 : (int)lroundf(lean * 16 + shake * 7 * sinf(time * 71));
  int sy = inPlace ? 0 : (int)lroundf(shake * 5 * cosf(time * 53));
  bool flipped = flip && !inPlace;
  int f = (int)(clampf(fade, 0, 1) * 256);
  int t = (int)(clampf(tintAmount, 0, 1) * 256);
  bool flicker = time < flickerUntil && fmodf(time, 0.12f) < 0.06f;
  int bright = flicker ? 150 : 256;
  int vig = 70 + spooky * 40;  // vignette strength
  for (int y = 0; y < 240; y++) {
    int syy = (flipped ? 239 - y : y) - sy;
    syy = syy < 0 ? 0 : syy > 239 ? 239 : syy;
    int dy = y - 120;
    for (int x = 0; x < 240; x++) {
      int sxx = x - sx;
      sxx = sxx < 0 ? 0 : sxx > 239 ? 239 : sxx;
      const uint8_t *p = src + (syy * 240 + sxx) * 2;
      uint16_t v = (p[0] << 8) | p[1];
      int r = (v >> 8) & 0xF8, g = (v >> 3) & 0xFC, b = (v << 3) & 0xF8;
      if (f) {
        r += ((GLASS.r - r) * f) >> 8;
        g += ((GLASS.g - g) * f) >> 8;
        b += ((GLASS.b - b) * f) >> 8;
      }
      if (t) {
        // Tint the dark soul, keep the bright glass
        int k = t * (255 - ((r + g + b) / 3)) >> 8;
        r += ((tint.r - r) * k) >> 8;
        g += ((tint.g - g) * k) >> 8;
        b += ((tint.b - b) * k) >> 8;
      }
      int dx = x - 120;
      int d2 = dx * dx + dy * dy;  // 0..28800
      int k = bright;
      if (d2 > 7000) {
        k -= (d2 - 7000) * vig / 21800;
      }
      if (k < 0) k = 0;
      r = r * k >> 8;
      g = g * k >> 8;
      b = b * k >> 8;
      // In place, src and dst are the same pixel and it was read above.
      dst[y * 240 + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
  }
}

void SoulDirector::render(const uint8_t *src, Canvas &c, const GhostLook &look) {
  uint16_t *dst = c.pixels();
  bool inPlace = (const void *)src == (const void *)dst;
  Color tint = EYE_TINTS[look.eyeColor % EYES_COUNT];
  float tintAmount = spooky * 0.06f + (spooky ? tintPulse * 0.25f : 0);
  pixelPass(src, dst, inPlace, tint, tintAmount);

  // The glass moves with the pendant (camera shake), the soul behind it also leans.
  int shx = (int)lroundf(shake * 7 * sinf(time * 71)), shy = (int)lroundf(shake * 5 * cosf(time * 53));
  c.setOffset(shx, shy);
  for (int i = 0; i < smudgeCount; i++) {
    float age = time - smudges[i].born;
    if (age > 6) continue;
    uint8_t a = (uint8_t)(90 * (1 - age / 6));
    c.glow(smudges[i].x, smudges[i].y, 30, 34, {255, 255, 255}, a);
  }
  float life = spooky == 2 ? 60 : 30;
  int alive = 0;
  for (int i = 0; i < crackCount; i++) {
    const Crack &k = cracks[i];
    float age = time - k.born;
    if (age >= life) continue;
    cracks[alive++] = k;
    float a = age < life - 5 ? 1 : (life - age) / 5;
    c.line(k.x0 + 0.8f, k.y0 + 0.8f, k.x1 + 0.8f, k.y1 + 0.8f, k.width + 1, {30, 30, 40}, (uint8_t)(110 * a));
    c.line(k.x0, k.y0, k.x1, k.y1, k.width, {248, 250, 255}, (uint8_t)(235 * a));
  }
  crackCount = alive;
  c.setOffset(0, 0);

  if (glitch > 0.05f) {
    // Torn horizontal bands, like a broken signal.
    int bands = 2 + (int)(glitch * 6);
    for (int i = 0; i < bands; i++) {
      int y0 = (int)(random01() * 230), h = 3 + (int)(random01() * 16);
      int off = (int)((random01() - 0.5f) * 60 * glitch);
      for (int y = y0; y < y0 + h && y < 240; y++) {
        uint16_t *row = dst + y * 240;
        if (off > 0) {
          memmove(row + off, row, (240 - off) * 2);
        } else if (off < 0) {
          memmove(row, row - off, (240 + off) * 2);
        }
      }
      c.rect(0, y0, 240, h, i % 2 ? Color{255, 60, 200} : Color{60, 255, 230}, (uint8_t)(50 * glitch));
    }
  }
  if (flash > 0.01f) {
    c.overlay({255, 255, 255}, (uint8_t)(flash * 110));
  }
}
