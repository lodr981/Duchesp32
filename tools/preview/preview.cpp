// Desktop preview of the ghost renderer: renders a scripted session to PPM frames.
// Build: g++ -O2 -std=c++17 -I../../SoulCage preview.cpp ../../SoulCage/gfx.cpp
//        ../../SoulCage/ghost.cpp ../../SoulCage/pet.cpp -o preview
// Run:   ./preview out_dir [scene] [level] [spooky] [eyeColor]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "ghost.h"

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

struct Cue {
  float at;
  GhostAction action;
  Food food;
  int special;  // 1 sleep, 2 wake, 3 upside down on, 4 off, 5 lean, 6 hungry
};

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  GhostLook look;
  look.scene = argc > 2 ? atoi(argv[2]) : SCENE_GRAVEYARD;
  look.level = argc > 3 ? atoi(argv[3]) : 3;
  look.spooky = argc > 4 ? atoi(argv[4]) : 1;
  look.eyeColor = argc > 5 ? atoi(argv[5]) : EYES_GREEN;
  float seconds = argc > 6 ? atof(argv[6]) : 30;

  std::vector<Cue> cues = {
      {1.0f, GA_WAVE, FOOD_BAMBOO, 0},   {3.5f, GA_BOO, FOOD_BAMBOO, 0},     {6.5f, GA_SCARED, FOOD_BAMBOO, 0},
      {7.8f, GA_DIZZY, FOOD_BAMBOO, 0},  {10.6f, GA_EAT, FOOD_COOKIE, 0},    {14.0f, GA_PLAY, FOOD_BAMBOO, 0},
      {18.2f, GA_LOVE, FOOD_BAMBOO, 0},  {21.0f, GA_SPIN, FOOD_BAMBOO, 0},   {23.0f, GA_HIDE, FOOD_BAMBOO, 0},
      {26.2f, GA_NONE, FOOD_BAMBOO, 1},  {29.0f, GA_NONE, FOOD_BAMBOO, 2},
  };
  Ghost ghost;
  ghost.setSpontaneous(false);
  std::vector<uint16_t> buf(240 * 240);
  Canvas canvas(buf.data(), 240, 240);
  GhostInput in;
  const float fps = 20, dt = 1 / fps;
  size_t next = 0;
  double totalMs = 0;
  int frames = (int)(seconds * fps);
  for (int i = 0; i < frames; i++) {
    float t = i * dt;
    while (next < cues.size() && cues[next].at <= t) {
      const Cue &c = cues[next++];
      if (c.special == 1) ghost.setSleeping(true);
      else if (c.special == 2) ghost.setSleeping(false);
      else ghost.trigger(c.action, c.food);
    }
    ghost.update(dt, in, look);
    auto t0 = std::chrono::steady_clock::now();
    ghost.render(canvas, look);
    totalMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    char name[64];
    snprintf(name, sizeof(name), "/f%04d.ppm", i);
    writePpm(out + name, buf.data());
  }
  printf("frames %d, avg render %.2f ms (desktop)\n", frames, totalMs / frames);
  return 0;
}
