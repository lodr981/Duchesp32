#pragma once
// "Trapped souls" director: plays the original photographic frames as clips
// (calm, banging, knocking, approaching...) and reacts to the motion sensor
// with glass cracks, hand smudges, camera shake, glitches and escapes.
// Platform independent (see tools/preview).
#include "gfx.h"
#include "ghost.h"

enum SoulEvent : uint8_t { SOUL_SHAKE, SOUL_STRONG_SHAKE, SOUL_KNOCK, SOUL_WAKE, SOUL_VANISH };

class SoulDirector {
 public:
  SoulDirector();
  // id = animation id ("soul_m", "soul_f", ...), frameCount of that animation.
  void setSoul(const char *id, uint16_t frameCount);
  void event(SoulEvent e);
  void setSleepy(bool sleepy);
  bool sleepy() const { return isSleepy; }

  // baseDelay = seconds per frame from the speed setting.
  void update(float dt, float baseDelay, const GhostInput &in, const GhostLook &look);
  uint16_t frame() const { return cursor; }

  // src: decoded frame, RGB565 big-endian. dst: native RGB565 for the canvas.
  // src may equal dst (in place): then shifting/shaking the photo is skipped.
  void render(const uint8_t *src, Canvas &dst, const GhostLook &look);

  struct Clip {
    uint8_t from, to;
    float delay;  // multiplier of the base delay
    bool pingpong;
  };
  struct Impact {
    uint8_t frame;
    int16_t x, y;
  };
  struct Script {
    const char *id;
    Clip idle, bang, knock, calm, approach;
    bool hasApproach;
    Impact impacts[8];
    uint8_t impactCount;
  };

 private:
  struct Crack {
    float x0, y0, x1, y1, width, born;
  };
  struct Smudge {
    float x, y, born;
  };

  enum Mode : uint8_t { M_IDLE, M_APPROACH, M_BANG, M_KNOCK, M_ESCAPE, M_RETURN, M_SLEEP };

  const Script *script;
  Script generic;  // for animations without a hand-made script
  uint16_t frameCount = 1;
  uint16_t cursor = 0;
  int8_t direction = 1;
  float frameTimer = 0;
  Mode mode = M_IDLE;
  Clip clip;
  int cyclesLeft = 0;
  float modeTime = 0;
  float time = 0;
  float nextEvent = 15;
  bool isSleepy = false;
  uint32_t rng = 777;

  // effects
  float shake = 0;        // camera shake strength, decays
  float glitch = 0;       // 0..1
  float fade = 0;         // 0 = soul visible, 1 = gone
  float flash = 0;
  float tintPulse = 0;
  float flickerUntil = 0;
  float lean = 0;
  bool flip = false;
  uint8_t spooky = 1;
  int lastImpactFrame = -1;

  static const int MAX_CRACKS = 400;
  Crack cracks[MAX_CRACKS];
  int crackCount = 0;
  static const int MAX_SMUDGES = 8;
  Smudge smudges[MAX_SMUDGES];
  int smudgeCount = 0;

  float random01();
  void play(Mode m, const Clip &c, int cycles);
  void advance();
  void onFrameShown();
  void addCrackBurst(float x, float y, int rays, float maxLen);
  void addSmudge(float x, float y);
  void pixelPass(const uint8_t *src, uint16_t *dst, bool inPlace, Color tint, float tintAmount);
};
