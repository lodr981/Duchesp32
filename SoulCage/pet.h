#pragma once
// Tamagotchi-style care: needs drop over time, care raises them and earns XP.
// Kid friendly: the ghost never dies, low needs only make it sad, hungry or sleepy.
#include <stdint.h>

enum Food : uint8_t { FOOD_BAMBOO, FOOD_COOKIE, FOOD_PIE, FOOD_CANDY, FOOD_COUNT };

enum PetResult : uint8_t { PET_OK, PET_REFUSED, PET_LEVEL_UP };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_OK, MOOD_HUNGRY, MOOD_BORED, MOOD_TIRED, MOOD_SLEEPING };

#define PET_MAX_LEVEL 5
#define PET_DATA_VERSION 1

struct PetData {
  uint8_t version;
  uint8_t level;  // 1..PET_MAX_LEVEL
  char name[22];
  float food;     // 0..100 (sytost)
  float fun;      // 0..100 (zábava)
  float energy;   // 0..100
  uint32_t xp;
  uint32_t savedEpoch;  // unix time of the last save, 0 = unknown
  uint32_t ageMinutes;  // time spent switched on
};

class Pet {
 public:
  PetData d;

  Pet();
  void reset();
  bool valid() const;

  // dt in seconds while the pendant runs.
  void update(float dt, bool sleeping);
  // Time the pendant was switched off (only known with NTP time).
  void applyOffline(uint32_t seconds);

  PetResult feed(Food food);
  PetResult play();
  PetResult pet();
  PetResult shake();

  bool hungry() const { return d.food < 30; }
  bool bored() const { return d.fun < 30; }
  bool tired() const { return d.energy < 25; }
  bool exhausted() const { return d.energy < 8; }
  PetMood mood(bool sleeping) const;

  uint32_t xpForLevel(uint8_t level) const;  // XP needed to reach `level`
  uint32_t xpToNext() const;

 private:
  PetResult addXp(uint32_t amount);
  uint32_t secondsAccumulator;
};

const char *foodName(Food food);
const char *foodId(Food food);
int foodFromId(const char *id);
const char *moodName(PetMood mood);
const char *levelTitle(uint8_t level);
