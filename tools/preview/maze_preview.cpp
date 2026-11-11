// Desktop test of the tilt maze: an autopilot "tilts" the pendant towards the
// next waypoint, through the calibration and the levels.
// Build: g++ -O2 -std=c++17 -I../../SoulCage maze_preview.cpp ../../SoulCage/maze.cpp ../../SoulCage/gfx.cpp
//        ../../SoulCage/gfx_text.cpp ../../SoulCage/ghost.cpp ../../SoulCage/pet.cpp -o maze_preview
// Run:   ./maze_preview out_dir [first level] [last level] [frame step]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "maze.h"

static void writePpm(const std::string &path, const uint16_t *px) {
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 240 240 255\n");
  for (int i = 0; i < 240 * 240; i++) {
    uint16_t v = px[i];
    uint8_t rgb[3] = {(uint8_t)(((v >> 11) & 31) * 255 / 31), (uint8_t)(((v >> 5) & 63) * 255 / 63),
                      (uint8_t)((v & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  int first = argc > 2 ? atoi(argv[2]) : 1, last = argc > 3 ? atoi(argv[3]) : 15;
  int step = argc > 4 ? atoi(argv[4]) : 1;
  std::vector<uint16_t> buf(240 * 240);
  Canvas canvas(buf.data(), 240, 240);
  Ghost ghost;
  ghost.setSpontaneous(false);
  GhostLook look;
  look.spooky = 0;
  MazeGame game;
  std::vector<uint16_t> cache(240 * 240);
  game.setCache(cache.data());
  MazeMapping uncalibrated;  // forces the calibration screens first
  game.start(first, uncalibrated);
  // Physical mounting of the sensor: screen right = -gy, screen down = +gx.
  auto physical = [](float tx, float ty, float g[3]) {
    g[0] = ty, g[1] = -tx, g[2] = 1;
  };
  const float dt = 1 / 25.0f;
  float px = 0, py = 0, t = 0, stateT = 0;
  int frame = 0, written = 0, done = 0;
  double worst = 0;
  while (game.active() && t < 1500) {
    float tx = 0, ty = 0;
    // calibration answers
    if (t > 3 && t < 4.5f) tx = 0.6f;
    else if (t > 5.2f && t < 6.7f) ty = 0.6f;
    float bx, by, wx, wy;
    game.marble(bx, by);
    game.hint(wx, wy);
    float vx = (bx - px) / dt, vy = (by - py) / dt;
    px = bx, py = by;
    if (t > 7 && game.playing()) {
      tx = (wx - bx) * 0.03f - vx * 0.004f;
      ty = (wy - by) * 0.03f - vy * 0.004f;
    }
    float g[3];
    physical(tx, ty, g);
    MazeEvent e = game.update(dt, g[0], g[1], g[2], ghost, look);
    if (e == MAZE_EV_CALIBRATED) {
      const MazeMapping &m = game.mapping();
      printf("calibrated: axisX=%d signX=%d axisY=%d signY=%d\n", m.axisX, m.signX, m.axisY, m.signY);
    }
    if (e == MAZE_EV_LEVEL_DONE || e == MAZE_EV_ALL_DONE) {
      printf("level %2d done in %5.1f s, %d stars\n", game.level(), game.lastTime(), game.lastStars());
      worst = game.lastTime() > worst ? game.lastTime() : worst;
      done++;
      if (game.level() >= last && e != MAZE_EV_ALL_DONE) {
        stateT = t;
      }
    }
    if (stateT && t - stateT > 3) break;
    if (frame % step == 0) {
      game.render(canvas, ghost, look);
      char name[64];
      snprintf(name, sizeof(name), "/m%05d.ppm", written++);
      writePpm(out + name, buf.data());
    }
    frame++;
    t += dt;
  }
  printf("levels done: %d, frames: %d\n", done, written);
  return done ? 0 : 1;
}
