#include "ccm.h"

#include <algorithm>
#include <cmath>

namespace {

std::uint16_t max_for_bit_depth(std::uint8_t bit_depth)
{
    if (bit_depth == 0) {
        bit_depth = 8;
    }
    if (bit_depth >= 16) {
        return 0xFFFFu;
    }
    return static_cast<std::uint16_t>((1u << bit_depth) - 1u);
}

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

std::uint16_t quantize(float value, std::uint16_t max_value)
{
    const float scaled = clamp01(value) * static_cast<float>(max_value);
    return static_cast<std::uint16_t>(std::lround(scaled));
}

} // namespace

void ccm_block::process(const std::uint16_t* in,
                        std::uint16_t* out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const ccm_config& cfg,
                        std::uint8_t bit_depth) const
{
    const std::size_t samples = static_cast<std::size_t>(width) * height * 3u;
    if (in == nullptr || out == nullptr || samples == 0u) {
        return;
    }

    if (!cfg.is_enable) {
        if (in != out) {
            std::copy(in, in + samples, out);
        }
        return;
    }

    const std::uint16_t max_value = max_for_bit_depth(bit_depth);
    const float inv_max = 1.0f / static_cast<float>(max_value);

    for (std::size_t i = 0; i < samples; i += 3u) {
        const float rgb[3] = {
            static_cast<float>(std::min(in[i], max_value)) * inv_max,
            static_cast<float>(std::min(in[i + 1u], max_value)) * inv_max,
            static_cast<float>(std::min(in[i + 2u], max_value)) * inv_max,
        };

        out[i]     = quantize(cfg.corrected_red[0]   * rgb[0] +
                             cfg.corrected_red[1]   * rgb[1] +
                             cfg.corrected_red[2]   * rgb[2], max_value);
        out[i + 1u] = quantize(cfg.corrected_green[0] * rgb[0] +
                              cfg.corrected_green[1] * rgb[1] +
                              cfg.corrected_green[2] * rgb[2], max_value);
        out[i + 2u] = quantize(cfg.corrected_blue[0]  * rgb[0] +
                              cfg.corrected_blue[1]  * rgb[1] +
                              cfg.corrected_blue[2]  * rgb[2], max_value);
    }
}

void ccm_block::process(const std::vector<std::uint16_t>& in,
                        std::vector<std::uint16_t>& out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const ccm_config& cfg,
                        std::uint8_t bit_depth) const
{
    const std::size_t samples = static_cast<std::size_t>(width) * height * 3u;
    out.resize(samples);
    if (in.size() < samples) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg, bit_depth);
}
