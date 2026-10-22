#include "ghost.h"
#include <math.h>
#include <string.h>

// ------------------------------------------------------------------ helpers

static const float PI_F = 3.14159265f;

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}
static float ease(float x) {
  x = clampf(x, 0, 1);
  return x * x * (3 - 2 * x);
}
static float easeOut(float x) {
  x = clampf(x, 0, 1);
  return 1 - (1 - x) * (1 - x) * (1 - x);
}
static float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}
static float seg(float t, float a, float b) {
  return (t - a) / (b - a);
}
// Deterministic pseudo random 0..1 from an integer (stars, snow...).
static float hash01(uint32_t n) {
  n = (n << 13) ^ n;
  n = n * (n * n * 15731u + 789221u) + 1376312589u;
  return (n & 0x7fffffff) / 2147483647.0f;
}

static const Color INK = {10, 8, 16};
static const Color BODY = {232, 238, 255};
static const Color WHITE = {255, 255, 255};
static const Color BLUSH = {255, 150, 190};
static const Color EYE_IDLE = {200, 235, 255};
static const Color EYE_COLORS[EYES_COUNT] = {{130, 255, 170}, {205, 140, 255}, {255, 95, 95}, {120, 200, 255}};

enum Eyes : uint8_t { E_DOTS, E_GLOW, E_CLOSED, E_HAPPY, E_SCARED, E_DIZZY, E_SAD };
enum Mouth : uint8_t { M_SMIRK, M_SMILE, M_GRIN, M_LAUGH, M_BOO, M_O, M_WOBBLE, M_SLEEP, M_FROWN, M_CHEW };

struct GhostPose {
  float x = 0, y = 0, scale = 1, squash = 0, tilt = 0, alpha = 1;
  float lookX = 0, lookY = 0;
  Eyes eyes = E_DOTS;
  Mouth mouth = M_SMIRK;
  float mouthOpen = 1;
  float spooky = 0, shake = 0, flash = 0, zzz = 0, fog = 0.35f, blush = 1;
  bool waveArm = false;
  float spinX = 1;      // horizontal squeeze while spinning, <0 = back side
  float hearts = 0;     // progress 0..1, 0 = none
  float sparkles = 0;   // intensity
  int foodItem = -1;
  float foodLeft = 1, foodDrop = 0;
  bool bubble = false;
  bool ball = false;
  float ballX = 0, ballY = 0;
  bool flip = false;
  Color flashColor = {200, 255, 210};
};

// ------------------------------------------------------------------ names

static const char *const ACTION_IDS[GA_COUNT] = {"none", "wave", "dance", "boo", "hide", "spin", "laugh", "love",
                                                 "scared", "dizzy", "eat", "refuse", "play", "yawn", "levelup"};
static const char *const ACTION_NAMES[GA_COUNT] = {"",          "zamávej",   "tancuj",   "BAF!",      "schovej se",
                                                   "zatoč se",  "smích",     "srdíčka",  "lekni se",  "motá se",
                                                   "jí",        "nechce",    "hraje si", "zívá",      "nový level"};

const char *ghostActionId(GhostAction action) {
  return action < GA_COUNT ? ACTION_IDS[action] : "";
}
const char *ghostActionName(GhostAction action) {
  return action < GA_COUNT ? ACTION_NAMES[action] : "";
}
int ghostActionFromId(const char *id) {
  for (int i = 1; i < GA_COUNT; i++) {
    if (strcmp(id, ACTION_IDS[i]) == 0) return i;
  }
  return -1;
}
const char *sceneName(uint8_t scene) {
  static const char *const names[SCENE_COUNT] = {"Hřbitov", "Bambusový les", "Strašidelný dům", "Sklenice", "Zima"};
  return scene < SCENE_COUNT ? names[scene] : "";
}
const char *eyeColorName(uint8_t color) {
  static const char *const names[EYES_COUNT] = {"zelené", "fialové", "červené", "modré"};
  return color < EYES_COUNT ? names[color] : "";
}

// ------------------------------------------------------------------ state machine

Ghost::Ghost() {}

float Ghost::random01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng & 0xFFFFFF) / 16777216.0f;
}

float Ghost::actionDuration(GhostAction a) const {
  switch (a) {
    case GA_WAVE: return 2.0f;
    case GA_DANCE: return 4.0f;
    case GA_BOO: return 2.4f;
    case GA_HIDE: return 3.0f;
    case GA_SPIN: return 1.6f;
    case GA_LAUGH: return 1.8f;
    case GA_LOVE: return 2.6f;
    case GA_SCARED: return 1.3f;
    case GA_DIZZY: return 2.6f;
    case GA_EAT: return 3.2f;
    case GA_REFUSE: return 1.5f;
    case GA_PLAY: return 4.0f;
    case GA_YAWN: return 2.2f;
    case GA_LEVELUP: return 3.2f;
    default: return 0;
  }
}

void Ghost::trigger(GhostAction a, Food f) {
  if (a == GA_NONE || a >= GA_COUNT) {
    return;
  }
  if (asleep) {
    asleep = false;  // any interaction wakes the ghost up
  }
  action = a;
  actionTime = 0;
  food = f;
}

void Ghost::setSleeping(bool s) {
  if (s == asleep) {
    return;
  }
  asleep = s;
  action = s ? GA_NONE : GA_YAWN;
  actionTime = 0;
}

