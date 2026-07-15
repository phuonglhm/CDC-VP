#pragma once

#include "hevc/yuv420.hpp"

#include <vector>

namespace hevc {

// Quantized coefficients for one 8x8, 16x16 or 32x32 HEVC transform unit. Coefficients use
// raster order; entropy coding applies the HEVC grouped diagonal scan later.
struct QuantizedBlock {
    unsigned size = 0;
    std::vector<int> coefficients;

    [[nodiscard]] bool has_nonzero() const;
};

// Forward integer DCT-II and scalar quantization for an 8-bit plane whose
// intra prediction is the constant value supplied by prediction.
[[nodiscard]] QuantizedBlock transform_quantize_block(
    const Plane& plane, unsigned x0, unsigned y0, unsigned block_size,
    int qp, int prediction = 128);

// Variant for spatial intra prediction. The prediction vector is one square
// block in raster order and must contain block_size * block_size samples.
[[nodiscard]] QuantizedBlock transform_quantize_predicted_block(
    const Plane& plane, unsigned x0, unsigned y0, unsigned block_size,
    int qp, const std::vector<std::uint8_t>& prediction);

// Inverse quantization and inverse integer transform. The reconstructed block
// is clipped to 8-bit samples and written into destination.
void inverse_reconstruct_block(const QuantizedBlock& block, int qp,
                               Plane& destination, unsigned x0, unsigned y0,
                               int prediction = 128);

void inverse_reconstruct_predicted_block(
    const QuantizedBlock& block, int qp, Plane& destination,
    unsigned x0, unsigned y0,
    const std::vector<std::uint8_t>& prediction);

} // namespace hevc
