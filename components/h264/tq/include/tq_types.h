#pragma once

#include <array>
#include <cstdint>

namespace h264::tq {

enum class BlockClass : std::uint8_t {
    Luma4x4 = 0,
    Luma16x16Dc = 1,
    ChromaDc = 2,
    ChromaAc = 3
};

struct TqRequest {
    std::array<std::int16_t, 16> residual{};
    std::array<std::uint8_t, 16> predictor{};
    std::uint8_t qp{26};
    BlockClass block_class{BlockClass::Luma4x4};
};

struct TqResult {
    std::array<std::int16_t, 16> levels{};
    std::array<std::int16_t, 16> reconstructed_residual{};
    std::array<std::uint8_t, 16> reconstructed{};
    bool valid{false};
};

} // namespace h264::tq