void Ghost::update(float dt, const GhostInput &in, const GhostLook &look) {
  time += dt;
  if (action != GA_NONE) {
    actionTime += dt;
    if (actionTime >= actionDuration(action)) {
      action = GA_NONE;
    }
  }
  float k = clampf(dt * 1.5f, 0, 1);
  sleepBlend += ((asleep ? 1.0f : 0.0f) - sleepBlend) * k;
  lean += (clampf(in.lean, -1, 1) - lean) * clampf(dt * 4, 0, 1);
  flipBlend += ((in.upsideDown ? 1.0f : 0.0f) - flipBlend) * clampf(dt * 6, 0, 1);
  flatLook = in.lyingFlat;

  if (time > nextBlink) {
    blinkUntil = time + 0.13f;
    nextBlink = time + 2.2f + random01() * 3.5f;
  }

  if (!spontaneous || asleep || action != GA_NONE || time < nextIdleEvent) {
    return;
  }
  float spookyRate = look.spooky == 2 ? 0.6f : 1.0f;
  nextIdleEvent = time + (12 + random01() * 25) * spookyRate;
  if (needTired && random01() < 0.6f) {
    trigger(GA_YAWN);
    return;
  }
  if (needHungry && random01() < 0.7f) {
    bubbleUntil = time + 4;
    return;
  }
  // Weighted pick of a spontaneous action.
  const GhostAction options[] = {GA_WAVE, GA_SPIN, GA_LAUGH, GA_HIDE, GA_BOO, GA_DANCE};
  float weights[] = {3, 2, 2, 1.5f, look.spooky == 0 ? 0.0f : look.spooky == 1 ? 1.5f : 4.0f, 1};
  if (needBored) {
    weights[0] = weights[2] = weights[5] = 0.3f;  // bored ghosts mope instead of dancing
  }
  float total = 0;
  for (float w : weights) total += w;
  float r = random01() * total;
  for (int i = 0; i < 6; i++) {
    r -= weights[i];
    if (r <= 0) {
      trigger(options[i]);
      return;
    }
  }
}

