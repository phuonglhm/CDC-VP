#pragma once

#include <cstdint>
#include <vector>

struct twodnr_config {
    bool is_enable = false;
    std::uint8_t window_size = 5;
    std::uint8_t patch_size = 3;
    std::uint16_t wts = 10;
};

class twodnr_block {
public:
    void process(const std::uint8_t* in,
                 std::uint8_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const twodnr_config& cfg) const;

    void process(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const twodnr_config& cfg) const;
};
