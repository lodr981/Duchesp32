#pragma once
#include <Arduino.h>

struct Animation {
  const char *id;
  const char *name;
  const uint8_t *data;      // concatenated zlib streams
  const uint32_t *offsets;  // frameCount + 1 entries
  uint16_t frameCount;
  bool soul;                // selectable as soul (intro is not)
};

const Animation &introAnimation();
size_t soulCount();
const Animation &soulAt(size_t index);

// Inflates one frame (240x240 RGB565 big-endian) into `out`. Returns false on corrupt data.
bool decodeFrame(const Animation &anim, uint16_t frame, uint8_t *out, size_t outLen);
