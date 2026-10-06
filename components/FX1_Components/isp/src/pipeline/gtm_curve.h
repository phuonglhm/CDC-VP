// SPDX-License-Identifier: Apache-2.0
// GTM curve generator (HAS §6.16.4.3, Table 6-48): the integer Reinhard curve
// built at the end of each frame from the previous frame's metered luma.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace cdc::components::fx1_isp::pipe {

using gtm_curve = std::array<std::uint16_t, 65>;  // UQ8.8 ratios, point i covers luma 4*i

// round(2^16 / Lw^2), Lw = lwhite / 256, saturated to 16 bits (ALG-GTM-01).
inline std::uint32_t gtm_lw2_recip(std::uint32_t lwhite) {
   if (lwhite == 0) {
      return 0xFFFFu;
   }
   const std::uint64_t sq = std::uint64_t{lwhite} * lwhite;
   return static_cast<std::uint32_t>(std::min<std::uint64_t>(((std::uint64_t{1} << 32) + sq / 2) / sq, 0xFFFFu));
}

inline gtm_curve gtm_build_curve(std::uint32_t yavg, std::uint32_t key, std::uint32_t lwhite) {
   gtm_curve r{};
   const std::uint64_t recip = gtm_lw2_recip(lwhite);
   const std::uint64_t avg = std::max<std::uint32_t>(yavg, 1u);
   for (std::uint32_t i = 1; i <= 64; ++i) {
      const std::uint64_t yi = 4u * i;
      const std::uint64_t l = std::min<std::uint64_t>(std::uint64_t{key} * yi * 256u / avg, 0xFFFFFFu);  // sat24
      const std::uint64_t l2 = std::min<std::uint64_t>((l * l) >> 16, 0xFFFFFFu);                      // sat24
      const std::uint64_t num = l + ((l2 * recip) >> 16);
      const std::uint64_t den = 65536u + l;
      const std::uint64_t ld = (num << 16) / den;                     // full width (ALG-GTM-02)
      const std::uint64_t out = std::min<std::uint64_t>((ld * 255u + 32768u) >> 16, 255u);
      r[i] = static_cast<std::uint16_t>(std::min<std::uint64_t>((out * 256u + (yi >> 1)) / yi, 0xFFFFu));
   }
   r[0] = r[1];
   return r;
}

}  // namespace cdc::components::fx1_isp::pipe
