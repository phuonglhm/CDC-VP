#pragma once

#include <cstdint>
#include <vector>

struct wb_config {
    bool is_enable = false;
    float r_gain = 1.0f;
    float b_gain = 1.0f;
};

class wb_block {
public:
    void process(const std::uint16_t* in,
                 std::uint16_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const wb_config& cfg) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const wb_config& cfg) const;
};
