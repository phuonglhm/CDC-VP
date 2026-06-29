#pragma once

#include <cstdint>

namespace cdc::components {

struct block {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t size = 16;
};

} // namespace cdc::components
