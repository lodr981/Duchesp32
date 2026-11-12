// Desktop preview of the weather and aircraft radar screens with made-up data
// around Jenišovice (rain cells drifting east, a few aircraft).
// Build: g++ -O2 -std=c++17 -I../../SoulCage radar_preview.cpp ../../SoulCage/radarview.cpp
//        ../../SoulCage/gfx.cpp ../../SoulCage/gfx_text.cpp -o radar_preview
// Run:   ./radar_preview out_dir
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "radarview.h"

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

static uint16_t rgb(int r, int g, int b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  GeoView view;  // Jenišovice, 50 km
  // CHMU-like palette, light to heavy
  const uint16_t pal[] = {rgb(0, 163, 224), rgb(0, 85, 136), rgb(0, 190, 0), rgb(255, 170, 0), rgb(255, 68, 0), rgb(193, 0, 0)};
  std::vector<std::vector<uint16_t>> frames(RADAR_FRAMES, std::vector<uint16_t>(RADAR_PX * RADAR_PX));
  WeatherFrame wf[RADAR_FRAMES];
  for (int f = 0; f < RADAR_FRAMES; f++) {
    for (int y = 0; y < RADAR_PX; y++) {
      for (int x = 0; x < RADAR_PX; x++) {
        float lat, lon;
        view.unproject((x + 0.5f) * 240 / RADAR_PX, (y + 0.5f) * 240 / RADAR_PX, lat, lon);
        // two cells moving east-north-east towards home
        float v = 0;
        float c1x = 14.55f + f * 0.06f, c1y = 50.52f + f * 0.012f;
        float c2x = 15.45f + f * 0.03f, c2y = 50.85f;
        v += 6 * expf(-((lon - c1x) * (lon - c1x) / 0.02f + (lat - c1y) * (lat - c1y) / 0.004f));
        v += 3.5f * expf(-((lon - c2x) * (lon - c2x) / 0.05f + (lat - c2y) * (lat - c2y) / 0.006f));
        v += 0.6f * sinf(lon * 37 + lat * 23) * (v > 0.6f);
        int idx = (int)v;
        frames[f][y * RADAR_PX + x] = v < 0.7f ? 0 : pal[idx > 5 ? 5 : idx];
      }
    }
    wf[f].pixels = frames[f].data();
    wf[f].minutesAgo = (RADAR_FRAMES - 1 - f) * 5;
    snprintf(wf[f].time, sizeof(wf[f].time), "14:%02d", 10 + f * 5);
  }
  Plane planes[] = {
      {50.70f, 15.30f, 250, 10900, 840, "CSA4JK", true}, {50.55f, 14.90f, 75, 3100, 420, "OKHLD", true},
      {50.80f, 14.95f, 130, 11800, 870, "RYR8PL", true}, {50.45f, 15.40f, 310, 7200, 700, "WZZ1HU", true},
      {50.64f, 15.18f, 0, 600, 150, "OK-TUR", false},     {50.95f, 15.60f, 200, 9800, 820, "DLH6TA", true},
  };
  std::vector<uint16_t> buf(240 * 240);
  Canvas canvas(buf.data(), 240, 240);
  RadarScreens screens;
  int n = 0;
  for (int i = 0; i < 160; i++) {
    float t = i * 0.05f;
    screens.drawWeather(canvas, view, wf, RADAR_FRAMES, "Jenišovice", "", t);
    char name[64];
    snprintf(name, sizeof(name), "/w%04d.ppm", n++);
    writePpm(out + name, buf.data());
  }
  for (int i = 0; i < 100; i++) {
    float t = i * 0.05f;
    for (Plane &p : planes) {  // move along the track
      p.lat += cosf(p.track * 0.0174533f) * p.speedKmh / 3600 * 0.05f * 4 / 111.32f;
      p.lon += sinf(p.track * 0.0174533f) * p.speedKmh / 3600 * 0.05f * 4 / (111.32f * 0.635f);
    }
    screens.drawPlanes(canvas, view, planes, 6, "Jenišovice", "", t);
    char name[64];
    snprintf(name, sizeof(name), "/p%04d.ppm", i);
    writePpm(out + name, buf.data());
  }
  WeatherFrame none[1];
  (void)none;
  screens.drawWeather(canvas, view, nullptr, 0, "Jenišovice", "Bez internetu", 0);
  writePpm(out + "/nonet.ppm", buf.data());
  printf("ok\n");
  return 0;
}