void Ghost::computePose(GhostPose &p, const GhostLook &look) const {
  const float T = time;
  // Idle floating, plus leaning with the pendant.
  p.x = 10 * sinf(T * 0.9f) + lean * 38;
  p.y = 7 * sinf(T * 2.1f);
  p.tilt = 6 * sinf(T * 1.3f) + lean * 14;
  p.lookX = clampf(0.8f * sinf(T * 1.1f) + lean, -1, 1);
  p.lookY = 0.35f * sinf(T * 1.7f);
  p.eyes = E_DOTS;
  p.mouth = look.level >= 3 ? M_SMIRK : M_SMILE;
  p.spooky = look.spooky == 2 ? 0.25f : 0;
  if (flatLook) {
    p.lookY = -1;
    p.lookX *= 0.3f;
  }
  if (needBored) {
    p.eyes = E_SAD;
    p.mouth = M_FROWN;
    p.lookY = 0.8f;
    p.y += 6;
  } else if (needTired) {
    p.eyes = E_SAD;
    p.y += 4;
  }
  if (time < bubbleUntil) {
    p.bubble = true;
  }
  if (T < blinkUntil && p.eyes == E_DOTS) {
    p.eyes = E_CLOSED;
  }

  const float t = actionTime;
  switch (action) {
    case GA_WAVE:
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
      p.waveArm = true;
      p.y -= fabsf(sinf(t * 6)) * 4;
      break;
    case GA_DANCE:
      p.x = 28 * sinf(t * 4) + lean * 20;
      p.y = -fabsf(sinf(t * 8)) * 10;
      p.tilt = 16 * sinf(t * 4);
      p.squash = 0.08f * sinf(t * 8);
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
      p.waveArm = sinf(t * 4) > 0;
      p.sparkles = 1;
      break;
    case GA_BOO:
      p.eyes = E_GLOW;
      p.lookX = lerpf(p.lookX, 0, ease(t / 0.6f));
      p.lookY = lerpf(p.lookY, 0.1f, ease(t / 0.6f));
      if (t < 0.9f) {
        float k = ease(t / 0.8f);
        p.spooky = fmaxf(p.spooky, ease(t / 0.9f));
        p.scale = 1 + 0.08f * k;
        p.fog += 0.35f * k;
        p.mouth = t > 0.45f ? M_GRIN : M_SMIRK;
      } else if (t < 1.2f) {
        float k = ease(seg(t, 0.9f, 1.2f));
        p.spooky = 1;
        p.scale = lerpf(1.08f, 0.88f, k);
        p.y -= 14 * k;
        p.squash = -0.12f * k;
        p.mouth = M_GRIN;
        p.fog = 0.7f;
      } else if (t < 1.4f) {
        float k = easeOut(seg(t, 1.2f, 1.4f));
        p.spooky = 1;
        p.scale = lerpf(0.88f, 1.55f, k);
        p.mouth = M_BOO;
        p.flash = k;
        p.shake = k;
        p.squash = 0.1f * k;
        p.fog = 0.7f;
      } else if (t < 1.8f) {
        p.spooky = 1;
        p.scale = 1.55f + 0.04f * sinf(t * 40);
        p.mouth = M_BOO;
        p.shake = 1;
        p.flash = 1 - seg(t, 1.4f, 1.8f);
        p.fog = 0.7f;
      } else if (t < 2.0f) {
        float k = ease(seg(t, 1.8f, 2.0f));
        p.scale = lerpf(1.55f, 1, k);
        p.mouth = M_GRIN;
        p.spooky = 1 - 0.6f * k;
      } else {
        p.eyes = E_HAPPY;
        p.mouth = M_LAUGH;
        p.spooky = 0.4f * (1 - seg(t, 2.0f, 2.4f));
        p.y -= fabsf(sinf(t * 14)) * 5;
      }
      break;
    case GA_HIDE:
      if (t < 0.6f) {
        float flicker = hash01((uint32_t)(T * 25)) > 0.3f ? 1.0f : 0.5f;
        p.alpha = (1 - t / 0.6f) * flicker;
        p.eyes = E_GLOW;
        p.spooky = fmaxf(p.spooky, 0.5f);
      } else if (t < 1.6f) {
        p.alpha = 0;
      } else if (t < 2.3f) {
        float k = ease(seg(t, 1.6f, 2.3f));
        p.alpha = k;
        p.x = lerpf(-90, p.x, k);
        p.lookX = 1;
      } else {
        p.eyes = E_HAPPY;
        p.mouth = M_LAUGH;
        p.y -= fabsf(sinf(t * 14)) * 5;
      }
      break;
    case GA_SPIN: {
      float ph = t / 1.6f;
      p.spinX = cosf(ph * 2 * PI_F * 2);
      p.y -= sinf(ph * PI_F) * 14;
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
      p.sparkles = ph > 0.7f ? (1 - ph) * 3 : 0;
      break;
    }
    case GA_LAUGH:
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
      p.y -= fabsf(sinf(t * 14)) * 6;
      p.tilt += 10 * sinf(t * 9);
      break;
    case GA_LOVE:
      p.eyes = E_HAPPY;
      p.mouth = M_SMILE;
      p.blush = 2;
      p.hearts = t / 2.6f;
      p.spooky = 0;
      break;
    case GA_SCARED: {
      float k = 1 - t / 1.3f;
      p.eyes = E_SCARED;
      p.mouth = M_O;
      p.shake = 0.8f * k;
      p.x += 9 * sinf(t * 55) * k;
      p.tilt += 12 * sinf(t * 40) * k;
      p.squash = 0.08f * sinf(t * 30) * k;
      break;
    }
    case GA_DIZZY:
      p.eyes = E_DIZZY;
      p.mouth = M_WOBBLE;
      p.tilt = 18 * sinf(t * 5);
      p.x += 12 * sinf(t * 5);
      break;
    case GA_EAT: {
      p.foodItem = food;
      p.lookX = 0;
      p.lookY = 0.6f;
      if (t < 0.5f) {
        p.foodDrop = 1 - easeOut(t / 0.5f);
        p.mouth = M_O;
      } else if (t < 2.3f) {
        int bites = (int)((t - 0.6f) / 0.6f) + (t > 0.6f ? 1 : 0);
        p.foodLeft = 1 - clampf(bites / 3.0f, 0, 1);
        float phase = fmodf(t - 0.6f, 0.6f);
        p.mouth = t > 0.6f && phase < 0.2f ? M_CHEW : M_SMILE;
        p.eyes = t > 0.6f ? E_HAPPY : E_DOTS;
        if (p.foodLeft <= 0) p.foodItem = -1;
      } else {
        p.foodItem = -1;
        p.eyes = E_HAPPY;
        p.mouth = M_LAUGH;
        p.hearts = seg(t, 2.3f, 3.2f) * 0.8f;
      }
      break;
    }
    case GA_REFUSE: {
      float k = 1 - t / 1.5f;
      p.x += 8 * sinf(t * 18) * k;
      p.eyes = E_CLOSED;
      p.mouth = M_FROWN;
      break;
    }
    case GA_PLAY: {
      p.ball = t < 3.3f;
      float bt = t * 1.6f;
      p.ballX = 32 * sinf(bt * PI_F);
      p.ballY = -52 - 24 * fabsf(cosf(bt * PI_F));
      p.lookX = clampf(p.ballX / 32, -1, 1);
      p.lookY = -0.8f;
      p.mouth = M_SMILE;
      p.y -= fabsf(sinf(bt * PI_F * 2)) * 3;
      if (t > 3.3f) {
        p.eyes = E_HAPPY;
        p.mouth = M_LAUGH;
        p.sparkles = 1;
      }
      break;
    }
    case GA_YAWN: {
      float k = sinf(clampf(t / 2.2f, 0, 1) * PI_F);
      p.eyes = E_CLOSED;
      p.mouth = M_O;
      p.mouthOpen = 1 + 0.9f * k;
      p.squash = -0.06f * k;
      break;
    }
    case GA_LEVELUP:
      if (t < 1.2f) {
        p.spinX = cosf(t / 1.2f * 2 * PI_F * 2);
        p.y -= sinf(t / 1.2f * PI_F) * 18;
      }
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
      p.sparkles = 1.5f;
      p.flash = t > 1.2f && t < 1.6f ? 1 - seg(t, 1.2f, 1.6f) : 0;
      p.flashColor = {255, 230, 140};
      p.scale = t > 1.2f ? 1 + 0.06f * sinf(seg(t, 1.2f, 3.2f) * PI_F) : 1;
      break;
    default:
      break;
  }

  // Sleeping overrides the face and sinks the ghost a bit.
  if (sleepBlend > 0.02f) {
    float b = sleepBlend;
    if (b > 0.5f) {
      p.eyes = E_CLOSED;
      p.mouth = M_SLEEP;
      p.bubble = false;
    }
    p.y += 18 * b;
    p.zzz = b;
    p.fog += 0.15f * b;
    p.tilt *= 1 - 0.6f * b;
    p.x *= 1 - 0.5f * b;
  }
  if (flipBlend > 0.5f) {
    p.flip = true;
    if (action == GA_NONE) {
      p.eyes = E_HAPPY;
      p.mouth = M_LAUGH;
    }
  }
  if (look.spooky == 0) {
    p.spooky *= 0.5f;  // cute mode keeps even BOO gentle
  }
}

// ------------------------------------------------------------------ scenes

