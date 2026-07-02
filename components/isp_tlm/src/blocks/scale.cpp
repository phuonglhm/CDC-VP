#include "blocks/scale.h"

#include <algorithm>
#include <cstdint>

namespace {

std::uint8_t clip_to_uint8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

std::uint8_t interpolate_bilinear_yuv(const std::uint8_t* data,
                                       std::uint32_t width,
                                       std::uint32_t height,
                                       float x,
                                       float y,
                                       std::uint32_t channel)
{
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, static_cast<int>(width) - 1);
    const int y1 = std::min(y0 + 1, static_cast<int>(height) - 1);

    const float x_frac = x - x0;
    const float y_frac = y - y0;

    const std::size_t stride = width * 3u;
    const std::size_t offset_c = channel;

    const std::uint8_t p00 = data[y0 * stride + x0 * 3u + offset_c];
    const std::uint8_t p10 = data[y0 * stride + x1 * 3u + offset_c];
    const std::uint8_t p01 = data[y1 * stride + x0 * 3u + offset_c];
    const std::uint8_t p11 = data[y1 * stride + x1 * 3u + offset_c];

    const float top = p00 * (1.0f - x_frac) + p10 * x_frac;
    const float bottom = p01 * (1.0f - x_frac) + p11 * x_frac;

    return clip_to_uint8(static_cast<int>(top * (1.0f - y_frac) + bottom * y_frac));
}

} // namespace

void scale_block::process(const std::uint8_t* in,
                          std::uint8_t* out,
                          std::uint32_t in_width,
                          std::uint32_t in_height,
                          std::uint32_t out_width,
                          std::uint32_t out_height,
                          const scale_config& cfg) const
{
    if (in == nullptr || out == nullptr) {
        return;
    }

    if (!cfg.is_enable || (in_width == out_width && in_height == out_height)) {
        const std::size_t size = static_cast<std::size_t>(in_width) * in_height * 3u;
        std::copy(in, in + size, out);
        return;
    }

    const float x_scale = static_cast<float>(in_width) / static_cast<float>(out_width);
    const float y_scale = static_cast<float>(in_height) / static_cast<float>(out_height);

    for (std::uint32_t dy = 0; dy < out_height; ++dy) {
        for (std::uint32_t dx = 0; dx < out_width; ++dx) {
            const float src_x = dx * x_scale;
            const float src_y = dy * y_scale;

            for (std::uint32_t c = 0; c < 3; ++c) {
                const std::size_t dst_idx = (dy * out_width + dx) * 3u + c;
                out[dst_idx] = interpolate_bilinear_yuv(in, in_width, in_height, src_x, src_y, c);
            }
        }
    }
}

void scale_block::process(const std::vector<std::uint8_t>& in,
                          std::vector<std::uint8_t>& out,
                          std::uint32_t in_width,
                          std::uint32_t in_height,
                          std::uint32_t out_width,
                          std::uint32_t out_height,
                          const scale_config& cfg) const
{
    const std::size_t out_pixels = static_cast<std::size_t>(out_width) * out_height;
    out.resize(out_pixels * 3u);

    const std::size_t required_in = static_cast<std::size_t>(in_width) * in_height * 3u;
    if (in.size() < required_in) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), in_width, in_height, out_width, out_height, cfg);
}
