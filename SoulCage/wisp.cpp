#include "wisp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float CX = 120, CY = 120, FIELD_R = 108, LANTERN_R = 9;

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

float WispGame::random01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng & 0xFFFFFF) / 16777216.0f;
}

void WispGame::start(const MazeMapping &m, uint32_t bestScore, uint32_t jarLights) {
  map = m;
  best = bestScore;
  jar = jarLights;
  state = S_COUNTDOWN;
  stateTime = 0;
  calSum[0] = calSum[1] = calSum[2] = 0;
  calSamples = 0;
  px = CX, py = CY + 20, vx = vy = 0;
  for (auto &t : trail) t[0] = px, t[1] = py;
  lives = 3;
  invulnerable = flash = shake = hurt = 0;
  charge = 1;
  points = lights = 0;
  newRecord = false;
  combo = 0;
  wave = 1;
  waveBanner = 0;
  nextLight = 0.5f, nextGold = 18, nextHeart = 45;
  lightCount = spiritCount = sparkCount = popupCount = 0;
  rng ^= (uint32_t)(time * 1000) + 1;
}

void WispGame::spawnLight(Kind k) {
  if (lightCount == (int)(sizeof(lightsArr) / sizeof(lightsArr[0]))) return;
  // Somewhere inside the glass, not right on top of the lantern.
  float x, y;
  for (int tries = 0; tries < 10; tries++) {
    float a = random01() * 6.2832f, r = FIELD_R * 0.85f * sqrtf(random01());
    x = CX + r * cosf(a), y = CY + r * sinf(a);
    if (hypotf(x - px, y - py) > 50) break;
  }
  float speed = k == K_GOLD ? 55 : 18;
  float a = random01() * 6.2832f;
  lightsArr[lightCount++] = {x, y, cosf(a) * speed, sinf(a) * speed, 0, k == K_BLUE ? 14.0f : 4.5f, random01() * 6, k};
}

void WispGame::spawnSpirit() {
  if (spiritCount == (int)(sizeof(spirits) / sizeof(spirits[0]))) return;
  // From the rim, opposite the lantern if possible.
  float a = atan2f(py - CY, px - CX) + 3.1416f + (random01() - 0.5f) * 1.5f;
  spirits[spiritCount++] = {CX + (FIELD_R - 6) * cosf(a), CY + (FIELD_R - 6) * sinf(a), 0, 0, random01() * 6, 0.8f};
}

void WispGame::burst(float x, float y, int n, float speed, Color c) {
  for (int i = 0; i < n; i++) {
    if (sparkCount == (int)(sizeof(sparks) / sizeof(sparks[0]))) {
      memmove(sparks, sparks + 1, sizeof(Spark) * (sparkCount - 1));
      sparkCount--;
    }
    float a = random01() * 6.2832f, s = speed * (0.3f + 0.7f * random01());
    sparks[sparkCount++] = {x, y, cosf(a) * s, sinf(a) * s, 0.5f + random01() * 0.5f, c};
  }
}

void WispGame::popup(float x, float y, const char *text) {
  if (popupCount == (int)(sizeof(popups) / sizeof(popups[0]))) {
    memmove(popups, popups + 1, sizeof(Popup) * (popupCount - 1));
    popupCount--;
  }
  Popup &p = popups[popupCount++];
  p.x = x, p.y = y, p.age = 0;
  strncpy(p.text, text, sizeof(p.text) - 1);
  p.text[sizeof(p.text) - 1] = 0;
}

bool WispGame::nearestLight(float &x, float &y) const {
  float best2 = 1e9f;
  for (int i = 0; i < lightCount; i++) {
    float d = hypotf(lightsArr[i].x - px, lightsArr[i].y - py) / (lightsArr[i].kind == K_BLUE ? 1 : 2);
    if (d < best2) best2 = d, x = lightsArr[i].x, y = lightsArr[i].y;
  }
  return best2 < 1e9f;
}

