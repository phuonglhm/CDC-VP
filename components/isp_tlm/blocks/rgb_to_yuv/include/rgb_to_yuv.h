#pragma once

#include <cstdint>
#include <vector>

struct csc_config {
    std::uint8_t conv_standard = 1;
};

class csc_block {
public:
    void process(const std::uint16_t* in,
                 std::uint8_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const csc_config& cfg,
                 std::uint8_t bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const csc_config& cfg,
                 std::uint8_t bit_depth) const;
};
