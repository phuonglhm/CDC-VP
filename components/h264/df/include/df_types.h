#pragma once

#include <array>
#include <cstdint>

namespace h264::df {

struct DfRequest {
    std::array<std::uint8_t, 16> current_block; 
    std::uint8_t bs; // Boundary Strength (0-4)
    std::uint8_t qp; // Quantization Parameter
};

struct DfResult {
    std::array<std::uint8_t, 16> left_filtered;
    std::array<std::uint8_t, 16> right_filtered;
    bool valid;
};

} // namespace h264::df