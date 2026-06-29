#pragma once

#include <cstdint>
#include <vector>

#include "frame.h"
#include "prediction_result.h"

namespace cdc::components {

class cabac {
public:
    std::vector<std::uint8_t> encode(const frame& reconstructed,
                                     const prediction_result& result) const;
};

} // namespace cdc::components