bool WispGame::nearestSpirit(float &x, float &y) const {
  float best2 = 1e9f;
  for (int i = 0; i < spiritCount; i++) {
    float d = hypotf(spirits[i].x - px, spirits[i].y - py);
    if (d < best2) best2 = d, x = spirits[i].x, y = spirits[i].y;
  }
  return best2 < 1e9f;
}

// One fixed physics step for the lantern.
void WispGame::step(float h, float tx, float ty) {
  vx += tx * 650 * h;
  vy += ty * 650 * h;
  vx *= 1 - 1.4f * h;
  vy *= 1 - 1.4f * h;
  px += vx * h;
  py += vy * h;
  float dx = px - CX, dy = py - CY, d = sqrtf(dx * dx + dy * dy), lim = FIELD_R - LANTERN_R;
  if (d > lim) {
    float nx = dx / d, ny = dy / d;
    px = CX + nx * lim, py = CY + ny * lim;
    float vn = vx * nx + vy * ny;
    if (vn > 0) vx -= 1.5f * vn * nx, vy -= 1.5f * vn * ny;  // bounce off the glass
  }
}

WispEvent WispGame::update(float dt, float ax, float ay, float az, bool tapped) {
  time += dt;
  stateTime += dt;
  float g[3] = {ax, ay, az};

  int alive = 0;
  for (int i = 0; i < sparkCount; i++) {
    Spark s = sparks[i];
    s.life -= dt;
    if (s.life <= 0) continue;
    s.x += s.vx * dt;
    s.y += s.vy * dt;
    s.vx *= 1 - 2 * dt;
    s.vy *= 1 - 2 * dt;
    sparks[alive++] = s;
  }
  sparkCount = alive;
  alive = 0;
  for (int i = 0; i < popupCount; i++) {
    popups[i].age += dt;
    popups[i].y -= 25 * dt;
    if (popups[i].age < 1) popups[alive++] = popups[i];
  }
  popupCount = alive;

  if (state == S_COUNTDOWN) {
    if (stateTime > 0.3f) {
      for (int i = 0; i < 3; i++) calSum[i] += g[i];
      calSamples++;
    }
    if (stateTime > 2.4f) {
      if (calSamples)
        for (int i = 0; i < 3; i++) neutral[i] = calSum[i] / calSamples;
      state = S_PLAY;
      stateTime = 0;
    }
    return WISP_EV_NONE;
  }
  if (state == S_OVER) {
    if (newRecord && fmodf(stateTime, 0.6f) < dt) {
      burst(40 + random01() * 160, 50 + random01() * 100, 16, 90, {255, 220, 120});
    }
    if (stateTime > 6) state = S_OFF;
    return WISP_EV_NONE;
  }
  if (state != S_PLAY) {
    return WISP_EV_NONE;
  }

  // --- lantern
  float tx = map.signX * (g[map.axisX] - neutral[map.axisX]);
  float ty = map.signY * (g[map.axisY] - neutral[map.axisY]);
  tx = fabsf(tx) < 0.03f ? 0 : clampf(tx, -0.8f, 0.8f);
  ty = fabsf(ty) < 0.03f ? 0 : clampf(ty, -0.8f, 0.8f);
  const float h = 1.0f / 240;
  physicsTime += dt;
  if (physicsTime > 0.1f) physicsTime = 0.1f;
  for (; physicsTime >= h; physicsTime -= h) step(h, tx, ty);
  trail[trailHead][0] = px, trail[trailHead][1] = py;
  trailHead = (trailHead + 1) % 10;

  invulnerable = fmaxf(0, invulnerable - dt);
  flash = fmaxf(0, flash - dt * 2.5f);
  shake = fmaxf(0, shake - dt * 2);
  hurt = fmaxf(0, hurt - dt * 2);
  charge = fminf(1, charge + dt / 4.0f);  // the flash recharges in 4 s

  // --- waves
  int newWave = 1 + (int)(stateTime / 20);
  if (newWave != wave) {
    wave = newWave;
    waveBanner = 2;
  }
  waveBanner = fmaxf(0, waveBanner - dt);
  int wantSpirits = wave - 1 < 6 ? wave - 1 : 6;
  if (spiritCount < wantSpirits && fmodf(stateTime, 3) < dt) spawnSpirit();

  // --- lights
  nextLight -= dt;
  nextGold -= dt;
  nextHeart -= dt;
  int blue = 0;
  for (int i = 0; i < lightCount; i++) blue += lightsArr[i].kind == K_BLUE;
  if (nextLight <= 0 && blue < 6) {
    spawnLight(K_BLUE);
    nextLight = 0.8f + random01() * 0.9f;
  }
  if (nextGold <= 0) {
    spawnLight(K_GOLD);
    nextGold = 14 + random01() * 12;
  }
  if (nextHeart <= 0 && wave >= 3) {
    if (lives < 3) spawnLight(K_HEART);
    nextHeart = 35 + random01() * 20;
  }
  alive = 0;
  for (int i = 0; i < lightCount; i++) {
    Light l = lightsArr[i];
    l.age += dt;
    l.phase += dt;
    l.vx += sinf(l.phase * 1.7f) * 20 * dt;
    l.vy += cosf(l.phase * 1.3f) * 20 * dt;
    l.x += l.vx * dt;
    l.y += l.vy * dt;
    float dx = l.x - CX, dy = l.y - CY, d = sqrtf(dx * dx + dy * dy);
    if (d > FIELD_R - 8) {
      float nx = dx / d, ny = dy / d, vn = l.vx * nx + l.vy * ny;
      l.x = CX + nx * (FIELD_R - 8), l.y = CY + ny * (FIELD_R - 8);
      if (vn > 0) l.vx -= 2 * vn * nx, l.vy -= 2 * vn * ny;
    }
    if (hypotf(l.x - px, l.y - py) < LANTERN_R + 7) {
      char buf[16];
      if (l.kind == K_HEART) {
        lives = lives < 3 ? lives + 1 : 3;
        popup(l.x, l.y, "život!");
        burst(l.x, l.y, 14, 80, {255, 110, 160});
      } else {
        combo = time - lastCatch < 1.5f ? (combo < 5 ? combo + 1 : 5) : 1;
        lastCatch = time;
        int gained = (l.kind == K_GOLD ? 5 : 1) * combo;
        points += gained;
        lights += l.kind == K_GOLD ? 5 : 1;
        snprintf(buf, sizeof(buf), "+%d", gained);
        popup(l.x, l.y, buf);
        burst(l.x, l.y, l.kind == K_GOLD ? 22 : 10, 90, l.kind == K_GOLD ? Color{255, 220, 120} : Color{140, 210, 255});
      }
      continue;
    }
    if (l.age > l.life) continue;  // faded away
    lightsArr[alive++] = l;
  }
  lightCount = alive;

  // --- flash
  if (tapped && charge >= 1) {
    charge = 0;
    flash = 1;
    burst(px, py, 30, 160, {255, 240, 190});
    int kept = 0;
    for (int i = 0; i < spiritCount; i++) {
      if (hypotf(spirits[i].x - px, spirits[i].y - py) < 75) {
        burst(spirits[i].x, spirits[i].y, 14, 70, {255, 80, 90});
        points += 2;
        popup(spirits[i].x, spirits[i].y, "+2");
        continue;
      }
      spirits[kept++] = spirits[i];
    }
    spiritCount = kept;
  }

  // --- red spirits hunt the lantern
  float speed = 22 + 6 * wave;
  for (int i = 0; i < spiritCount; i++) {
    Spirit &s = spirits[i];
    s.phase += dt;
    s.stun = fmaxf(0, s.stun - dt);
    float dx = px - s.x, dy = py - s.y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    float want = s.stun > 0 ? 0 : speed;
    s.vx += ((dx / d) * want - s.vx) * clampf(dt * 1.5f, 0, 1) + sinf(s.phase * 2.3f) * 15 * dt;
    s.vy += ((dy / d) * want - s.vy) * clampf(dt * 1.5f, 0, 1) + cosf(s.phase * 1.9f) * 15 * dt;
    s.x += s.vx * dt;
    s.y += s.vy * dt;
    if (d < LANTERN_R + 9 && invulnerable <= 0) {
      lives--;
      invulnerable = 1.5f;
      shake = 1;
      hurt = 1;
      combo = 0;
      burst(px, py, 20, 120, {255, 70, 80});
      s.x -= dx / d * 40, s.y -= dy / d * 40;  // knocked back
      s.stun = 1;
      if (lives <= 0) {
        state = S_OVER;
        stateTime = 0;
        newRecord = points > best;
        if (newRecord) best = points;
        jar += lights;
        return WISP_EV_OVER;
      }
    }
  }
  return WISP_EV_NONE;
}

