#include "awb.h"

#include <algorithm>
#include <cmath>

void awb_block::process(const std::uint16_t* rgb_in,
                        std::uint32_t width,
                        std::uint32_t height,
                        awb_config& cfg,
                        std::uint8_t bit_depth) const
{
    if (rgb_in == nullptr || width == 0 || height == 0) {
        return;
    }

    if (!cfg.is_enable) {
        cfg.r_gain_out = 1.0f;
        cfg.b_gain_out = 1.0f;
        return;
    }

    const std::uint32_t max_value = (1u << bit_depth) - 1;
    const float under_thresh = max_value * cfg.underexposed_percentage;
    const float over_thresh = max_value * (1.0f - cfg.overexposed_percentage);

    const std::size_t num_pixels = static_cast<std::size_t>(width) * height;

    std::uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
    std::uint32_t count = 0;

    for (std::size_t i = 0; i < num_pixels; ++i) {
        const std::uint16_t r = rgb_in[i * 3 + 0];
        const std::uint16_t g = rgb_in[i * 3 + 1];
        const std::uint16_t b = rgb_in[i * 3 + 2];

        // Skip over/underexposed pixels to avoid skewing the statistics
        const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        if (lum < under_thresh || lum > over_thresh) {
            continue;
        }

        sum_r += r;
        sum_g += g;
        sum_b += b;
        ++count;
    }

    if (count == 0) {
        cfg.r_gain_out = 1.0f;
        cfg.b_gain_out = 1.0f;
        return;
    }

    const float avg_r = static_cast<float>(sum_r) / count;
    const float avg_g = static_cast<float>(sum_g) / count;
    const float avg_b = static_cast<float>(sum_b) / count;

    const float avg_r_norm = avg_r / static_cast<float>(max_value);
    const float avg_g_norm = avg_g / static_cast<float>(max_value);
    const float avg_b_norm = avg_b / static_cast<float>(max_value);

    if (avg_r_norm > 0.01f && avg_g_norm > 0.01f && avg_b_norm > 0.01f) {
        cfg.r_gain_out = avg_g_norm / avg_r_norm;
        cfg.b_gain_out = avg_g_norm / avg_b_norm;

        cfg.r_gain_out = std::clamp(cfg.r_gain_out, 0.25f, 4.0f);
        cfg.b_gain_out = std::clamp(cfg.b_gain_out, 0.25f, 4.0f);
    } else {
        cfg.r_gain_out = 1.0f;
        cfg.b_gain_out = 1.0f;
    }
}