static void drawBats(Canvas &c, float T, int count) {
  for (int i = 0; i < count; i++) {
    float bx = fmodf(T * 35 + i * 130, 300) - 30, by = 50 + i * 28 + 6 * sinf(T * 3 + i);
    float f = 7 * fabsf(sinf(T * 11 + i));
    c.triangle(bx, by, bx - 12, by - f, bx - 5, by + 3, INK);
    c.triangle(bx, by, bx + 12, by - f, bx + 5, by + 3, INK);
    c.circle(bx, by + 1, 2.5f, INK);
  }
}

static void drawStars(Canvas &c, float T, int count, float maxY) {
  for (int i = 0; i < count; i++) {
    float x = 10 + hash01(i * 7 + 1) * 220, y = 8 + hash01(i * 7 + 2) * maxY;
    float b = 0.5f + 0.5f * sinf(T * 3 + hash01(i * 7 + 3) * 6);
    c.circle(x, y, 0.8f + b, mixColor({90, 90, 120}, {255, 250, 210}, b));
  }
}

static void drawScene(Canvas &c, uint8_t scene, float T, float sp, Color eye) {
  Color tint = {(uint8_t)(eye.r / 9), (uint8_t)(eye.g / 9), (uint8_t)(eye.b / 9)};
  switch (scene) {
    case SCENE_BAMBOO: {
      c.gradient(mixColor({14, 34, 30}, tint, sp), mixColor({8, 20, 18}, tint, sp * 0.5f));
      const float xs[] = {14, 40, 194, 220};
      for (int i = 0; i < 4; i++) {
        Color stalk = i % 2 ? Color{40, 95, 52} : Color{50, 112, 60};
        c.rect(xs[i], -5, 11, 250, stalk);
        for (int y = 18; y < 240; y += 46) c.rect(xs[i] - 1, y + i * 5, 13, 3, {30, 70, 40});
        c.ellipse(xs[i] + (i < 2 ? 18 : -8), 70 + i * 23, 12, 4, {70, 140, 70}, 255, i < 2 ? -25 : 25);
        c.ellipse(xs[i] + (i < 2 ? 16 : -6), 150 + i * 11, 11, 3.5f, {62, 128, 64}, 255, i < 2 ? 20 : -20);
      }
      for (int i = 0; i < 7; i++) {
        float a = T * 0.8f + i * 1.7f;
        float x = 120 + 85 * sinf(a * 0.7f + i), y = 50 + 150 * fabsf(sinf(a * 0.45f + i * 0.9f));
        float b = 0.5f + 0.5f * sinf(T * 5 + i);
        c.glow(x, y, 9, 9, {230, 255, 140}, (uint8_t)(90 * b));
        c.circle(x, y, 1.6f, {240, 255, 170});
      }
      break;
    }
    case SCENE_HOUSE: {
      c.gradient(mixColor({40, 22, 58}, tint, sp), mixColor({20, 12, 32}, tint, sp * 0.5f));
      drawStars(c, T, 12, 90);
      c.glow(186, 56, 50, 50, {255, 240, 190}, 60);
      c.circle(186, 56, 24, {255, 240, 185});
      c.circle(178, 50, 4, {235, 220, 160});
      c.circle(192, 64, 3, {235, 220, 160});
      c.triangle(10, 154, 130, 154, 70, 98, {26, 18, 40});
      c.rect(22, 150, 96, 95, {26, 18, 40});
      c.rect(92, 108, 12, 28, {26, 18, 40});
      for (int i = 0; i < 4; i++) {
        float wx = i % 2 ? 80 : 40, wy = i < 2 ? 162 : 198;
        float on = hash01((uint32_t)(T * 2) + i * 31) > 0.15f ? 1.0f : 0.35f;
        c.rect(wx, wy, 18, 18, {255, 205, 90}, (uint8_t)(255 * on));
        c.rect(wx + 8, wy, 2, 18, {26, 18, 40});
        c.rect(wx, wy + 8, 18, 2, {26, 18, 40});
      }
      drawBats(c, T, 3);
      break;
    }
    case SCENE_JAR: {
      c.gradient(mixColor({14, 13, 22}, tint, sp), mixColor({30, 22, 30}, tint, sp * 0.5f));
      c.rect(0, 214, 240, 30, {62, 42, 32});
      c.rect(0, 214, 240, 3, {82, 58, 44});
      c.roundRect(46, 34, 148, 182, 26, {160, 200, 255}, 22);  // glass tint
      c.line(46, 60, 46, 190, 3, {180, 210, 240}, 120);
      c.line(194, 60, 194, 190, 3, {180, 210, 240}, 120);
      c.arc(120, 190, 74, 26, 0, 180, 3, {180, 210, 240}, 120);
      c.line(62, 70, 62, 120, 5, WHITE, 45);  // reflection
      c.line(70, 70, 70, 90, 3, WHITE, 35);
      c.roundRect(52, 20, 136, 20, 5, {120, 120, 134});
      for (int x = 60; x < 186; x += 10) c.rect(x, 22, 3, 16, {95, 95, 108});
      break;
    }
    case SCENE_WINTER: {
      c.gradient(mixColor({18, 28, 60}, tint, sp), mixColor({58, 78, 118}, tint, sp * 0.5f));
      drawStars(c, T, 10, 70);
      for (int i = 0; i < 2; i++) {
        float x = i ? 205 : 30;
        for (int k = 0; k < 3; k++) {
          float y = 150 + k * 22, w = 22 + k * 8;
          c.triangle(x - w, y + 26, x + w, y + 26, x, y - 8, {22, 60, 50});
          c.triangle(x - w * 0.5f, y + 4, x + w * 0.5f, y + 4, x, y - 8, {235, 240, 255});
        }
      }
      c.ellipse(120, 262, 210, 52, {225, 235, 250});
      for (int i = 0; i < 34; i++) {
        float speed = 0.5f + hash01(i * 3 + 1);
        float x = fmodf(hash01(i * 3 + 2) * 260 + sinf(T + i) * 8, 250);
        float y = fmodf(hash01(i * 3 + 3) * 260 + T * 22 * speed, 260) - 10;
        c.circle(x, y, 0.8f + speed, WHITE, 220);
      }
      break;
    }
    default: {  // graveyard
      Color top = mixColor({14, 16, 36}, tint, sp), bottom = mixColor({30, 24, 52}, tint, sp * 0.6f);
      c.gradient(top, bottom);
      drawStars(c, T, 18, 130);
      Color moon = mixColor({255, 236, 170}, mixColor({255, 255, 255}, eye, 0.5f), sp);
      c.circle(188, 50, 26, moon);
      c.circle(201, 43, 25, mixColor(top, bottom, 43.0f / 240));
      c.line(38, 245, 39, 150, 12, {8, 8, 14});
      c.line(39, 172, 20, 140, 5, {8, 8, 14});
      c.line(20, 140, 13, 126, 3, {8, 8, 14});
      c.line(39, 160, 54, 138, 5, {8, 8, 14});
      c.line(54, 138, 66, 128, 3, {8, 8, 14});
      const float gxs[] = {14, 60, 166, 206}, ghs[] = {44, 30, 34, 50};
      for (int i = 0; i < 4; i++) {
        c.roundRect(gxs[i], 240 - ghs[i], 24, ghs[i] + 14, 11, {46, 46, 64});
        c.rect(gxs[i] + 11, 248 - ghs[i], 2.5f, 13, {30, 30, 42});
        c.rect(gxs[i] + 7, 252 - ghs[i], 10, 2.5f, {30, 30, 42});
      }
      drawBats(c, T, 2);
      break;
    }
  }
}

