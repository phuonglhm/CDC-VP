#pragma once

#include <cstdint>
#include <vector>

struct ccm_config {
    bool is_enable = false;
    float corrected_red[3] = {1.0f, 0.0f, 0.0f};
    float corrected_green[3] = {0.0f, 1.0f, 0.0f};
    float corrected_blue[3] = {0.0f, 0.0f, 1.0f};
};

class ccm_block {
public:
    void process(const std::uint16_t* in,
                 std::uint16_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const ccm_config& cfg,
                 std::uint8_t bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const ccm_config& cfg,
                 std::uint8_t bit_depth) const;
};
