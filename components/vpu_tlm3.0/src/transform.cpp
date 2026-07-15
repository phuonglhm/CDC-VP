#include "hevc/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace hevc {
namespace {

constexpr std::array<int, 6> kQuantScales{{26214, 23302, 20560, 18396, 16384, 14564}};
constexpr std::array<int, 6> kInverseQuantScales{{40, 45, 51, 57, 64, 72}};

// Magnitudes of the normative HEVC integer DCT basis for angles m*pi/64.
// Symmetry reconstructs every 8x8, 16x16 and 32x32 matrix element from this row.
constexpr std::array<int, 33> kDctMagnitude{{
     0, 90, 90, 90, 89, 88, 87, 85, 83, 82, 80,
    78, 75, 73, 70, 67, 64, 61, 57, 54, 50, 46,
    43, 38, 36, 31, 25, 22, 18, 13,  9,  4,  0,
}};

int rounded_shift(std::int64_t value, unsigned shift) {
    if (shift == 0) return static_cast<int>(value);
    return static_cast<int>((value + (std::int64_t{1} << (shift - 1))) >> shift);
}

int transform_matrix(unsigned size, unsigned frequency, unsigned sample) {
    if (frequency == 0) return 64;
    const unsigned scale = 32 / size;
    unsigned angle = ((2 * sample + 1) * frequency * scale) & 127U;
    if (angle > 64) angle = 128 - angle;
    int sign = 1;
    if (angle > 32) {
        angle = 64 - angle;
        sign = -1;
    }
    return sign * kDctMagnitude[angle];
}

int quantize(int coefficient, unsigned block_size, int qp) {
    const int transform_shift =
        15 - 8 - static_cast<int>(std::countr_zero(block_size));
    const int qbits = 14 + qp / 6 + transform_shift;
    const std::int64_t add = std::int64_t{171} << (qbits - 9);
    const auto magnitude = static_cast<std::int64_t>(std::abs(coefficient));
    const int level = static_cast<int>(
        (magnitude * kQuantScales[static_cast<unsigned>(qp % 6)] + add) >> qbits);
    return coefficient < 0 ? -level : level;
}

int dequantize(int level, unsigned block_size, int qp) {
    const int transform_shift =
        15 - 8 - static_cast<int>(std::countr_zero(block_size));
    const int right_shift = 6 - (transform_shift + qp / 6);
    std::int64_t value = static_cast<std::int64_t>(level) *
                         kInverseQuantScales[static_cast<unsigned>(qp % 6)];
    if (right_shift > 0) {
        value = (value + (std::int64_t{1} << (right_shift - 1))) >> right_shift;
    } else {
        value <<= -right_shift;
    }
    return static_cast<int>(std::clamp<std::int64_t>(value, -32768, 32767));
}

void validate_block(unsigned size, int qp) {
    if ((size != 8 && size != 16 && size != 32) || qp < 0 || qp > 51) {
        throw std::invalid_argument(
            "full-AC transform supports 8x8/16x16/32x32 and QP 0..51");
    }
}

QuantizedBlock transform_quantize_impl(
    const Plane& plane, unsigned x0, unsigned y0, unsigned block_size,
    int qp, int constant_prediction,
    const std::vector<std::uint8_t>* spatial_prediction) {
    validate_block(block_size, qp);
    if (x0 + block_size > plane.width || y0 + block_size > plane.height) {
        throw std::invalid_argument("transform block is outside its plane");
    }
    if (spatial_prediction != nullptr &&
        spatial_prediction->size() !=
            static_cast<std::size_t>(block_size) * block_size) {
        throw std::invalid_argument("invalid spatial prediction block");
    }

    const unsigned n = block_size;
    const unsigned shift_first = std::countr_zero(n) - 1;
    const unsigned shift_second = std::countr_zero(n) + 6;
    std::vector<int> temporary(static_cast<std::size_t>(n) * n);
    std::vector<int> transformed(static_cast<std::size_t>(n) * n);

    for (unsigned y = 0; y < n; ++y) {
        for (unsigned kx = 0; kx < n; ++kx) {
            std::int64_t sum = 0;
            for (unsigned x = 0; x < n; ++x) {
                const int prediction = spatial_prediction == nullptr
                    ? constant_prediction
                    : (*spatial_prediction)[static_cast<std::size_t>(y) * n + x];
                const int residual = static_cast<int>(plane.at(x0 + x, y0 + y)) -
                                     prediction;
                sum += static_cast<std::int64_t>(transform_matrix(n, kx, x)) *
                       residual;
            }
            temporary[static_cast<std::size_t>(y) * n + kx] =
                rounded_shift(sum, shift_first);
        }
    }
    for (unsigned ky = 0; ky < n; ++ky) {
        for (unsigned kx = 0; kx < n; ++kx) {
            std::int64_t sum = 0;
            for (unsigned y = 0; y < n; ++y) {
                sum += static_cast<std::int64_t>(transform_matrix(n, ky, y)) *
                       temporary[static_cast<std::size_t>(y) * n + kx];
            }
            transformed[static_cast<std::size_t>(ky) * n + kx] =
                quantize(rounded_shift(sum, shift_second), n, qp);
        }
    }
    return {n, std::move(transformed)};
}

void inverse_reconstruct_impl(
    const QuantizedBlock& block, int qp, Plane& destination,
    unsigned x0, unsigned y0, int constant_prediction,
    const std::vector<std::uint8_t>* spatial_prediction) {
    validate_block(block.size, qp);
    const unsigned n = block.size;
    if (block.coefficients.size() != static_cast<std::size_t>(n) * n ||
        x0 + n > destination.width || y0 + n > destination.height) {
        throw std::invalid_argument("invalid inverse transform block");
    }
    if (spatial_prediction != nullptr &&
        spatial_prediction->size() != static_cast<std::size_t>(n) * n) {
        throw std::invalid_argument("invalid spatial prediction block");
    }

    std::vector<int> dequantized(block.coefficients.size());
    std::transform(block.coefficients.begin(), block.coefficients.end(),
                   dequantized.begin(),
                   [&](int level) { return dequantize(level, n, qp); });
    std::vector<int> temporary(block.coefficients.size());

    // The normative inverse performs the vertical pass first. The order is
    // observable because HEVC rounds and clips between the two 1-D passes.
    for (unsigned y = 0; y < n; ++y) {
        for (unsigned kx = 0; kx < n; ++kx) {
            std::int64_t sum = 0;
            for (unsigned ky = 0; ky < n; ++ky) {
                sum += static_cast<std::int64_t>(transform_matrix(n, ky, y)) *
                       dequantized[static_cast<std::size_t>(ky) * n + kx];
            }
            temporary[static_cast<std::size_t>(y) * n + kx] =
                std::clamp(rounded_shift(sum, 7), -32768, 32767);
        }
    }
    for (unsigned y = 0; y < n; ++y) {
        for (unsigned x = 0; x < n; ++x) {
            std::int64_t sum = 0;
            for (unsigned kx = 0; kx < n; ++kx) {
                sum += static_cast<std::int64_t>(transform_matrix(n, kx, x)) *
                       temporary[static_cast<std::size_t>(y) * n + kx];
            }
            const int prediction = spatial_prediction == nullptr
                ? constant_prediction
                : (*spatial_prediction)[static_cast<std::size_t>(y) * n + x];
            const int reconstructed = prediction + rounded_shift(sum, 12);
            destination.samples[static_cast<std::size_t>(y0 + y) *
                                    destination.width + x0 + x] =
                static_cast<std::uint8_t>(std::clamp(reconstructed, 0, 255));
        }
    }
}

} // namespace