static void drawFog(Canvas &c, float T, float amount, float sp, Color eye) {
  Color fogC = mixColor({150, 150, 190}, mixColor({150, 150, 190}, eye, 0.6f), sp);
  for (int i = 0; i < 4; i++) {
    float x = fmodf(hash01(i + 40) * 340 + (hash01(i + 50) - 0.5f) * T * 18 + 400, 340) - 50;
    float y = 190 + hash01(i + 60) * 55;
    c.glow(x, y, 70 + hash01(i + 70) * 40, 20, fogC, (uint8_t)clampf(amount * 110, 0, 255));
  }
}

// ------------------------------------------------------------------ props

static void drawFood(Canvas &c, int type, float x, float y, float s, uint8_t a) {
  switch (type) {
    case FOOD_BAMBOO:
      c.roundRect(x - s * 0.25f, y - s, s * 0.5f, s * 2, s * 0.2f, {90, 170, 80}, a);
      c.rect(x - s * 0.28f, y - s * 0.2f, s * 0.56f, s * 0.15f, {60, 120, 55}, a);
      c.ellipse(x + s * 0.55f, y - s * 0.7f, s * 0.6f, s * 0.2f, {110, 200, 90}, a, -30);
      break;
    case FOOD_COOKIE:
      c.circle(x, y, s, {196, 140, 80}, a);
      c.circle(x - s * 0.35f, y - s * 0.3f, s * 0.15f, {90, 55, 30}, a);
      c.circle(x + s * 0.3f, y + s * 0.1f, s * 0.17f, {90, 55, 30}, a);
      c.circle(x - s * 0.1f, y + s * 0.45f, s * 0.13f, {90, 55, 30}, a);
      break;
    case FOOD_PIE:
      c.triangle(x - s, y + s * 0.6f, x + s, y + s * 0.6f, x, y - s * 0.9f, {245, 140, 40}, a);
      c.rect(x - s, y + s * 0.45f, s * 2, s * 0.35f, {170, 110, 60}, a);
      break;
    case FOOD_CANDY:
      c.triangle(x - s * 0.6f, y, x - s * 1.3f, y - s * 0.5f, x - s * 1.3f, y + s * 0.5f, {255, 120, 180}, a);
      c.triangle(x + s * 0.6f, y, x + s * 1.3f, y - s * 0.5f, x + s * 1.3f, y + s * 0.5f, {255, 120, 180}, a);
      c.circle(x, y, s * 0.7f, {255, 90, 160}, a);
      c.arc(x, y, s * 0.4f, s * 0.4f, 200, 320, s * 0.18f, WHITE, a);
      break;
  }
}

static void drawZ(Canvas &c, float x, float y, float s, Color col, uint8_t a) {
  float pts[] = {x - s, y - s, x + s, y - s, x - s, y + s, x + s, y + s};
  c.polyline(pts, 4, fmaxf(1.5f, s * 0.35f), col, a);
}

// ------------------------------------------------------------------ the ghost

namespace {
// Maps ghost-local coordinates (origin = head centre, unscaled) to the screen.
struct Xf {
  float gx, gy, sx, sy, shear, fy;
  float X(float lx, float ly) const { return gx + (lx + shear * ly) * sx; }
  float Y(float ly) const { return gy + fy * ly * sy; }
};
}  // namespace

static void localArc(Canvas &c, const Xf &f, float cx, float cy, float rx, float ry, float a0, float a1, float width,
                     Color col, uint8_t a) {
  const int n = 14;
  float pts[(n + 1) * 2];
  for (int i = 0; i <= n; i++) {
    float an = (a0 + (a1 - a0) * i / n) * 0.017453293f;
    float lx = cx + rx * cosf(an), ly = cy + ry * sinf(an);
    pts[i * 2] = f.X(lx, ly);
    pts[i * 2 + 1] = f.Y(ly);
  }
  c.polyline(pts, n + 1, width * fminf(f.sx, f.sy), col, a);
}

