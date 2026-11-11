// Desktop test of "Lovec světýlek": an autopilot steers to the lights, dodges the
// red spirits and flashes when one gets close.
// Build: g++ -O2 -std=c++17 -I../../SoulCage wisp_preview.cpp ../../SoulCage/wisp.cpp ../../SoulCage/gfx.cpp
//        ../../SoulCage/gfx_text.cpp -o wisp_preview
// Run:   ./wisp_preview out_dir [frame step] [seconds]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "wisp.h"

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
  int step = argc > 2 ? atoi(argv[2]) : 1;
  float seconds = argc > 3 ? atof(argv[3]) : 200;
  std::vector<uint16_t> buf(240 * 240);
  Canvas canvas(buf.data(), 240, 240);
  MazeMapping m;
  m.calibrated = true;  // identity mapping: x -> right, y -> down
  WispGame game;
  std::vector<uint16_t> cache(240 * 240);
  game.setCache(cache.data());
  game.start(m, 40, 85);
  const float dt = 1 / 30.0f;
  float lx = 120, ly = 140;
  int frame = 0, written = 0;
  for (float t = 0; t < seconds && game.active(); t += dt, frame++) {
    float px, py, tx = 0, ty = 0;
    game.lantern(px, py);
    float vx = (px - lx) / dt, vy = (py - ly) / dt;
    lx = px, ly = py;
    bool tap = false;
    if (game.playing()) {
      float x, y;
      if (game.nearestLight(x, y)) tx = (x - px) * 0.02f, ty = (y - py) * 0.02f;
      if (game.nearestSpirit(x, y)) {
        float d = hypotf(x - px, y - py) + 1;
        if (d < 60) tx -= (x - px) / d * 0.5f, ty -= (y - py) / d * 0.5f;
        tap = d < 40;
      }
      tx -= vx * 0.002f, ty -= vy * 0.002f;
    }
    if (game.update(dt, tx, ty, 1, tap) == WISP_EV_OVER) {
      printf("game over after %.0f s: score %u, lights %u, record %d\n", t, game.score(), game.caught(), game.record());
    }
    if (frame % step == 0) {
      game.render(canvas);
      char name[64];
      snprintf(name, sizeof(name), "/w%05d.ppm", written++);
      writePpm(out + name, buf.data());
    }
  }
  printf("frames %d\n", written);
  return 0;
}
