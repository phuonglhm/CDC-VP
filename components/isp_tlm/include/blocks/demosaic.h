#ifndef DEMOSAIC_BLOCK_H
#define DEMOSAIC_BLOCK_H

#include <stdint.h>

struct demosaic_config {
   bool is_enable;
};

class demosaic_block {
public:
   // out points to a contiguous 3-channel buffer RGB of size 3 * w * h.
   // formatted as followed: R0, G0, B0, R1, G1, B1,...
   void process(const uint16_t *in,
                uint16_t *out,
                uint32_t w,
                uint32_t h,
                const demosaic_config &cfg,
                uint8_t bayer_pattern,
                uint8_t bit_depth);
};

#endif // DEMOSAIC_BLOCK_H