static void localEllipse(Canvas &c, const Xf &f, float lx, float ly, float rx, float ry, Color col, uint8_t a,
                         float angle = 0) {
  c.ellipse(f.X(lx, ly), f.Y(ly), rx * f.sx, ry * f.sy, col, a, angle * f.fy);
}

static void localTriangle(Canvas &c, const Xf &f, float x0, float y0, float x1, float y1, float x2, float y2, Color col,
                          uint8_t a) {
  c.triangle(f.X(x0, y0), f.Y(y0), f.X(x1, y1), f.Y(y1), f.X(x2, y2), f.Y(y2), col, a);
}

// Lower half of an ellipse (open mouth), shaded in local coordinates.
static void localChord(Canvas &c, const Xf &f, float cx, float cy, float rx, float ry, Color col, uint8_t a) {
  float r = fmaxf(rx * f.sx, ry * f.sy) + 2;
  float sx = f.X(cx, cy), sy = f.Y(cy);
  float scale = fminf(f.sx, f.sy);
  c.shade(sx - r, sy - r, sx + r, sy + r, col, a, [&](float px, float py) {
    float ly = (py - f.gy) / (f.fy * f.sy);
    float lx = (px - f.gx) / f.sx - f.shear * ly;
    float u = (lx - cx) / rx, v = (ly - cy) / ry;
    float inside = (1 - sqrtf(u * u + v * v)) * fminf(rx, ry) * scale;
    float below = (ly - cy) * scale;
    return clampf(fminf(inside, below) + 0.5f, 0, 1);
  });
}

