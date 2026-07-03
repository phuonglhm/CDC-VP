#include "2dnr.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t WEIGHT_LUT_SIZE = 256;

std::vector<float> build_weight_lut(std::uint16_t wts)
{
    std::vector<float> lut(WEIGHT_LUT_SIZE);
    if (wts == 0) {
        lut[0] = 1.0f;
        for (std::size_t i = 1; i < WEIGHT_LUT_SIZE; ++i) {
            lut[i] = 0.0f;
        }
        return lut;
    }

    for (std::size_t i = 0; i < WEIGHT_LUT_SIZE; ++i) {
        lut[i] = static_cast<float>(std::exp(-static_cast<double>(i) / static_cast<double>(wts)));
    }
    return lut;
}

std::vector<float> build_patch_gaussian_kernel(std::uint8_t patch_size)
{
    const int radius = static_cast<int>(patch_size) / 2;
    const int size = static_cast<int>(patch_size);
    std::vector<float> kernel(static_cast<std::size_t>(size * size));

    float sigma = static_cast<float>(patch_size) / 6.0f;
    if (sigma < 0.5f) sigma = 0.5f;
    const float sigma_sq = sigma * sigma;
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

float compute_patch_distance(const std::uint8_t* y_work,
                            std::uint32_t width,
                            std::uint32_t height,
                            int py,
                            int px,
                            int sy,
                            int sx,
                            const std::vector<float>& patch_gaussian,
                            int patch_radius)
{
    float dist = 0.0f;

    for (int dy = -patch_radius; dy <= patch_radius; ++dy) {
        int ry = py + dy;
        ry = std::max(0, std::min(static_cast<int>(height) - 1, ry));
        int ry2 = sy + dy;
        ry2 = std::max(0, std::min(static_cast<int>(height) - 1, ry2));

        for (int dx = -patch_radius; dx <= patch_radius; ++dx) {
            int rx = px + dx;
            rx = std::max(0, std::min(static_cast<int>(width) - 1, rx));
            int rx2 = sx + dx;
            rx2 = std::max(0, std::min(static_cast<int>(width) - 1, rx2));

            const int diff = static_cast<int>(y_work[static_cast<std::size_t>(ry) * width + static_cast<std::size_t>(rx)]) -
                             static_cast<int>(y_work[static_cast<std::size_t>(ry2) * width + static_cast<std::size_t>(rx2)]);
            const float weighted_diff_sq = static_cast<float>(diff * diff) *
                                          patch_gaussian[static_cast<std::size_t>((dy + patch_radius) * static_cast<int>(patch_radius * 2 + 1) + (dx + patch_radius))];
            dist += weighted_diff_sq;
        }
    }

    const std::size_t patch_area = static_cast<std::size_t>(patch_radius * 2 + 1) * static_cast<std::size_t>(patch_radius * 2 + 1);
    return dist / static_cast<float>(patch_area);
}

std::uint8_t get_pixel_clamped(const std::uint8_t* buf, std::uint32_t width, std::uint32_t height, int y, int x)
{
    y = std::max(0, std::min(static_cast<int>(height) - 1, y));
    x = std::max(0, std::min(static_cast<int>(width) - 1, x));
    return buf[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
}

} // namespace

void twodnr_block::process(const std::uint8_t* in,
                           std::uint8_t* out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const twodnr_config& cfg) const
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

    if (cfg.window_size < 3 || cfg.patch_size < 3) {
        if (in != out) {
            std::copy(in, in + pixels * 3u, out);
        }
        return;
    }

    const std::vector<float> weight_lut = build_weight_lut(cfg.wts);
    const std::vector<float> patch_gaussian = build_patch_gaussian_kernel(cfg.patch_size);

    const int patch_radius = static_cast<int>(cfg.patch_size) / 2;
    const int window_radius = static_cast<int>(cfg.window_size) / 2;

    const std::size_t lut_max_idx = WEIGHT_LUT_SIZE - 1;

    for (std::uint32_t py = 0; py < height; ++py) {
        for (std::uint32_t px = 0; px < width; ++px) {
            float weighted_sum = 0.0f;
            float weight_sum = 0.0f;

            for (int wy = -window_radius; wy <= window_radius; ++wy) {
                for (int wx = -window_radius; wx <= window_radius; ++wx) {
                    const int sy = static_cast<int>(py) + wy;
                    const int sx = static_cast<int>(px) + wx;

                    if (sy < 0 || sy >= static_cast<int>(height) || sx < 0 || sx >= static_cast<int>(width)) {
                        continue;
                    }

                    const float dist = compute_patch_distance(in, width, height,
                                                               static_cast<int>(py), static_cast<int>(px),
                                                               sy, sx,
                                                               patch_gaussian, patch_radius);

                    const std::size_t lut_idx = static_cast<std::size_t>(std::min<float>(dist, static_cast<float>(lut_max_idx)));
                    const float weight = weight_lut[lut_idx];

                    const std::uint8_t search_pixel = get_pixel_clamped(in, width, height, sy, sx);
                    weighted_sum += static_cast<float>(search_pixel) * weight;
                    weight_sum += weight;
                }
            }

            const std::size_t p = static_cast<std::size_t>(py) * width + static_cast<std::size_t>(px);
            const std::size_t i = p * 3u;

            if (weight_sum > 0.0f) {
                const float y_denoised = weighted_sum / weight_sum;
                out[i] = static_cast<std::uint8_t>(std::lround(std::max(0.0f, std::min(255.0f, y_denoised))));
            } else {
                out[i] = in[i];
            }

            out[i + 1u] = in[i + 1u];
            out[i + 2u] = in[i + 2u];
        }
    }
}

void twodnr_block::process(const std::vector<std::uint8_t>& in,
                           std::vector<std::uint8_t>& out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const twodnr_config& cfg) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    out.resize(pixels * 3u);

    if (in.size() < pixels * 3u) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
