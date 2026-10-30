// SPDX-License-Identifier: Apache-2.0
#include "dma/dma_rules.h"

#include <algorithm>

namespace cdc::components::fx1_isp::dma {

std::vector<burst> split_line(std::uint64_t addr, std::uint32_t bytes, std::uint32_t max_beats,
                              std::uint32_t beat_bytes) {
   std::vector<burst> out;
   const std::uint64_t max_chunk = std::uint64_t{max_beats} * beat_bytes;
   std::uint64_t remaining = bytes;
   while (remaining > 0) {
      const std::uint64_t to_4k = 4096u - (addr % 4096u);
      const auto chunk = static_cast<std::uint32_t>(std::min({remaining, max_chunk, to_4k}));
      out.push_back({addr, chunk});
      addr += chunk;
      remaining -= chunk;
   }
   return out;
}

extent active_extent(std::uint32_t width, std::uint32_t height, std::uint32_t pattern) {
   const std::uint32_t sh = pattern & 1u, sv = (pattern >> 1) & 1u;
   return {width > sh ? (width - sh) & ~1u : 0u, height > sv ? (height - sv) & ~1u : 0u};
}

bool active_geometry_legal(std::uint32_t width, std::uint32_t height, std::uint32_t pattern) {
   const extent a = active_extent(width, height, pattern);
   return a.width >= min_active && a.width <= max_active_w && a.height >= min_active && a.height <= max_active_h;
}

bool frame_geometry_ok(std::uint32_t width, std::uint32_t height) {
   return width % 2u == 0 && width >= 2 && width <= max_h_active && height % 2u == 0 && height >= 2 &&
          height <= max_v_active;
}

bool idma_setup_ok(const idma_setup &s, std::uint32_t beat_bytes) {
   return s.depth <= 1u && frame_geometry_ok(s.width, s.height) && s.base % beat_bytes == 0 && s.stride % beat_bytes == 0 &&
          std::uint64_t{s.stride} >= 2u * std::uint64_t{s.width};
}

bool odma_setup_ok(const odma_setup &s, std::uint32_t beat_bytes) {
   if (s.rgb) {  // CH2_ODMA_Y_STRIDE >= 3W (MAS v1.07 Table 6-153)
      return frame_geometry_ok(s.width, s.height) && s.y_base % beat_bytes == 0 && s.y_stride % beat_bytes == 0 &&
             s.y_stride >= 3u * s.width;
   }
   return frame_geometry_ok(s.width, s.height) && s.y_base % beat_bytes == 0 && s.uv_base % beat_bytes == 0 &&
          s.y_stride % beat_bytes == 0 && s.uv_stride % beat_bytes == 0 && s.y_stride >= s.width &&
          s.uv_stride >= s.width;
}

}  // namespace cdc::components::fx1_isp::dma
