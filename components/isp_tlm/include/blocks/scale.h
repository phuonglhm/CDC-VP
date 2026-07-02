#pragma once

#include <cstdint>
#include <vector>

struct scale_config {
    bool is_enable = false;
    std::uint16_t in_width = 0;
    std::uint16_t in_height = 0;
    std::uint16_t out_width = 0;
    std::uint16_t out_height = 0;
};

class scale_block {
public:
    void process(const std::uint8_t* in,
                 std::uint8_t* out,
                 std::uint32_t in_width,
                 std::uint32_t in_height,
                 std::uint32_t out_width,
                 std::uint32_t out_height,
                 const scale_config& cfg) const;

    void process(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t in_width,
                 std::uint32_t in_height,
                 std::uint32_t out_width,
                 std::uint32_t out_height,
                 const scale_config& cfg) const;
};
