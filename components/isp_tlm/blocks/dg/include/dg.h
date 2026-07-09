#ifndef DG_BLOCK_H
#define DG_BLOCK_H

#include <stdint.h>
#include <stddef.h>

static const float kGainArray[] = {1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 32.0f, 64.0f};
static const size_t kGainArraySize = sizeof(kGainArray) / sizeof(kGainArray[0]);

struct dg_config {
   bool is_enable;
   bool is_auto;
   uint16_t current_gain;
};

class dg_block {
public:
   void process(
       const uint16_t *in, uint16_t *out, uint32_t w, uint32_t h, const dg_config &cfg, uint8_t bit_depth);
};

#endif // DG_BLOCK_H
