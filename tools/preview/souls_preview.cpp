// Desktop preview of the trapped-souls mode: decodes the real frames with zlib
// and runs the SoulDirector through a scripted session.
// Build: g++ -O2 -std=c++17 -Ishim -I../../SoulCage souls_preview.cpp ../../SoulCage/gfx.cpp
//        ../../SoulCage/soul.cpp ../../SoulCage/ghost.cpp ../../SoulCage/pet.cpp -lz -o souls_preview
// Run:   ./souls_preview out_dir [soul 0|1] [spooky] [eyeColor] [seconds] [hunt]
//        with "hunt": taps, six QR seals and the freed soul instead of the motion script
#include <zlib.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "anim_data.h"
#include "hunt.h"
#include "soul.h"

static bool decode(const Animation &a, uint16_t frame, uint8_t *out) {
  uLongf len = 240 * 240 * 2;
  return uncompress(out, &len, a.data + a.offsets[frame], a.offsets[frame + 1] - a.offsets[frame]) == Z_OK;
}

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
  int soulIndex = argc > 2 ? atoi(argv[2]) : 0;
  GhostLook look;
  look.spooky = argc > 3 ? atoi(argv[3]) : 1;
  look.eyeColor = argc > 4 ? atoi(argv[4]) : 0;
  float seconds = argc > 5 ? atof(argv[5]) : 32;

  const Animation *anim = nullptr;
  int n = 0;
  for (const Animation &a : kAnimations) {
    if (a.soul && n++ == soulIndex) anim = &a;
  }
  if (!anim) return 1;
  SoulDirector director;
  director.setSoul(anim->id, anim->frameCount);
  std::vector<uint8_t> src(240 * 240 * 2);
  std::vector<uint16_t> dst(240 * 240);
  Canvas canvas(dst.data(), 240, 240);
  GhostInput in;
  bool hunt = argc > 6 && std::string(argv[6]) == "hunt";
  HuntData hd = {};
  hd.active = hunt;
  hd.count = 6;
  HuntOverlay overlay;
  const float fps = 25, dt = 1 / fps;
  for (int i = 0; i < (int)(seconds * fps); i++) {
    float t = i * dt;
    if (hunt) {
      if (i == (int)(2 * fps)) director.event(SOUL_TAP);
      if (i == (int)(4 * fps)) director.event(SOUL_DOUBLE_TAP);
      for (int k = 0; k < 6; k++) {
        if (i == (int)((7 + k * 3.5f) * fps)) {
          hd.found |= 1 << k;
          if (k < 5) {
            director.event(SOUL_SEAL);
            overlay.sealBroken(k);
          } else {
            hd.active = 0;
            director.event(SOUL_FREE);
            overlay.freed();
          }
        }
      }
      director.update(dt, 0.1f, in, look);
      overlay.update(dt);
      decode(*anim, director.frame(), src.data());
      director.render(src.data(), canvas, look);
      overlay.render(canvas, hd, "Děkuju, Amelko!");
      char name[64];
      snprintf(name, sizeof(name), "/f%04d.ppm", i);
      writePpm(out + name, dst.data());
      continue;
    }
    // scripted session
    if (i == (int)(3 * fps)) director.event(SOUL_SHAKE);
    if (i == (int)(8 * fps)) director.event(SOUL_KNOCK);
    if (i == (int)(11 * fps)) director.event(SOUL_STRONG_SHAKE);
    in.upsideDown = t > 17 && t < 19.5f;
    in.lean = t > 20 && t < 23.5f ? sinf((t - 20) * 1.8f) : 0;
    if (i == (int)(24 * fps)) director.setSleepy(true);
    if (i == (int)(29 * fps)) director.event(SOUL_WAKE);
    director.update(dt, 0.1f, in, look);
    decode(*anim, director.frame(), src.data());
    director.render(src.data(), canvas, look);
    char name[64];
    snprintf(name, sizeof(name), "/f%04d.ppm", i);
    writePpm(out + name, dst.data());
  }
  return 0;
}
