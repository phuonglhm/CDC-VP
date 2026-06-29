#ifndef BNR_BLOCK_H
#define BNR_BLOCK_H

#include <stdint.h>
#include "isp_types.h"

struct bnr_config {
    bool is_enable;
    uint8_t filter_window;
    float r_std_dev_s;
    float r_std_dev_r;
    float g_std_dev_s;
    float g_std_dev_r;
    float b_std_dev_s;
    float b_std_dev_r;
};

class bnr_block {
public:
    void process(const uint16_t* in, uint16_t* out, uint32_t w, uint32_t h, const bnr_config& cfg, cfa_types bayer_pattern, uint8_t bit_depth);
};

#endif // BNR_BLOCK_H
