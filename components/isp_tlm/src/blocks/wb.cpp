#include "blocks/wb.h"

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

std::uint16_t scale_and_clip(std::uint16_t value, float gain, std::uint16_t max_value)
{
    const float scaled = static_cast<float>(value) * gain;
    if (scaled <= 0.0f) {
        return 0;
    }
    if (scaled >= static_cast<float>(max_value)) {
        return max_value;
    }
    return static_cast<std::uint16_t>(std::lround(scaled));
}

} // namespace

void wb_block::process(const std::uint16_t* in,
                       std::uint16_t* out,
                       std::uint32_t width,
                       std::uint32_t height,
                       const wb_config& cfg) const
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

    const std::uint16_t max_value = max_for_bit_depth(cfg.bit_depth);
    for (std::size_t i = 0; i < samples; i += 3u) {
        out[i] = scale_and_clip(in[i], cfg.r_gain, max_value);
        out[i + 1u] = std::min(in[i + 1u], max_value);
        out[i + 2u] = scale_and_clip(in[i + 2u], cfg.b_gain, max_value);
    }
}

void wb_block::process(const std::vector<std::uint16_t>& in,
                       std::vector<std::uint16_t>& out,
                       std::uint32_t width,
                       std::uint32_t height,
                       const wb_config& cfg) const
{
    const std::size_t samples = static_cast<std::size_t>(width) * height * 3u;
    out.resize(samples);
    if (in.size() < samples) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
