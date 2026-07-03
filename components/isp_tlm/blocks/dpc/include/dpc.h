#ifndef DPC_BLOCK_H
#define DPC_BLOCK_H

#include <stdint.h>

struct dpc_config {
    bool is_enable;
    uint16_t dp_threshold;
};

class dpc_block {
public:
    void process(const uint16_t* in, uint16_t* out, uint32_t w, uint32_t h, const dpc_config& cfg);
};

#endif // DPC_BLOCK_H
