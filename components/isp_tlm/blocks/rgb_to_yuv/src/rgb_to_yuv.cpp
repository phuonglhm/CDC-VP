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

    const std::size_t pixels = static_cast<std::size_t>(width) * height;

    for (std::size_t p = 0; p < pixels; ++p) {
        const std::size_t i = p * 3u;
        const std::int32_t r_raw = static_cast<std::int32_t>(in[i]);
        const std::int32_t g_raw = static_cast<std::int32_t>(in[i + 1u]);
        const std::int32_t b_raw = static_cast<std::int32_t>(in[i + 2u]);

        // Normalize dynamic bit-depth input to 8-bit range [0, 255]
        const std::int32_t shift = (bit_depth > 8) ? (bit_depth - 8) : 0;
        const std::int32_t r = (bit_depth >= 8) ? (r_raw >> shift) : (r_raw << (8 - bit_depth));
        const std::int32_t g = (bit_depth >= 8) ? (g_raw >> shift) : (g_raw << (8 - bit_depth));
        const std::int32_t b = (bit_depth >= 8) ? (b_raw >> shift) : (b_raw << (8 - bit_depth));

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
