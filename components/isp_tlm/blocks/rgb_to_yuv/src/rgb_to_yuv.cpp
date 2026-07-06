#include "rgb_to_yuv.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::int32_t LUMA_OFFSET = 0;
constexpr std::int32_t CHROMA_OFFSET = 128;

std::uint8_t clip_to_uint8(std::int32_t value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

} // namespace

void csc_block::process(const std::uint16_t* in,
                        std::uint8_t* out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const csc_config& cfg,
                        std::uint8_t bit_depth) const
{
    if (in == nullptr || out == nullptr || width == 0 || height == 0) {
        return;
    }

    // Resolve input bit-depth: use cfg-provided value, fall back to 12 for back-compat.
    const std::uint32_t effective_bit_depth = (bit_depth > 0) ? bit_depth : 12u;

    // Number of bits to drop (or gain) when normalizing to 8-bit [0, 255].
    // 12-bit input -> shift 4.   16-bit input -> shift 8.   <8-bit -> left-shift up.
    const std::int32_t shift_to_8bit =
        static_cast<std::int32_t>(effective_bit_depth) - 8;

    const std::size_t pixels = static_cast<std::size_t>(width) * height;

    if (!cfg.is_enable) {
        for (std::size_t p = 0; p < pixels; ++p) {
            for (std::size_t c = 0; c < 3u; ++c) {
                const std::int32_t raw = static_cast<std::int32_t>(in[p * 3u + c]);
                std::int32_t v;
                if (shift_to_8bit > 0) {
                    // Down-shift with rounding (less quantization noise than pure truncation)
                    v = (raw + (1 << (shift_to_8bit - 1))) >> shift_to_8bit;
                } else if (shift_to_8bit < 0) {
                    v = raw << (-shift_to_8bit);
                } else {
                    v = raw;
                }
                out[p * 3u + c] = clip_to_uint8(v);
            }
        }
        return;
    }

    for (std::size_t p = 0; p < pixels; ++p) {
        const std::size_t i = p * 3u;
        const std::int32_t r_raw = static_cast<std::int32_t>(in[i]);
        const std::int32_t g_raw = static_cast<std::int32_t>(in[i + 1u]);
        const std::int32_t b_raw = static_cast<std::int32_t>(in[i + 2u]);

        // Normalize dynamic bit-depth input to 8-bit range [0, 255] with rounding.
        auto to_8bit = [shift_to_8bit](std::int32_t v) -> std::int32_t {
            if (shift_to_8bit > 0) {
                return (v + (1 << (shift_to_8bit - 1))) >> shift_to_8bit;
            }
            if (shift_to_8bit < 0) {
                return v << (-shift_to_8bit);
            }
            return v;
        };

        const std::int32_t r = to_8bit(r_raw);
        const std::int32_t g = to_8bit(g_raw);
        const std::int32_t b = to_8bit(b_raw);

        std::int32_t y_raw, u_raw, v_raw;

        if (cfg.conv_standard == 1) {
            y_raw = (54 * r + 183 * g + 18 * b) >> 8;
            u_raw = (-29 * r - 99 * g + 128 * b) >> 8;
            v_raw = (128 * r - 116 * g - 12 * b) >> 8;
        } else {
            y_raw = (77 * r + 150 * g + 29 * b) >> 8;
            u_raw = (-43 * r - 84 * g + 128 * b) >> 8;
            v_raw = (128 * r - 107 * g - 21 * b) >> 8;
        }

        out[p * 3u]     = clip_to_uint8(y_raw + LUMA_OFFSET);
        out[p * 3u + 1u] = clip_to_uint8(u_raw + CHROMA_OFFSET);
        out[p * 3u + 2u] = clip_to_uint8(v_raw + CHROMA_OFFSET);
    }
}

void csc_block::process(const std::vector<std::uint16_t>& in,
                        std::vector<std::uint8_t>& out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const csc_config& cfg,
                        std::uint8_t bit_depth) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    out.resize(pixels * 3u);

    if (in.size() < pixels * 3u) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg, bit_depth);
}
