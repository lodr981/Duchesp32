#pragma once
// "Free the soul" treasure hunt: printed QR seals are hidden around the house,
// scanning one with the phone breaks a seal on the pendant; the last one frees
// the soul. Drawing and game data are platform independent.
#include <stdint.h>
#include "gfx.h"

#define HUNT_MAX_SEALS 10
#define HUNT_CODE_LEN 5

struct HuntData {
  uint8_t version;
  uint8_t active;
  uint8_t count;   // seals in this game
  uint16_t found;  // bit per seal
  uint16_t freed;  // souls freed so far (all games)
  char codes[HUNT_MAX_SEALS][HUNT_CODE_LEN + 1];
};

#define HUNT_DATA_VERSION 1

int huntFoundCount(const HuntData &d);
int huntSealForCode(const HuntData &d, const char *code);  // -1 = unknown

// Seals around the display edge, the celebration and messages.
class HuntOverlay {
 public:
  void sealBroken(int index);  // flash + "Pečeť X/N"
  void freed();                // fireworks and thank you
  bool celebrating() const { return freedTime >= 0; }
  void update(float dt);
  // greeting = e.g. "Děkuju, Amelko!"
  void render(Canvas &c, const HuntData &d, const char *thanks);

 private:
  struct Spark {
    float x, y, vx, vy, life;
    Color c;
  };
  float time = 0;
  int flashSeal = -1;
  float flashTime = 99;
  float freedTime = -1;
  Spark sparks[100];
  int sparkCount = 0;
  uint32_t rng = 99;
  float random01();
  void burst(float x, float y, int n, float speed);
};
