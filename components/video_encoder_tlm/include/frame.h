#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cdc::components {

struct frame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> luma;

    frame() = default;
    frame(std::uint32_t width, std::uint32_t height);

    std::size_t pixel_count() const;
};

} // namespace cdc::components
