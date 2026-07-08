#include "dg.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

static const float kGainArray[] = {1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 32.0f, 64.0f};
static const size_t kGainArraySize = sizeof(kGainArray) / sizeof(kGainArray[0]);

void dg_block::process(
    const uint16_t *in, uint16_t *out, uint32_t w, uint32_t h, const dg_config &cfg, uint8_t bit_depth) {
   if (!cfg.is_enable) {
      std::memcpy(out, in, w * h * sizeof(uint16_t));
      return;
   }

   uint32_t bit_range = (1u << bit_depth) - 1;

   // determine active gain index
   uint16_t gain_idx = cfg.current_gain;

   gain_idx = (gain_idx >= kGainArraySize) ? kGainArraySize - 1 : gain_idx;

   float gain_multiplier = kGainArray[gain_idx];

   for (uint32_t i = 0; i < h; ++i) {
      for (uint32_t j = 0; j < w; ++j) {
         uint32_t idx = i * w + j;
         float val = static_cast<float>(in[idx]) * gain_multiplier;
         out[idx] = static_cast<uint16_t>(std::clamp(val, 0.0f, static_cast<float>(bit_range)));
      }
   }
}
