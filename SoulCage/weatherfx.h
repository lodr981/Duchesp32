#pragma once
// Weather outside drawn on the glass over the soul / ghost: rain running down,
// snow, a thunderstorm with flashes, fog and frost on the rim.
// Platform independent (see tools/preview/weatherfx_preview.cpp).
#include <stdint.h>
#include "gfx.h"

enum WeatherFxKind : uint8_t { FX_NONE, FX_RAIN, FX_HEAVY_RAIN, FX_SNOW, FX_STORM, FX_FOG };

// From the current WMO weather code, temperature and the radar at home (level 0..4, -1 unknown).
WeatherFxKind weatherFxKind(int code, int radarLevelAtHome);
const char *weatherFxName(WeatherFxKind k);  // "déšť", "sníh"... for the app

class WeatherFx {
 public:
  void set(WeatherFxKind kind, bool frost);
  void update(float dt);
  void render(Canvas &c);
  WeatherFxKind kind() const { return kind_; }
  bool frost() const { return frost_; }

 private:
  static const int MAX_DROPS = 46;
  struct Drop {
    float x, y, v, len, phase;
  };
  Drop drops[MAX_DROPS];
  int dropCount = 0;
  struct Bead {  // water drops sliding down the glass
    float x, y, r, v, wait;
  };
  Bead beads[8];
  WeatherFxKind kind_ = FX_NONE;
  bool frost_ = false;
  float strength = 0;  // fades in and out
  float clock = 0, flash = 0, nextFlash = 4, boltX = 120;
  uint32_t rnd = 0x1234567;
  float rand01();
  void respawn(Drop &d, bool top);
};
