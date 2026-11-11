#pragma once
// Tilt maze: roll the marble to the glowing portal. 15 levels that grow, get holes
// (level 8+) and go dark (level 11+). Platform independent (see tools/preview).
#include "ghost.h"
#include "gfx.h"

#define MAZE_LEVELS 15
#define MAZE_MAX_N 11

enum MazeEvent : uint8_t { MAZE_EV_NONE, MAZE_EV_LEVEL_DONE, MAZE_EV_ALL_DONE, MAZE_EV_CALIBRATED };

struct MazeMapping {
  uint8_t axisX = 0, axisY = 1;  // which accelerometer axis moves the marble right / down
  int8_t signX = 1, signY = 1;
  bool calibrated = false;
};

class MazeGame {
 public:
  // Starts a level (1..MAZE_LEVELS). Without a calibrated mapping it asks for one first.
  void start(int level, const MazeMapping &mapping);
  void recalibrate();
  bool active() const { return state != S_OFF; }
  bool playing() const { return state == S_PLAY || state == S_FALL; }
  void stop() { state = S_OFF; }
  int level() const { return currentLevel; }

  // gx, gy, gz: filtered gravity from the accelerometer (g).
  MazeEvent update(float dt, float gx, float gy, float gz, Ghost &ghost, const GhostLook &look);
  void render(Canvas &c, Ghost &ghost, const GhostLook &look);
  // Optional 240x240 buffer for the static maze (walls are drawn once per level).
  void setCache(uint16_t *buffer) { cache = buffer; cacheValid = false; }

  const MazeMapping &mapping() const { return map; }
  uint8_t lastStars() const { return stars; }
  float lastTime() const { return playTime; }

  // For the desktop autopilot: next waypoint of the solution from the marble.
  bool hint(float &x, float &y) const;
  void marble(float &x, float &y) const { x = bx, y = by; }

 private:
  enum State : uint8_t {
    S_OFF,
    S_CAL_FLAT,
    S_CAL_RIGHT,
    S_CAL_DOWN,
    S_COUNTDOWN,
    S_PLAY,
    S_FALL,
    S_WIN,
    S_ALL_DONE,
  };
  struct Particle {
    float x, y, vx, vy, life;
    Color c;
  };

  State state = S_OFF;
  int currentLevel = 1;
  MazeMapping map;
  float stateTime = 0, playTime = 0, time = 0;
  uint8_t stars = 0;

  // maze
  int n = 5;
  float cell = 30, ox = 37, oy = 37;
  uint8_t walls[MAZE_MAX_N][MAZE_MAX_N];  // bit0 right wall, bit1 bottom wall
  uint8_t dist[MAZE_MAX_N][MAZE_MAX_N];   // steps from the exit (for hints)
  int exitX = 0, exitY = 0, pathLength = 0;
  int holeCount = 0;
  int8_t holes[10][2];
  float lightRadius = 0;  // 0 = no darkness

  // marble
  float bx = 0, by = 0, vx = 0, vy = 0, radius = 6;
  float physicsTime = 0;  // fixed-step accumulator
  uint16_t *cache = nullptr;
  bool cacheValid = false;
  void drawStatic(Canvas &c);
  float neutral[3] = {0, 0, 1};
  float calSum[3] = {0, 0, 0};
  int calSamples = 0;

  Particle parts[90];
  int partCount = 0;
  uint32_t rng = 1;

  float random01();
  void generate(int level);
  void resetMarble();
  void collide();
  void burst(float x, float y, int count, float speed);
  float cellCenterX(int cx) const { return ox + (cx + 0.5f) * cell; }
  float cellCenterY(int cy) const { return oy + (cy + 0.5f) * cell; }
  void drawMaze(Canvas &c);
  void drawParticles(Canvas &c);
};
