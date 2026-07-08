#include "gc.h"

#include "lut.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace {

// Number of bits to shift the input sample so it lands in the 12-bit
// LUT index range [0, 4095].
//   - input > 12 bit: shift right (downscale)
//   - input < 12 bit: shift left  (upscale)
//   - input == 12 bit: no shift
int shift_amount(std::uint8_t bit_depth)
{
    return static_cast<int>(gc_lut::GAMMA_LUT_BIT_DEPTH) - static_cast<int>(bit_depth);
}

} // namespace

void gc_block::process(const std::uint16_t* in,
                      std::uint16_t* out,
                      std::uint32_t width,
                      std::uint32_t height,
                      const gc_config& cfg) const
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

    const std::size_t lut_size = gc_lut::get_lut_size(); 
    //gc_lut::get_lut_size();
    const std::uint16_t* lut = gc_lut::get_lut();

    if (lut == nullptr || lut_size == 0u) {
        if (in != out) {
            std::copy(in, in + samples, out);
        }
        return;
    }

    // Clamp bit_depth to a sane range so the shift amount stays bounded.
    const std::uint8_t bd = (cfg.bit_depth == 0u || cfg.bit_depth > 16u) ? 12u : cfg.bit_depth;
    const int shift = shift_amount(bd);
    const std::uint16_t max_lut_idx = static_cast<std::uint16_t>(lut_size - 1u);

    for (std::size_t i = 0; i < samples; ++i) {
        std::uint32_t idx;
        if (shift > 0) {
            // input bit_depth < 12: upscale into LUT range
            idx = static_cast<std::uint32_t>(in[i]) << shift;
        } else if (shift < 0) {
            // input bit_depth > 12: downscale into LUT range
            idx = static_cast<std::uint32_t>(in[i]) >> (-shift);
        } else {
            idx = in[i];
        }

        if (idx > max_lut_idx) {
            idx = max_lut_idx;
        }
        out[i] = lut[idx];
    }
}

void gc_block::process(const std::vector<std::uint16_t>& in,
                      std::vector<std::uint16_t>& out,
                      std::uint32_t width,
                      std::uint32_t height,
                      const gc_config& cfg) const
{
    const std::size_t samples = static_cast<std::size_t>(width) * height * 3u;
    out.resize(samples);
    if (in.size() < samples) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
