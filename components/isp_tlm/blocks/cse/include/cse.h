#pragma once

#include <cstdint>
#include <vector>

struct cse_config {
    bool is_enable = false;
    float saturation_gain = 4.0f;
};

class cse_block {
public:
    void process(const std::uint8_t* in,
                 std::uint8_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const cse_config& cfg) const;

    void process(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const cse_config& cfg) const;
};