// ------------------------------------------------------------------ drawing

static void drawSpirit(Canvas &c, float x, float y, float t, float stun) {
  float wob = sinf(t * 6) * 1.5f;
  c.glow(x, y, 22, 22, {255, 40, 60}, stun > 0 ? 60 : 120);
  c.circle(x, y - 2 + wob, 9, {120, 10, 30});
  c.rect(x - 9, y - 2 + wob, 18, 9, {120, 10, 30});
  for (int k = 0; k < 3; k++) c.circle(x - 6 + k * 6, y + 7 + wob, 3, {120, 10, 30});
  // angry eyes
  c.triangle(x - 6, y - 4 + wob, x - 1, y - 2 + wob, x - 5, y + wob, {255, 220, 90});
  c.triangle(x + 6, y - 4 + wob, x + 1, y - 2 + wob, x + 5, y + wob, {255, 220, 90});
}

void WispGame::drawBackground(Canvas &c) {
  c.gradient({20, 14, 46}, {6, 4, 16});
  c.glow(CX, CY, FIELD_R + 6, FIELD_R + 6, {90, 60, 160}, 70);
  c.glow(CX - 30, CY + 62, 80, 22, {120, 110, 170}, 40);
  c.glow(CX + 40, CY - 58, 60, 16, {120, 110, 170}, 25);
  c.ring(CX, CY, FIELD_R + 2, 2, {150, 120, 220}, 140);
}

