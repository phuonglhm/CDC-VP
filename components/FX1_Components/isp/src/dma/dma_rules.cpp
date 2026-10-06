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

bool frame_geometry_ok(std::uint32_t width, std::uint32_t height) {
   return width % 2u == 0 && width >= 2 && width <= max_h_active && height % 2u == 0 && height >= 2 &&
          height <= max_v_active;
}

bool idma_setup_ok(const idma_setup &s, std::uint32_t beat_bytes) {
   return frame_geometry_ok(s.width, s.height) && s.base % beat_bytes == 0 && s.stride % beat_bytes == 0 &&
          std::uint64_t{s.stride} >= 2u * std::uint64_t{s.width};
}

bool odma_setup_ok(const odma_setup &s, std::uint32_t beat_bytes) {
   return frame_geometry_ok(s.width, s.height) && s.y_base % beat_bytes == 0 && s.uv_base % beat_bytes == 0 &&
          s.y_stride % beat_bytes == 0 && s.uv_stride % beat_bytes == 0 && s.y_stride >= s.width &&
          s.uv_stride >= s.width;
}

}  // namespace cdc::components::fx1_isp::dma