static void drawGhost(Canvas &c, const GhostPose &p, const GhostLook &look, float T, int shakeY) {
  const float levelScale[PET_MAX_LEVEL] = {0.82f, 0.9f, 0.96f, 1.0f, 1.04f};
  uint8_t level = look.level < 1 ? 1 : look.level > PET_MAX_LEVEL ? PET_MAX_LEVEL : look.level;
  float S = p.scale * levelScale[level - 1];
  Xf f;
  f.gx = 120 + p.x;
  f.gy = 113 + p.y;
  float spin = fabsf(p.spinX) < 0.08f ? 0.08f : fabsf(p.spinX);
  f.sx = S * (1 + p.squash) * spin;
  f.sy = S * (1 - p.squash);
  f.shear = tanf(p.tilt * 0.017453293f);
  f.fy = p.flip ? -1.0f : 1.0f;
  bool back = p.spinX < 0;
  uint8_t A = (uint8_t)clampf(p.alpha * 255, 0, 255);
  if (A < 3) {
    return;
  }
  Color eyeSpooky = EYE_COLORS[look.eyeColor % EYES_COUNT];
  Color body = level == PET_MAX_LEVEL ? mixColor(BODY, {255, 236, 170}, 0.25f) : BODY;

  // Halo
  float flicker = 1 - 0.25f * p.spooky * (sinf(T * 37) > 0.6f ? 1 : 0);
  Color halo = mixColor({140, 190, 255}, eyeSpooky, p.spooky);
  if (level == PET_MAX_LEVEL) halo = mixColor(halo, {255, 220, 120}, 0.5f);
  c.glow(f.X(0, 25), f.Y(25), 95 * f.sx + 10, 125 * f.sy, halo, (uint8_t)(120 * p.alpha * flicker));

  // Body: head (half circle) + tail rows, split exactly at the head centre row.
  const float R = 53;
  float tailLen = (level >= 3 ? 122.0f : 110.0f);
  int split = (int)floorf(f.gy + 0.5f);
  if (f.fy > 0) {
    c.setClip(0, 0, 240, split + shakeY);
  } else {
    c.setClip(0, split + shakeY, 240, 240);
  }
  c.ellipse(f.X(0, 0), f.Y(0), R * f.sx, R * f.sy, body, A);
  c.resetClip();
  float tailRows = tailLen * f.sy;
  for (int i = 0; i <= (int)tailRows + 1; i++) {
    int row = f.fy > 0 ? split + i : split - 1 - i;
    float ly = ((row + 0.5f) - f.gy) / (f.fy * f.sy);
    float v = ly / tailLen;
    if (v < 0 || v > 1) continue;
    float w = R * (1 - 0.92f * powf(v, 1.5f)) + 3 * sinf(v * 22 + T * 7) * v;
    float off = 25 * powf(v, 1.4f) * sinf(T * 3.2f - v * 4.5f);
    float xc = f.X(off, ly);
    float fade = fmaxf(0.05f, 1 - 0.95f * powf(v, 0.9f));
    c.span(row, xc - w * f.sx, xc + w * f.sx, body, (uint8_t)(A * fade));
  }

  // Ears and arms
  for (int sx = -1; sx <= 1; sx += 2) {
    localEllipse(c, f, sx * 41 + 2 * sinf(T * 5 + sx), -45.5f, 14.3f, 14.3f, INK, A);
  }
  for (int sx = -1; sx <= 1; sx += 2) {
    if (p.waveArm && sx == 1) {
      float a2 = sinf(T * 16) * 0.6f;
      localEllipse(c, f, 55 + 6 * sinf(a2), -8, 10.5f, 15.2f, INK, A, 20 * a2);
    } else {
      localEllipse(c, f, sx * 45.6f, 32 + 3.8f * sinf(T * 4 + sx), 9.5f, 13.3f, INK, A);
    }
  }

  // Accessories by level
  if (level == 2) {
    float bx = 50, by = -56;
    localTriangle(c, f, bx, by, bx - 11, by - 7, bx - 11, by + 7, {255, 120, 170}, A);
    localTriangle(c, f, bx, by, bx + 11, by - 7, bx + 11, by + 7, {255, 120, 170}, A);
    localEllipse(c, f, bx, by, 3.5f, 3.5f, {255, 80, 140}, A);
  } else if (level == 4) {
    localTriangle(c, f, -34, -48, 34, -48, 10, -108, {90, 50, 140}, A);
    localEllipse(c, f, 0, -48, 48, 7, {70, 38, 110}, A);
    localEllipse(c, f, 4, -60, 22, 4, {255, 200, 80}, A);
  } else if (level == 5) {
    Color gold = {250, 200, 60};
    float pts[][2] = {{-24, -48}, {-12, -48}, {0, -48}, {12, -48}, {24, -48}};
    for (int i = 0; i < 4; i += 1) {
      float x0 = pts[i][0], x1 = pts[i + 1][0];
      localTriangle(c, f, x0, -50, x1, -50, (x0 + x1) / 2, -72, gold, A);
    }
    c.rect(f.X(-24, -54), f.Y(f.fy > 0 ? -54 : -46), 48 * f.sx, 8 * f.sy, gold, A);
    localEllipse(c, f, 0, -60, 3, 3, {230, 50, 80}, A);
    localEllipse(c, f, -12, -58, 2.4f, 2.4f, {80, 160, 255}, A);
    localEllipse(c, f, 12, -58, 2.4f, 2.4f, {80, 200, 120}, A);
  }

  if (back) {
    return;  // spinning: we see the ghost from behind
  }

  // Face
  const float ey = -4.2f, ex = 23.3f;
  Color eyeC = mixColor(EYE_IDLE, eyeSpooky, p.spooky);
  for (int sx = -1; sx <= 1; sx += 2) {
    localEllipse(c, f, sx * ex, ey + 2.8f, 12.8f, 17.1f, INK, A, sx * 30.0f);
    float dl = 7 + 4 * sinf(T * 1.5f + sx);
    float dx = sx * ex * 1.08f, dy0 = ey + 11.4f;
    c.line(f.X(dx, dy0), f.Y(dy0), f.X(dx, dy0 + dl), f.Y(dy0 + dl), 3.8f * f.sx, INK, A);
    localEllipse(c, f, dx, dy0 + dl + 1, 3.0f, 3.0f, INK, A);
  }
  for (int sx = -1; sx <= 1; sx += 2) {
    float x = sx * ex + p.lookX * 4.8f, y = ey + 2.8f + p.lookY * 4.8f;
    switch (p.eyes) {
      case E_CLOSED:
        localArc(c, f, sx * ex, ey + 1, 8.5f, 6, 20, 160, 2.8f, {220, 230, 255}, A);
        break;
      case E_HAPPY:
        localArc(c, f, sx * ex, ey + 8, 8.5f, 7.5f, 200, 340, 2.8f, {220, 230, 255}, A);
        break;
      case E_SCARED:
        localEllipse(c, f, sx * ex, ey + 2.8f, 12.4f, 13.3f, {250, 250, 255}, A);
        localEllipse(c, f, sx * ex + p.lookX, ey + 2.8f, 2.9f, 2.9f, INK, A);
        break;
      case E_DIZZY: {
        float pts[36 * 2];
        for (int k = 0; k < 36; k++) {
          float an = k * 0.45f + T * 9, r = (1.5f + k * 0.3f) * 0.95f;
          float lx = sx * ex + r * cosf(an) * sx, ly = ey + 2.8f + r * sinf(an);
          pts[k * 2] = f.X(lx, ly);
          pts[k * 2 + 1] = f.Y(ly);
        }
        c.polyline(pts, 36, 2.1f * f.sx, eyeC, A);
        break;
      }
      case E_SAD:
        localEllipse(c, f, x, y + 2, 3.2f, 3.8f, eyeC, A);
        c.line(f.X(sx * ex - sx * 7, ey - 8), f.Y(ey - 8), f.X(sx * ex + sx * 5, ey - 12), f.Y(ey - 12), 2.2f * f.sx,
               {220, 230, 255}, A);
        break;
      default: {
        float rr = p.eyes == E_GLOW ? 5.2f + sinf(T * 20) : 3.8f;
        if (p.eyes == E_GLOW || p.spooky > 0.3f) {
          c.glow(f.X(x, y), f.Y(y), rr * 3.2f * f.sx, rr * 3.2f * f.sy, eyeC, (uint8_t)((60 + 150 * p.spooky) * p.alpha));
        }
        localEllipse(c, f, x, y, rr, rr * 1.25f, eyeC, A);
        localEllipse(c, f, x - rr * 0.35f, y - rr * 0.6f, rr * 0.3f, rr * 0.3f, WHITE, A);
      }
    }
  }
  // Nose and mouth
  localEllipse(c, f, 0, 15.2f, 5.7f, 3.3f, INK, A);
  const float my = 21.4f;
  bool fangs = level >= 3 || p.spooky > 0.5f;
  switch (p.mouth) {
    case M_SMIRK:
      localArc(c, f, -1, my - 1, 10.4f, 6.6f, 15, 150, 2.85f, INK, A);
      if (fangs) localTriangle(c, f, 2.8f, my + 4, 7.6f, my + 3, 4.8f, my + 9.5f, WHITE, A);
      break;
    case M_SMILE:
      localArc(c, f, 0, my - 1, 9, 6, 20, 160, 2.85f, INK, A);
      break;
    case M_GRIN:
      localChord(c, f, 0, my + 1, 15.2f, 10.5f, INK, A);
      for (int i = -1; i <= 1; i += 2) {
        localTriangle(c, f, i * 7.6f, my + 1.5f, i * 2.8f, my + 1.5f, i * 5.2f, my + 8.5f, WHITE, A);
      }
      break;
    case M_LAUGH:
      localChord(c, f, 0, my + 1, 12.4f, 9.5f, INK, A);
      localEllipse(c, f, 0, my + 7, 4.8f, 2.6f, {255, 120, 150}, A);
      if (fangs) localTriangle(c, f, -4.8f, my + 1.5f, 0, my + 1.5f, -2.4f, my + 7.5f, WHITE, A);
      break;
    case M_BOO:
      localEllipse(c, f, 0, my + 12, 16, 16, INK, A);
      localEllipse(c, f, 0, my + 21, 7.6f, 5.7f, {200, 40, 70}, A);
      for (int i = 0; i < 3; i++) {
        float fx = -11.4f + i * 7.6f;
        localTriangle(c, f, fx, my - 1, fx + 7.6f, my - 1, fx + 3.8f, my + 8.5f, WHITE, A);
      }
      for (int i = 0; i < 2; i++) {
        float fx = -7.6f + i * 7.6f;
        localTriangle(c, f, fx, my + 26.6f, fx + 7.6f, my + 26.6f, fx + 3.8f, my + 19, WHITE, A);
      }
      break;
    case M_O:
      localEllipse(c, f, 0, my + 5 * p.mouthOpen, 5.7f * (0.8f + 0.2f * p.mouthOpen), 6.2f * p.mouthOpen, INK, A);
      break;
    case M_WOBBLE: {
      float pts[21 * 2];
      for (int k = 0; k < 21; k++) {
        float lx = (k - 10) * 1.25f, ly = my + 4.8f + 2.4f * sinf(k * 1.3f + T * 12);
        pts[k * 2] = f.X(lx, ly);
        pts[k * 2 + 1] = f.Y(ly);
      }
      c.polyline(pts, 21, 2.85f * f.sx, INK, A);
      break;
    }
    case M_SLEEP:
      localEllipse(c, f, 0, my + 4.8f, 3.8f, 2.85f, INK, A);
      break;
    case M_FROWN:
      localArc(c, f, 0, my + 7, 8, 5, 200, 340, 2.8f, INK, A);
      break;
    case M_CHEW:
      localEllipse(c, f, 0, my + 5, 7, 6, INK, A);
      break;
  }
  for (int sx = -1; sx <= 1; sx += 2) {
    float blushA = clampf(170 * (1 - 0.7f * p.spooky) * p.blush * p.alpha, 0, 255);
    localEllipse(c, f, sx * 40.8f, 16.2f, 6.6f * (p.blush > 1 ? 1.3f : 1), 3.3f, BLUSH, (uint8_t)blushA);
  }

  // Food held at the mouth
  if (p.foodItem >= 0) {
    float s = 15 * S * (0.4f + 0.6f * p.foodLeft);
    drawFood(c, p.foodItem, f.X(0, my + 18), f.Y(my + 18) - p.foodDrop * 70, s, A);
  }
}

