#pragma once

#include <cstdint>
#include <vector>

struct yuv420_config {
    bool is_enable = false;
};

class yuv420_block {
public:
    void process(const std::uint8_t* in,
                 std::uint8_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const yuv420_config& cfg) const;

    void process(const std::vector<std::uint8_t>& in,
                 std::vector<std::uint8_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const yuv420_config& cfg) const;

    std::size_t get_yuv420_size(std::uint32_t width, std::uint32_t height) const;
};
