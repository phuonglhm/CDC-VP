#include "gc.h"

#include <algorithm>
#include <cstddef>

namespace {

std::size_t get_lut_size_for_bit_depth(std::uint8_t bit_depth)
{
    switch (bit_depth) {
    case 8:  return 256u;
    case 10: return 1024u;
    case 12: return 4096u;
    case 14: return 16384u;
    default: return 0u;
    }
}

const std::uint16_t* get_active_lut(const gc_config& cfg, std::uint8_t bit_depth, std::size_t& lut_size)
{
    lut_size = get_lut_size_for_bit_depth(bit_depth);

    switch (bit_depth) {
    case 8:
        if (!cfg.gamma_lut_8.empty()) {
            return cfg.gamma_lut_8.data();
        }
        break;
    case 10:
        if (!cfg.gamma_lut_10.empty()) {
            return cfg.gamma_lut_10.data();
        }
        break;
    case 12:
        if (!cfg.gamma_lut_12.empty()) {
            return cfg.gamma_lut_12.data();
        }
        break;
    case 14:
        if (!cfg.gamma_lut_14.empty()) {
            return cfg.gamma_lut_14.data();
        }
        break;
    default:
        break;
    }

    lut_size = 0;
    return nullptr;
}

} // namespace

void gc_block::process(const std::uint16_t* in,
                       std::uint16_t* out,
                       std::uint32_t width,
                       std::uint32_t height,
                       const gc_config& cfg,
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

    std::size_t lut_size = 0;
    const std::uint16_t* lut = get_active_lut(cfg, bit_depth, lut_size);

    if (lut == nullptr || lut_size == 0) {
        if (in != out) {
            std::copy(in, in + samples, out);
        }
        return;
    }

    const std::uint16_t max_lut_idx = static_cast<std::uint16_t>(lut_size - 1u);

    for (std::size_t i = 0; i < samples; ++i) {
        const std::uint16_t idx = std::min(in[i], max_lut_idx);
        out[i] = lut[idx];
    }
}

void gc_block::process(const std::vector<std::uint16_t>& in,
                       std::vector<std::uint16_t>& out,
                       std::uint32_t width,
                       std::uint32_t height,
                       const gc_config& cfg,
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
