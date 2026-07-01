#pragma once

#include <algorithm>
#include <cstdint>

#include "encoder_defs.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// Generic block region for H.265 TLM.
//
// Can represent:
// - CTU: Coding Tree Unit
// - CU : Coding Unit
// - PU : Prediction Unit
// - TU : Transform Unit
//
// `size` is kept for compatibility with earlier skeleton code.
// For edge blocks and partitioning, use width/height.
// -----------------------------------------------------------------------------

struct block {
    std::uint32_t x = 0;
    std::uint32_t y = 0;

    std::uint32_t size = 16;

    std::uint32_t width = 16;
    std::uint32_t height = 16;

    block_type type = block_type::cu;
    std::uint32_t depth = 0;

    block() = default;

    block(std::uint32_t x_,
          std::uint32_t y_,
          std::uint32_t size_,
          block_type type_ = block_type::cu,
          std::uint32_t depth_ = 0)
        : x(x_),
          y(y_),
          size(size_),
          width(size_),
          height(size_),
          type(type_),
          depth(depth_)
    {
    }

    block(std::uint32_t x_,
          std::uint32_t y_,
          std::uint32_t width_,
          std::uint32_t height_,
          block_type type_,
          std::uint32_t depth_ = 0)
        : x(x_),
          y(y_),
          size(std::max(width_, height_)),
          width(width_),
          height(height_),
          type(type_),
          depth(depth_)
    {
    }

    std::uint32_t right() const
    {
        return x + width;
    }

    std::uint32_t bottom() const
    {
        return y + height;
    }

    bool is_square() const
    {
        return width == height;
    }

    std::uint32_t area() const
    {
        return width * height;
    }

    bool contains(std::uint32_t px, std::uint32_t py) const
    {
        return px >= x && px < right() && py >= y && py < bottom();
    }
};

} // namespace cdc::components
