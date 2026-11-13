// Desktop preview of the weather on the glass (rain, heavy rain, snow, storm, fog, frost)
// over a stand-in soul. Build: see .github/workflows/build.yml. Run: ./weatherfx_preview out_dir
#include <cstdio>
#include <string>
#include <vector>
#include "weatherfx.h"

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
  std::vector<uint16_t> buf(240 * 240);
  Canvas c(buf.data(), 240, 240);
  struct {
    WeatherFxKind kind;
    bool frost;
    const char *name;
  } cases[] = {{FX_RAIN, false, "rain"},   {FX_HEAVY_RAIN, false, "heavy"}, {FX_SNOW, true, "snow"},
               {FX_STORM, false, "storm"}, {FX_FOG, false, "fog"},          {FX_NONE, true, "frost"}};
  if (weatherFxKind(95, -1) != FX_STORM || weatherFxKind(73, -1) != FX_SNOW || weatherFxKind(2, 2) != FX_RAIN ||
      weatherFxKind(1, -1) != FX_NONE) {
    printf("weatherFxKind mapping broken\n");
    return 1;
  }
  for (auto &k : cases) {
    WeatherFx fx;
    fx.set(k.kind, k.frost);
    for (int i = 0; i < 120; i++) {
      float t = i * 0.04f;
      c.gradient({30, 20, 50}, {8, 6, 16});
      c.glow(120, 120, 70, 80, {120, 200, 255}, 90);  // stand-in soul
      c.circle(108, 112, 6, {255, 255, 255});
      c.circle(132, 112, 6, {255, 255, 255});
      fx.update(0.04f);
      fx.render(c);
      (void)t;
      if (i % 30 == 29 || (k.kind == FX_STORM && i == 31)) {
        char name[96];
        snprintf(name, sizeof(name), i == 31 ? "/%s_flash.ppm" : "/%s_%d.ppm", k.name, i / 30);
        writePpm(out + name, buf.data());
      }
    }
  }
  printf("ok\n");
  return 0;
}