void WispGame::render(Canvas &c) {
  int sx = (int)(shake * 5 * sinf(time * 70)), sy = (int)(shake * 4 * cosf(time * 57));
  if (cache) {
    if (!cacheValid) {
      Canvas cc(cache, 240, 240);
      drawBackground(cc);
      cacheValid = true;
    }
    memcpy(c.pixels(), cache, 240 * 240 * 2);
  } else {
    drawBackground(c);
  }
  c.setOffset(sx, sy);

  for (int i = 0; i < lightCount; i++) {
    const Light &l = lightsArr[i];
    float fade = l.life - l.age < 1.5f ? clampf((l.life - l.age) / 1.5f, 0, 1) : 1;
    float tw = 0.75f + 0.25f * sinf(l.phase * 7);
    if (l.kind == K_GOLD) {
      c.glow(l.x, l.y, 20, 20, {255, 200, 80}, (uint8_t)(170 * fade));
      c.circle(l.x, l.y, 5 * tw, {255, 240, 170}, (uint8_t)(255 * fade));
      c.star(l.x + 8, l.y - 7, 4 * tw, {255, 240, 170}, (uint8_t)(220 * fade));
    } else if (l.kind == K_HEART) {
      c.glow(l.x, l.y, 18, 18, {255, 90, 150}, (uint8_t)(150 * fade));
      c.heart(l.x, l.y - 2, 6, {255, 120, 170}, (uint8_t)(255 * fade));
    } else {
      c.glow(l.x, l.y, 17, 17, {110, 190, 255}, (uint8_t)(170 * fade));
      c.circle(l.x, l.y, 4 * tw, {220, 240, 255}, (uint8_t)(255 * fade));
    }
  }
  for (int i = 0; i < spiritCount; i++) drawSpirit(c, spirits[i].x, spirits[i].y, time + spirits[i].phase, spirits[i].stun);

  if (state == S_PLAY || state == S_COUNTDOWN) {
    // trail and lantern
    for (int k = 0; k < 10; k++) {
      int idx = (trailHead + k) % 10;
      c.circle(trail[idx][0], trail[idx][1], 2 + k * 0.4f, {255, 200, 120}, (uint8_t)(10 + k * 9));
    }
    bool blink = invulnerable > 0 && fmodf(time, 0.2f) < 0.1f;
    if (!blink) {
      c.glow(px, py, 34 + 40 * flash, 34 + 40 * flash, {255, 200, 110}, (uint8_t)(110 + 120 * flash));
      c.arc(px, py - 6, 5, 5, 180, 360, 1.6f, {200, 170, 120});
      c.roundRect(px - 6, py - 5, 12, 13, 4, {255, 214, 140});
      c.circle(px, py + 1, 3.5f, {255, 250, 220});
      // flash charge ring
      c.arc(px, py, 13, 13, -90, -90 + 359 * charge, 1.6f, charge >= 1 ? Color{255, 240, 180} : Color{150, 130, 110},
            charge >= 1 ? 230 : 140);
    }
  }
  for (int i = 0; i < sparkCount; i++) {
    c.circle(sparks[i].x, sparks[i].y, 1.8f, sparks[i].c, (uint8_t)(255 * clampf(sparks[i].life * 2, 0, 1)));
  }
  for (int i = 0; i < popupCount; i++) {
    c.text(popups[i].text, popups[i].x, popups[i].y - 10, FONT_SMALL, {255, 255, 255}, (uint8_t)(255 * (1 - popups[i].age)));
  }
  c.setOffset(0, 0);
  if (hurt > 0) c.overlay({255, 30, 50}, (uint8_t)(70 * hurt));

  char buf[32];
  if (state == S_PLAY) {
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)points);
    c.text(buf, 120, 6, FONT_SMALL, {255, 255, 255});
    for (int i = 0; i < 3; i++) {
      c.heart(106 + i * 14, 222, 5, i < lives ? Color{255, 100, 150} : Color{70, 50, 90});
    }
    if (combo > 1 && time - lastCatch < 1.5f) {
      snprintf(buf, sizeof(buf), "x%d", combo);
      c.text(buf, 168, 24, FONT_SMALL, {255, 220, 120});
    }
    if (waveBanner > 0) {
      snprintf(buf, sizeof(buf), "Vlna %d", wave);
      c.text(buf, 120, 96, FONT_BIG, {255, 230, 160}, (uint8_t)(255 * clampf(waveBanner, 0, 1)));
    }
  } else if (state == S_COUNTDOWN) {
    c.overlay({10, 4, 20}, 110);
    c.text("Lovec světýlek", 120, 52, FONT_BIG, {255, 230, 160});
    int k = 3 - (int)(stateTime / 0.8f);
    c.text(k > 0 ? (k == 3 ? "3" : k == 2 ? "2" : "1") : "Jedeš!", 120, 100, FONT_BIG, {255, 255, 255});
    c.text("Nakláněj a sbírej světýlka", 120, 144, FONT_SMALL, {230, 215, 255});
    c.text("Ťuknutí = záblesk", 120, 166, FONT_SMALL, {255, 200, 230});
  } else if (state == S_OVER) {
    c.overlay({10, 4, 20}, 150);
    c.text(newRecord ? "Nový rekord!" : "Konec hry", 120, 56, FONT_BIG, newRecord ? Color{255, 220, 120} : Color{255, 255, 255});
    snprintf(buf, sizeof(buf), "%lu bodů", (unsigned long)points);
    c.text(buf, 120, 98, FONT_BIG, {255, 255, 255});
    snprintf(buf, sizeof(buf), "Rekord: %lu", (unsigned long)best);
    c.text(buf, 120, 140, FONT_SMALL, {230, 215, 255});
    snprintf(buf, sizeof(buf), "Sklenice: %lu/100", (unsigned long)(jar % 100));
    c.text(buf, 120, 166, FONT_SMALL, {140, 210, 255});
    if (jar / 100 > (jar - lights) / 100) c.text("Plná! Duše děkuje", 120, 190, FONT_SMALL, {255, 190, 230});
  }
}
