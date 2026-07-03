#ifndef DG_BLOCK_H
#define DG_BLOCK_H

#include <stdint.h>

struct dg_config {
   bool is_enable;
   bool is_auto;
   uint16_t current_gain;
   int32_t ae_feedback;
};

class dg_block {
public:
   void process(
       const uint16_t *in, uint16_t *out, uint32_t w, uint32_t h, const dg_config &cfg, uint8_t bit_depth);
};

#endif // DG_BLOCK_H
