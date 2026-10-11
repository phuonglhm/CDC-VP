#pragma once

#include <array>
#include <cstdint>

namespace h264::df {

struct DfRequest {
    std::array<std::uint8_t, 16> current_block;
    std::uint8_t bs; // Boundary Strength (0-4)
    std::uint8_t qp; // Edge QP, already mapped for chroma.
    bool chroma{false};
    int alpha_offset{0}, beta_offset{0}; // Actual offsets, not div2 syntax fields.
};

struct DfResult {
    std::array<std::uint8_t, 16> left_filtered;
    std::array<std::uint8_t, 16> right_filtered;
    bool valid;
};

} // namespace h264::df
