#ifndef LSC_BLOCK_H
#define LSC_BLOCK_H

#include <stdint.h>

struct lsc_config {
    bool is_enable;
    uint16_t grid_width;
    uint16_t grid_height;
};

class lsc_block {
public:
    void process(const uint16_t* in, uint16_t* out, uint32_t w, uint32_t h, const lsc_config& cfg, const float* lsc_mem_ptr, uint8_t bayer_pattern);
};

#endif // LSC_BLOCK_H
