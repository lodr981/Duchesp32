#pragma once
// "Lovec světýlek": tilt to steer a lantern around the glass and catch the
// soul's escaped lights. Gold lights are worth more, red spirits cost a life,
// a tap flashes the lantern and scares them away. Endless waves, high score.
// Platform independent (see tools/preview).
#include "gfx.h"
#include "maze.h"  // MazeMapping: the same tilt calibration

enum WispEvent : uint8_t { WISP_EV_NONE, WISP_EV_OVER };

class WispGame {
 public:
  void start(const MazeMapping &mapping, uint32_t best, uint32_t jar);
  bool active() const { return state != S_OFF; }
  bool playing() const { return state == S_PLAY; }
  void stop() { state = S_OFF; }

  // ax, ay, az: lightly filtered acceleration (g); tapped: a tap since the last update.
  WispEvent update(float dt, float ax, float ay, float az, bool tapped);
  void render(Canvas &c);
  // Optional 240x240 buffer for the static background (drawn once).
  void setCache(uint16_t *buffer) { cache = buffer; cacheValid = false; }

  uint32_t score() const { return points; }
  uint32_t caught() const { return lights; }  // lights caught this round (go to the jar)
  bool record() const { return newRecord; }

  // Desktop autopilot helpers
  void lantern(float &x, float &y) const { x = px, y = py; }
  bool nearestLight(float &x, float &y) const;
  bool nearestSpirit(float &x, float &y) const;

 private:
  enum State : uint8_t { S_OFF, S_COUNTDOWN, S_PLAY, S_OVER };
  enum Kind : uint8_t { K_BLUE, K_GOLD, K_HEART };
  struct Light {
    float x, y, vx, vy, age, life, phase;
    Kind kind;
  };
  struct Spirit {
    float x, y, vx, vy, phase, stun;
  };
  struct Spark {
    float x, y, vx, vy, life;
    Color c;
  };
  struct Popup {
    float x, y, age;
    char text[8];
  };

  State state = S_OFF;
  MazeMapping map;
  float neutral[3] = {0, 0, 1}, calSum[3] = {0, 0, 0};
  int calSamples = 0;
  float time = 0, stateTime = 0, physicsTime = 0;

  float px = 120, py = 120, vx = 0, vy = 0;
  float trail[10][2];
  int trailHead = 0;
  int lives = 3;
  float invulnerable = 0, flash = 0, charge = 1, shake = 0, hurt = 0;
  uint32_t points = 0, lights = 0, best = 0, jar = 0;
  bool newRecord = false;
  int combo = 0;
  float lastCatch = -10;
  int wave = 1;
  float waveBanner = 0, nextLight = 1, nextGold = 18, nextHeart = 40;

  Light lightsArr[14];
  int lightCount = 0;
  Spirit spirits[8];
  int spiritCount = 0;
  Spark sparks[80];
  int sparkCount = 0;
  Popup popups[6];
  int popupCount = 0;
  uint32_t rng = 4242;
  uint16_t *cache = nullptr;
  bool cacheValid = false;
  void drawBackground(Canvas &c);

  float random01();
  void spawnLight(Kind k);
  void spawnSpirit();
  void burst(float x, float y, int n, float speed, Color c);
  void popup(float x, float y, const char *text);
  void step(float h, float tx, float ty);
};
