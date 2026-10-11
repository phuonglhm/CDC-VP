#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace h264::tq {

class Itq {
public:
    std::array<std::int16_t, 16>
    inverse_transform(const std::array<std::int16_t,16>& levels,
                      std::uint8_t qp, std::optional<std::int32_t> scaled_dc = std::nullopt) const;

    // Inverse 2x2 Hadamard plus DC scaling for 8-bit 4:2:0 chroma.
    std::array<std::int32_t,4> inverse_chroma_dc(
        const std::array<std::int16_t,4>& levels, std::uint8_t qp) const;

    std::array<std::uint8_t, 16>
    reconstruct(const std::array<std::int16_t, 16>& residual,
                const std::array<std::uint8_t, 16>& predictor) const;

private:
    static std::uint8_t clip1(std::int32_t value);
};

} // namespace h264::tq