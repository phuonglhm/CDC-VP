#pragma once

#include <cstdint>

namespace cdc::components {

struct block_coord_4x4 {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

inline block_coord_4x4 decode_block_coord_4x4(std::uint8_t block_idx,
                                              std::uint8_t x_low,
                                              std::uint8_t y_low)
{
    block_coord_4x4 coord;
    coord.x = (static_cast<std::uint32_t>(block_idx & 0x0fu) << 8u) |
              static_cast<std::uint32_t>(x_low);
    coord.y = (static_cast<std::uint32_t>((block_idx >> 4u) & 0x0fu) << 8u) |
              static_cast<std::uint32_t>(y_low);
    return coord;
}

inline std::uint32_t make_extended_block_address(std::uint8_t block_idx,
                                                 std::uint8_t x_low,
                                                 std::uint8_t y_low)
{
    return (static_cast<std::uint32_t>(block_idx) << 16u) |
           (static_cast<std::uint32_t>(y_low) << 8u) |
           static_cast<std::uint32_t>(x_low);
}

inline std::uint32_t ctu_index_from_4x4(std::uint32_t block_coord_4x4_axis)
{
    return block_coord_4x4_axis / 4u;
}

} // namespace cdc::components
