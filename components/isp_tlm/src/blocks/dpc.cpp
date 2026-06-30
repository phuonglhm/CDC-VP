#include "dpc.h"
#include "isp_utils.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void dpc_block::process(const uint16_t *in, uint16_t *out, uint32_t w, uint32_t h, const dpc_config &cfg) {
   if (!cfg.is_enable) {
      std::memcpy(out, in, w * h * sizeof(uint16_t));
      return;
   }

   for (int i = 0; i < static_cast<int>(h); ++i) {
      for (int j = 0; j < static_cast<int>(w); ++j) {
         uint16_t P = in[i * w + j];

         // fetch neighbor pixels using global mirror padding
         uint16_t N0 = isp_utils::get_pixel_mirror(in, i - 2, j - 2, w, h); // top left
         uint16_t N1 = isp_utils::get_pixel_mirror(in, i - 2, j, w, h);     // top center
         uint16_t N2 = isp_utils::get_pixel_mirror(in, i - 2, j + 2, w, h); // top right
         uint16_t N3 = isp_utils::get_pixel_mirror(in, i, j - 2, w, h);     // left center
         uint16_t N4 = isp_utils::get_pixel_mirror(in, i, j + 2, w, h);     // right center
         uint16_t N5 = isp_utils::get_pixel_mirror(in, i + 2, j - 2, w, h); // bot left
         uint16_t N6 = isp_utils::get_pixel_mirror(in, i + 2, j, w, h);     // bot center
         uint16_t N7 = isp_utils::get_pixel_mirror(in, i + 2, j + 2, w, h); // bot right

         // range check
         uint16_t n_min = N0;
         if (N1 < n_min)
            n_min = N1;
         if (N2 < n_min)
            n_min = N2;
         if (N3 < n_min)
            n_min = N3;
         if (N4 < n_min)
            n_min = N4;
         if (N5 < n_min)
            n_min = N5;
         if (N6 < n_min)
            n_min = N6;
         if (N7 < n_min)
            n_min = N7;

         uint16_t n_max = N0;
         if (N1 > n_max)
            n_max = N1;
         if (N2 > n_max)
            n_max = N2;
         if (N3 > n_max)
            n_max = N3;
         if (N4 > n_max)
            n_max = N4;
         if (N5 > n_max)
            n_max = N5;
         if (N6 > n_max)
            n_max = N6;
         if (N7 > n_max)
            n_max = N7;

         bool cond1 = (P < n_min) || (P > n_max);

         // threshold check
         bool cond2 = (std::abs(static_cast<int>(P) - N0) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N1) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N2) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N3) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N4) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N5) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N6) > cfg.dp_threshold) &&
                      (std::abs(static_cast<int>(P) - N7) > cfg.dp_threshold);

         // dead pixel correction
         if (cond1 && cond2) {
            int32_t G_v = std::abs(2 * static_cast<int32_t>(P) - N1 - N6);  // vertical
            int32_t G_h = std::abs(2 * static_cast<int32_t>(P) - N3 - N4);  // horizontal
            int32_t G_ld = std::abs(2 * static_cast<int32_t>(P) - N2 - N5); // left diagonal
            int32_t G_rd = std::abs(2 * static_cast<int32_t>(P) - N0 - N7); // right diagonal

            int32_t min_grad = G_v;
            if (G_h < min_grad)
               min_grad = G_h;
            if (G_ld < min_grad)
               min_grad = G_ld;
            if (G_rd < min_grad)
               min_grad = G_rd;

            // tie breaking
            uint16_t corrected = P;
            if (min_grad == G_v) {
               corrected = (static_cast<uint32_t>(N1) + N6) / 2;
            } else if (min_grad == G_h) {
               corrected = (static_cast<uint32_t>(N3) + N4) / 2;
            } else if (min_grad == G_ld) {
               corrected = (static_cast<uint32_t>(N2) + N5) / 2;
            } else if (min_grad == G_rd) {
               corrected = (static_cast<uint32_t>(N0) + N7) / 2;
            }
            out[i * w + j] = corrected;
         } else {
            out[i * w + j] = P;
         }
      }
   }
}
