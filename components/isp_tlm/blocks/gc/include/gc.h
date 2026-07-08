#pragma once

#include <cstdint>
#include <vector>

struct gc_config {
    bool is_enable = false;
    std::uint8_t bit_depth = 12;
};

class gc_block {
public:
    void process(const std::uint16_t* in,
                 std::uint16_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg) const;
};
