#include "gc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

std::size_t get_lut_size_for_bit_depth(std::uint8_t bit_depth)
{
    switch (bit_depth) {
    case 8:  return 256u;
    case 10: return 1024u;
    case 12: return 4096u;
    case 14: return 16384u;
    case 16: return 65536u;
    default: return 0u;
    }
}

// Returns a pointer to the user-supplied LUT matching bit_depth, or nullptr if
// none was provided. lut_size_out is always set to the bit-depth table size
// regardless of which LUT (if any) the user gave us.
const std::uint16_t* get_user_lut(const gc_config& cfg, std::uint8_t bit_depth, std::size_t& lut_size_out)
{
    lut_size_out = get_lut_size_for_bit_depth(bit_depth);
    if (lut_size_out == 0u) {
        return nullptr;
    }

    const std::vector<std::uint16_t>* user_lut = nullptr;
    switch (bit_depth) {
    case 8:  user_lut = &cfg.gamma_lut_8;  break;
    case 10: user_lut = &cfg.gamma_lut_10; break;
    case 12: user_lut = &cfg.gamma_lut_12; break;
    case 14: user_lut = &cfg.gamma_lut_14; break;
    case 16: user_lut = &cfg.gamma_lut_16; break;
    default: return nullptr;
    }

    if (user_lut != nullptr && user_lut->size() == lut_size_out) {
        return user_lut->data();
    }
    return nullptr;
}

} // namespace

std::vector<std::uint16_t> build_gamma_lut(std::uint8_t bit_depth, float gamma)
{
    std::vector<std::uint16_t> lut;
    const std::size_t size = get_lut_size_for_bit_depth(bit_depth);
    if (size == 0u) {
        return lut;
    }

    lut.resize(size);
    const float    max_in  = static_cast<float>((1u << bit_depth) - 1u);
    const std::uint32_t max_out = (1u << bit_depth) - 1u;

    // A gamma exponent of <= 0 (or NaN) would corrupt the LUT. Treat as pass-through.
    const bool gamma_valid = std::isfinite(gamma) && gamma > 0.0f;
    const float g = gamma_valid ? gamma : 1.0f;

    for (std::size_t i = 0; i < size; ++i) {
        const float    n  = static_cast<float>(i) / max_in;          // normalize to [0,1]
        const float    cv = std::pow(n, g);                          // apply gamma
        const std::uint32_t v = static_cast<std::uint32_t>(cv * max_out + 0.5f);
        lut[i] = static_cast<std::uint16_t>(v > max_out ? max_out : v);
    }
    return lut;
}

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

    std::size_t        lut_size = 0;
    const std::uint16_t* lut = get_user_lut(cfg, bit_depth, lut_size);

    // If the user enabled GC but didn't supply a matching LUT, fall back to a
    // synthesized default (γ = cfg.default_gamma, typically 2.2). This makes
    // enabling GC actually do something instead of silently copying input to
    // output.
    std::vector<std::uint16_t> default_lut;
    if (lut == nullptr) {
        if (lut_size == 0u) {
            // Unsupported bit depth — be safe and pass through.
            if (in != out) {
                std::copy(in, in + samples, out);
            }
            return;
        }
        default_lut = build_gamma_lut(bit_depth, cfg.default_gamma);
        lut = default_lut.data();
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
