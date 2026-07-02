#pragma once

#include <cstdint>
#include <vector>

#include "isp_types.h"

struct awb_config {
    bool is_enable = false;
    std::uint8_t algorithm = 0;
    float underexposed_percentage = 0.01f;
    float overexposed_percentage = 0.01f;
    float percentage = 0.5f;
    float r_gain_out = 1.0f;
    float b_gain_out = 1.0f;
};

class awb_block {
public:
    void process(const std::uint16_t* in,
                 std::uint32_t width,
                 std::uint32_t height,
                 awb_config& cfg,
                 std::uint8_t bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::uint32_t width,
                 std::uint32_t height,
                 awb_config& cfg,
                 std::uint8_t bit_depth) const;
};
