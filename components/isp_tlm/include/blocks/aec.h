#ifndef AEC_BLOCK_H
#define AEC_BLOCK_H

#include <stdint.h>

struct aec_config {
    bool is_enable;
    uint8_t center_illuminance;
    float histogram_skewness;
    int32_t ae_feedback;
};

class aec_block {
public:
    void process(const uint16_t* in, uint32_t w, uint32_t h, const aec_config& cfg);
};

#endif // AEC_BLOCK_H