static void drawEffects(Canvas &c, const GhostPose &p, float T, Food food) {
  float gx = 120 + p.x, gy = 113 + p.y;
  if (p.hearts > 0) {
    for (int i = 0; i < 4; i++) {
      float ph = p.hearts * 1.25f - i * 0.22f;
      if (ph <= 0 || ph >= 1) continue;
      float x = gx + (i - 1.5f) * 24 + 6 * sinf(ph * 6 + i), y = gy - 40 - ph * 75;
      c.heart(x, y, 6 + 4 * ph, {255, 90, 140}, (uint8_t)(255 * (1 - ph)));
    }
  }
  if (p.sparkles > 0) {
    for (int i = 0; i < 7; i++) {
      float an = T * 1.4f + i * 0.9f;
      float x = gx + 78 * cosf(an), y = gy + 20 + 85 * sinf(an) * 0.8f;
      float tw = fabsf(sinf(T * 6 + i * 1.7f));
      c.star(x, y, 4 + 4 * tw, {255, 240, 150}, (uint8_t)clampf(255 * tw * p.sparkles, 0, 255));
    }
  }
  if (p.zzz > 0.05f) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(T * 0.5f + i * 0.33f, 1);
      drawZ(c, gx - 40 - ph * 24, gy - 58 - ph * 44, 4 + ph * 5, {220, 230, 255},
            (uint8_t)(255 * p.zzz * (1 - ph)));
    }
  }
  if (p.bubble) {
    c.circle(gx + 46, gy - 60, 3.5f, WHITE, 230);
    c.circle(gx + 56, gy - 74, 5.5f, WHITE, 230);
    c.circle(gx + 76, gy - 100, 21, WHITE, 235);
    drawFood(c, food, gx + 76, gy - 100, 10, 255);
  }
  if (p.ball) {
    float bx = gx + p.ballX, by = gy + p.ballY;
    c.circle(bx, by, 10, {245, 140, 40});
    c.arc(bx, by, 5, 10, 270, 450, 1.8f, {200, 100, 20});
    c.rect(bx - 1.2f, by - 14, 3, 5, {70, 120, 40});
  }
}

void Ghost::render(Canvas &c, const GhostLook &look) {
  GhostPose p;
  computePose(p, look);
  int shakeX = (int)(p.shake * 7 * sinf(time * 71)), shakeY = (int)(p.shake * 5 * cosf(time * 53));
  c.setOffset(shakeX, shakeY);
  Color eye = EYE_COLORS[look.eyeColor % EYES_COUNT];
  drawScene(c, look.scene, time, p.spooky, eye);
  drawFog(c, time, p.fog, p.spooky, eye);
  drawGhost(c, p, look, time, shakeY);
  drawEffects(c, p, time, needHungry ? FOOD_BAMBOO : food);
  c.setOffset(0, 0);
  if (p.flash > 0.01f) {
    c.overlay(p.flashColor, (uint8_t)clampf(p.flash * 50, 0, 255));
  }
}
