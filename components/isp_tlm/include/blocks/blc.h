#ifndef BLC_BLOCK_H
#define BLC_BLOCK_H

#include <stdint.h>
#include "../isp_types.h"

struct blc_config {
   bool is_enable;
   bool is_linear;
   uint16_t r_offset;
   uint16_t gr_offset;
   uint16_t gb_offset;
   uint16_t b_offset;
   uint16_t r_sat;
   uint16_t gr_sat;
   uint16_t gb_sat;
   uint16_t b_sat;
};

class blc_block {
public:
   void process(const uint16_t *in,
                uint16_t *out,
                uint32_t w,
                uint32_t h,
                const blc_config &cfg,
                cfa_types bayer_pattern,
                uint8_t bit_depth);
};

#endif // BLC_BLOCK_H
