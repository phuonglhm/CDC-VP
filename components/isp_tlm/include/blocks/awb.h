#ifndef AWB_BLOCK_H
#define AWB_BLOCK_H

#include <stdint.h>
#include "isp_types.h"

struct awb_config {
   bool is_enable;
   uint8_t algorithm;
   float underexposed_percentage;
   float overexposed_percentage;
   float percentage;
   float r_gain_out;
   float b_gain_out;
};

class awb_block {
public:
   void process(const uint16_t *in, uint32_t w, uint32_t h, awb_config &cfg, uint8_t bit_depth);
};

#endif // AWB_BLOCK_H
