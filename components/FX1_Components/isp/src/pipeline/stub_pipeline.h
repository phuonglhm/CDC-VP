// SPDX-License-Identifier: Apache-2.0
// M2 TEST-ONLY datapath stub (DEC-21). It has the real Input Formatter
// geometry (HAS Table 6-17) and the real output geometry (RESIZER_OUT_W/H), but
// NO image algorithm: luma = cropped Bayer sample >> 4 with nearest-neighbour
// scaling, chroma = 128. It exists so the DMA engines can be verified before
// the M3 pipeline replaces it; its output is not an ISP result.
#pragma once

#include <cstdint>

namespace cdc::components::fx1_isp::stub {

// Nearest-neighbour source index of output index `o` for an `in` -> `out`
// reduction (out <= in).
inline std::uint32_t source_index(std::uint32_t o, std::uint32_t in, std::uint32_t out) {
   return static_cast<std::uint32_t>(std::uint64_t{o} * in / out);
}

// One luma output line from one input (container) line.
//   in    : input line, `sh + active_width` samples used
//   sh    : Input Formatter horizontal shift (0/1)
//   out   : `out_width` bytes
inline void luma_line(const std::uint16_t *in, std::uint32_t sh, std::uint32_t active_width, std::uint8_t *out,
                      std::uint32_t out_width) {
   for (std::uint32_t ox = 0; ox < out_width; ++ox) {
      const std::uint32_t ix = sh + source_index(ox, active_width, out_width);
      out[ox] = static_cast<std::uint8_t>((in[ix] & 0x0FFFu) >> 4);
   }
}

constexpr std::uint8_t chroma_value = 128;

}  // namespace cdc::components::fx1_isp::stub
