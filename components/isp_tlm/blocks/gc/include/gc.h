#pragma once

#include <cstdint>
#include <vector>

struct gc_config {
    bool is_enable = false;
    std::vector<std::uint16_t> gamma_lut_8;
    std::vector<std::uint16_t> gamma_lut_10;
    std::vector<std::uint16_t> gamma_lut_12;
    std::vector<std::uint16_t> gamma_lut_14;
};

class gc_block {
public:
    void process(const std::uint16_t* in,
                 std::uint16_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg,
                 std::uint8_t bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg,
                 std::uint8_t bit_depth) const;
};
