#pragma once

#include <cstdint>
#include <vector>

struct gc_config {
    bool is_enable = false;
    std::vector<std::uint16_t> gamma_lut_8;
    std::vector<std::uint16_t> gamma_lut_10;
    std::vector<std::uint16_t> gamma_lut_12;
    std::vector<std::uint16_t> gamma_lut_14;
    std::vector<std::uint16_t> gamma_lut_16;
    // Default gamma exponent (≥ 1.0) applied when is_enable is true and the user
    // did not supply a per-bit-depth LUT. Defaults to the standard sRGB-style 2.2
    // value. Set to 1.0 to make the default LUT a pure pass-through.
    float default_gamma = 2.2f;
};

// Build a gamma-correction LUT for a given bit depth, sized to (1 << bit_depth).
// Used both as the auto-fallback inside gc_block::process and as a public utility
// for users who want to inspect/pre-fill a LUT before writing it back into
// gc_config.
std::vector<std::uint16_t> build_gamma_lut(std::uint8_t bit_depth, float gamma);

class gc_block {
public:
    void process(const std::uint16_t* in,
                 std::uint16_t* out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg,
                 std::uint8_t bit_depth) const;

    void process(const std::vector<std::uint16_t>& in,
                 std::vector<std::uint16_t>& out,
                 std::uint32_t width,
                 std::uint32_t height,
                 const gc_config& cfg,
                 std::uint8_t bit_depth) const;
};