bool QuantizedBlock::has_nonzero() const {
    return std::any_of(coefficients.begin(), coefficients.end(),
                       [](int value) { return value != 0; });
}

QuantizedBlock transform_quantize_block(const Plane& plane, unsigned x0,
                                        unsigned y0, unsigned block_size,
                                        int qp, int prediction) {
    return transform_quantize_impl(
        plane, x0, y0, block_size, qp, prediction, nullptr);
}

QuantizedBlock transform_quantize_predicted_block(
    const Plane& plane, unsigned x0, unsigned y0, unsigned block_size,
    int qp, const std::vector<std::uint8_t>& prediction) {
    return transform_quantize_impl(
        plane, x0, y0, block_size, qp, 0, &prediction);
}

void inverse_reconstruct_block(const QuantizedBlock& block, int qp,
                               Plane& destination, unsigned x0, unsigned y0,
                               int prediction) {
    inverse_reconstruct_impl(
        block, qp, destination, x0, y0, prediction, nullptr);
}

void inverse_reconstruct_predicted_block(
    const QuantizedBlock& block, int qp, Plane& destination,
    unsigned x0, unsigned y0,
    const std::vector<std::uint8_t>& prediction) {
    inverse_reconstruct_impl(
        block, qp, destination, x0, y0, 0, &prediction);
}

} // namespace hevc
