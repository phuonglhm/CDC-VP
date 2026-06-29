#pragma once

#include <cstdint>

namespace cdc::components {

enum class prediction_mode : std::uint32_t {
    intra = 0,
    inter = 1,
};

struct prediction_result {
    prediction_mode mode = prediction_mode::intra;
    std::uint32_t cost = 0;
};

} // namespace cdc::components
