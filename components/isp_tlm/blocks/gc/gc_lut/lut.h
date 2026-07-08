#pragma once

#include <cstddef>
#include <cstdint>

namespace gc_lut {

// The gamma correction block always uses a 12-bit LUT. Inputs of other
// bit depths are shifted into 12-bit range before lookup.
constexpr std::size_t GAMMA_LUT_12_SIZE = 4096;
constexpr std::uint8_t GAMMA_LUT_BIT_DEPTH = 12;

std::size_t get_lut_size();

const std::uint16_t* get_lut();

} // namespace gc_lut
