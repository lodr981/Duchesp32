#pragma once
// The panda ghost: procedural drawing, scenes and the action state machine.
// Platform independent (see tools/preview for the desktop preview).
#include "gfx.h"
#include "pet.h"

enum GhostAction : uint8_t {
  GA_NONE,
  GA_WAVE,
  GA_DANCE,
  GA_BOO,
  GA_HIDE,
  GA_SPIN,
  GA_LAUGH,
  GA_LOVE,
  GA_SCARED,
  GA_DIZZY,
  GA_EAT,
  GA_REFUSE,
  GA_PLAY,
  GA_YAWN,
  GA_LEVELUP,
  GA_COUNT
};

enum Scene : uint8_t { SCENE_GRAVEYARD, SCENE_BAMBOO, SCENE_HOUSE, SCENE_JAR, SCENE_WINTER, SCENE_COUNT };

enum EyeColor : uint8_t { EYES_GREEN, EYES_PURPLE, EYES_RED, EYES_BLUE, EYES_COUNT };

struct GhostInput {
  float lean = 0;          // -1..1, pendant tilted left/right
  bool upsideDown = false;
  bool lyingFlat = false;
};

struct GhostLook {
  uint8_t scene = SCENE_GRAVEYARD;
  uint8_t eyeColor = EYES_GREEN;
  uint8_t spooky = 1;  // 0 cute, 1 sometimes spooky, 2 very spooky
  uint8_t level = 1;
};

struct GhostPose;

class Ghost {
 public:
  Ghost();
  void trigger(GhostAction action, Food food = FOOD_BAMBOO);
  void setSleeping(bool sleeping);
  bool sleeping() const { return asleep; }
  void setNeeds(bool hungry, bool bored, bool tired) {
    needHungry = hungry;
    needBored = bored;
    needTired = tired;
  }
  // Spontaneous actions (BOO, waving...) when idle.
  void setSpontaneous(bool enabled) { spontaneous = enabled; }

  void update(float dt, const GhostInput &input, const GhostLook &look);
  void render(Canvas &canvas, const GhostLook &look);

  bool busy() const { return action != GA_NONE; }
  GhostAction current() const { return action; }

 private:
  float time = 0;        // global animation clock
  float actionTime = 0;  // time since the current action started
  GhostAction action = GA_NONE;
  Food food = FOOD_BAMBOO;
  bool asleep = false;
  float sleepBlend = 0;  // 0 awake .. 1 asleep (smooth)
  bool needHungry = false, needBored = false, needTired = false;
  bool spontaneous = true;
  float nextIdleEvent = 8;
  float nextBlink = 2, blinkUntil = 0;
  float bubbleUntil = 0;
  float lean = 0, flipBlend = 0;
  bool flatLook = false;
  uint32_t rng = 12345;

  float random01();
  float actionDuration(GhostAction a) const;
  void computePose(GhostPose &p, const GhostLook &look) const;
};

const char *ghostActionId(GhostAction action);
const char *ghostActionName(GhostAction action);
int ghostActionFromId(const char *id);
const char *sceneName(uint8_t scene);
const char *eyeColorName(uint8_t color);
