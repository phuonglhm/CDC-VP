#pragma once

#include <cstdint>
#include <vector>

struct sharpen_config {
    bool is_enable = false;
    std::uint8_t sharpen_sigma = 1;
    std::uint16_t sharpen_strength = 1;
};

class sharpen_block {
public:
    void process(const std::uint8_t* in,
                 std::uint8_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const sharpen_config& cfg) const;

    void process(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const sharpen_config& cfg) const;
};
