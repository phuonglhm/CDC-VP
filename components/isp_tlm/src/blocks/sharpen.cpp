#include "blocks/sharpen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr std::int32_t CHROMA_OFFSET = 128;

std::uint8_t clip_to_uint8(std::int32_t value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

std::uint8_t clip_to_uint8(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return static_cast<std::uint8_t>(std::lround(value));
}

std::vector<float> create_gaussian_kernel(std::uint8_t sigma)
{
    const int radius = static_cast<int>((sigma > 0) ? std::ceil(sigma * 3.0) : 1);
    const int size = 2 * radius + 1;
    std::vector<float> kernel(static_cast<std::size_t>(size * size));

    const float sigma_sq = static_cast<float>(sigma * sigma);
    float sum = 0.0f;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const float value = std::exp(-static_cast<float>(dx * dx + dy * dy) / (2.0f * sigma_sq));
            kernel[static_cast<std::size_t>((dy + radius) * size + (dx + radius))] = value;
            sum += value;
        }
    }

    for (float& k : kernel) {
        k /= sum;
    }

    return kernel;
}

float apply_gaussian_at(const std::uint8_t* y_plane,
                        std::uint32_t width,
                        std::uint32_t height,
                        int center_y,
                        int center_x,
                        const std::vector<float>& kernel,
                        int radius)
{
    const int size = 2 * radius + 1;
    float sum = 0.0f;

    for (int dy = -radius; dy <= radius; ++dy) {
        int sy = center_y + dy;
        sy = std::max(0, std::min(static_cast<int>(height) - 1, sy));

        for (int dx = -radius; dx <= radius; ++dx) {
            int sx = center_x + dx;
            sx = std::max(0, std::min(static_cast<int>(width) - 1, sx));

            const float k = kernel[static_cast<std::size_t>((dy + radius) * size + (dx + radius))];
            sum += static_cast<float>(y_plane[static_cast<std::size_t>(sy) * width + static_cast<std::size_t>(sx)]) * k;
        }
    }

    return sum;
}

} // namespace

void sharpen_block::process(const std::uint8_t* in,
                           std::uint8_t* out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const sharpen_config& cfg) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (in == nullptr || out == nullptr || pixels == 0u) {
        return;
    }

    if (!cfg.is_enable) {
        if (in != out) {
            std::copy(in, in + pixels * 3u, out);
        }
        return;
    }

    if (cfg.sharpen_strength == 0) {
        if (in != out) {
            std::copy(in, in + pixels * 3u, out);
        }
        return;
    }

    const std::vector<float> kernel = create_gaussian_kernel(cfg.sharpen_sigma);
    const int radius = static_cast<int>((cfg.sharpen_sigma > 0) ? std::ceil(cfg.sharpen_sigma * 3.0) : 1);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t p = static_cast<std::size_t>(y) * width + x;
            const std::size_t i = p * 3u;

            const float y_lp = apply_gaussian_at(in, width, height,
                                                 static_cast<int>(y), static_cast<int>(x),
                                                 kernel, radius);
            const float y_in = static_cast<float>(in[i]);
            const float y_sharp = y_in + (y_in - y_lp) * static_cast<float>(cfg.sharpen_strength);

            out[i] = clip_to_uint8(y_sharp);
            out[i + 1u] = in[i + 1u];
            out[i + 2u] = in[i + 2u];
        }
    }
}

void sharpen_block::process(const std::vector<std::uint8_t>& in,
                           std::vector<std::uint8_t>& out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const sharpen_config& cfg) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    out.resize(pixels * 3u);

    if (in.size() < pixels * 3u) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
