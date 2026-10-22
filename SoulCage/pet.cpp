#include "pet.h"
#include <string.h>

static const uint32_t LEVEL_XP[PET_MAX_LEVEL] = {0, 40, 120, 260, 500};

static float clampStat(float v) {
  return v < 0 ? 0 : v > 100 ? 100 : v;
}

Pet::Pet() {
  reset();
}

void Pet::reset() {
  memset(&d, 0, sizeof(d));
  d.version = PET_DATA_VERSION;
  d.level = 1;
  strncpy(d.name, "Panďulák", sizeof(d.name) - 1);
  d.food = 80;
  d.fun = 80;
  d.energy = 90;
  secondsAccumulator = 0;
}

bool Pet::valid() const {
  return d.version == PET_DATA_VERSION && d.level >= 1 && d.level <= PET_MAX_LEVEL && d.food >= 0 && d.food <= 100 &&
         d.fun >= 0 && d.fun <= 100 && d.energy >= 0 && d.energy <= 100;
}

void Pet::update(float dt, bool sleeping) {
  // Full to empty in roughly 6 h (food), 5 h (fun), 8 h (energy) of wearing.
  d.food = clampStat(d.food - dt / 216.0f);
  d.fun = clampStat(d.fun - dt / (sleeping ? 600.0f : 180.0f));
  if (sleeping) {
    d.energy = clampStat(d.energy + dt / 30.0f);  // a nap of ~50 min recharges fully
  } else {
    d.energy = clampStat(d.energy - dt / 288.0f);
  }
  secondsAccumulator += (uint32_t)(dt * 1000);
  while (secondsAccumulator >= 60000) {
    secondsAccumulator -= 60000;
    d.ageMinutes++;
  }
}

void Pet::applyOffline(uint32_t seconds) {
  // While switched off the ghost "sleeps": needs drop slowly and never below 15.
  if (seconds > 24 * 3600) {
    seconds = 24 * 3600;
  }
  float hours = seconds / 3600.0f;
  if (d.food > 15) d.food = d.food - hours * 4 < 15 ? 15 : d.food - hours * 4;
  if (d.fun > 15) d.fun = d.fun - hours * 4 < 15 ? 15 : d.fun - hours * 4;
  d.energy = clampStat(d.energy + hours * 30);
}

PetResult Pet::addXp(uint32_t amount) {
  d.xp += amount;
  if (d.level < PET_MAX_LEVEL && d.xp >= LEVEL_XP[d.level]) {
    d.level++;
    return PET_LEVEL_UP;
  }
  return PET_OK;
}

PetResult Pet::feed(Food food) {
  if (d.food > 95 && food != FOOD_CANDY) {
    return PET_REFUSED;  // full
  }
  switch (food) {
    case FOOD_BAMBOO: d.food += 30; break;
    case FOOD_COOKIE: d.food += 15; d.fun += 10; break;
    case FOOD_PIE: d.food += 40; d.energy += 5; break;
    case FOOD_CANDY:
      if (d.fun > 95) {
        return PET_REFUSED;
      }
      d.fun += 20;
      d.food += 5;
      break;
    default: return PET_REFUSED;
  }
  d.food = clampStat(d.food);
  d.fun = clampStat(d.fun);
  d.energy = clampStat(d.energy);
  // Feeding a hungry ghost is worth more than stuffing a full one.
  return addXp(d.food < 60 ? 4 : 2);
}

PetResult Pet::play() {
  if (d.energy < 12) {
    return PET_REFUSED;  // too tired
  }
  d.fun = clampStat(d.fun + 25);
  d.energy = clampStat(d.energy - 8);
  d.food = clampStat(d.food - 3);
  return addXp(5);
}

PetResult Pet::pet() {
  d.fun = clampStat(d.fun + 8);
  return addXp(2);
}

PetResult Pet::shake() {
  d.fun = clampStat(d.fun + 3);
  return addXp(1);
}

PetMood Pet::mood(bool sleeping) const {
  if (sleeping) return MOOD_SLEEPING;
  if (tired()) return MOOD_TIRED;
  if (hungry()) return MOOD_HUNGRY;
  if (bored()) return MOOD_BORED;
  if (d.food > 65 && d.fun > 65 && d.energy > 50) return MOOD_HAPPY;
  return MOOD_OK;
}

uint32_t Pet::xpForLevel(uint8_t level) const {
  if (level < 1) return 0;
  if (level > PET_MAX_LEVEL) level = PET_MAX_LEVEL;
  return LEVEL_XP[level - 1];
}

uint32_t Pet::xpToNext() const {
  if (d.level >= PET_MAX_LEVEL) return 0;
  return LEVEL_XP[d.level] - d.xp;
}

static const char *const FOOD_IDS[FOOD_COUNT] = {"bamboo", "cookie", "pie", "candy"};
static const char *const FOOD_NAMES[FOOD_COUNT] = {"bambus", "sušenka", "dýňový koláč", "bonbon"};

const char *foodName(Food food) {
  return food < FOOD_COUNT ? FOOD_NAMES[food] : "";
}

const char *foodId(Food food) {
  return food < FOOD_COUNT ? FOOD_IDS[food] : "";
}

int foodFromId(const char *id) {
  for (int i = 0; i < FOOD_COUNT; i++) {
    if (strcmp(id, FOOD_IDS[i]) == 0) return i;
  }
  return -1;
}

const char *moodName(PetMood mood) {
  switch (mood) {
    case MOOD_HAPPY: return "šťastný";
    case MOOD_HUNGRY: return "má hlad";
    case MOOD_BORED: return "nudí se";
    case MOOD_TIRED: return "je unavený";
    case MOOD_SLEEPING: return "spí";
    default: return "v pohodě";
  }
}

const char *levelTitle(uint8_t level) {
  static const char *const titles[PET_MAX_LEVEL] = {"Dušínek", "Malý strašák", "Strašidlo", "Velký bubák",
                                                    "Král duchů"};
  return level >= 1 && level <= PET_MAX_LEVEL ? titles[level - 1] : "";
}
