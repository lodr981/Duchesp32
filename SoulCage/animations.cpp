#include "animations.h"
#include "anim_data.h"
#include <miniz.h>  // ROM inflater of the ESP32-S3

static const size_t kAnimationTotal = sizeof(kAnimations) / sizeof(kAnimations[0]);

// The decompressor state is ~11 KB, keep it off the task stack.
static tinfl_decompressor *inflater;

const Animation &introAnimation() {
  for (size_t i = 0; i < kAnimationTotal; i++) {
    if (!kAnimations[i].soul) {
      return kAnimations[i];
    }
  }
  return kAnimations[0];
}

size_t soulCount() {
  size_t n = 0;
  for (size_t i = 0; i < kAnimationTotal; i++) {
    n += kAnimations[i].soul;
  }
  return n;
}

const Animation &soulAt(size_t index) {
  for (size_t i = 0; i < kAnimationTotal; i++) {
    if (kAnimations[i].soul && index-- == 0) {
      return kAnimations[i];
    }
  }
  return kAnimations[0];
}

bool decodeFrame(const Animation &anim, uint16_t frame, uint8_t *out, size_t outLen) {
  if (frame >= anim.frameCount) {
    return false;
  }
  if (!inflater) {
    inflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!inflater) {
      return false;
    }
  }
  const uint8_t *src = anim.data + anim.offsets[frame];
  size_t srcLen = anim.offsets[frame + 1] - anim.offsets[frame];
  size_t dstLen = outLen;
  tinfl_init(inflater);
  tinfl_status status = tinfl_decompress(inflater, src, &srcLen, out, out, &dstLen,
                                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  return status == TINFL_STATUS_DONE && dstLen == outLen;
}
