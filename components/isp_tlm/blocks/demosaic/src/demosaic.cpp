#include "demosaic.h"
#include "isp_utils.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

void demosaic_block::process(const uint16_t *in,
                             uint16_t *out,
                             uint32_t w,
                             uint32_t h,
                             const demosaic_config &cfg,
                             cfa_types bayer_pattern,
                             uint8_t bit_depth) {
   if (!cfg.is_enable) {
      // bypass mode: copy raw values directly into r, g, b channels
      for (uint32_t idx = 0; idx < w * h; ++idx) {
         out[3 * idx + 0] = in[idx];
         out[3 * idx + 1] = in[idx];
         out[3 * idx + 2] = in[idx];
      }
      return;
   }

   uint32_t bit_range = (1u << bit_depth) - 1;

   for (int r = 0; r < static_cast<int>(h); ++r) {
      bool is_even_row = (r & 1) == 0;
      for (int c = 0; c < static_cast<int>(w); ++c) {
         bool is_even_col = (c & 1) == 0;
         uint32_t out_idx = 3 * (r * w + c);

         float W[5][5];
         for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
               W[i][j] = isp_utils::get_pixel_mirror(in, r + i - 2, c + j - 2, w, h);
            }
         }

         // determine bayer channel at center pixel W[2][2]
         bayer_channel channel = bayer_channel::R;
         switch (bayer_pattern) {
         case cfa_types::RGGB:
            if (is_even_row) {
               channel = is_even_col ? bayer_channel::R : bayer_channel::GR;
            } else {
               channel = is_even_col ? bayer_channel::GB : bayer_channel::B;
            }
            break;
         case cfa_types::GRBG:
            if (is_even_row) {
               channel = is_even_col ? bayer_channel::GR : bayer_channel::R;
            } else {
               channel = is_even_col ? bayer_channel::B : bayer_channel::GB;
            }
            break;
         case cfa_types::BGGR:
            if (is_even_row) {
               channel = is_even_col ? bayer_channel::B : bayer_channel::GB;
            } else {
               channel = is_even_col ? bayer_channel::GR : bayer_channel::R;
            }
            break;
         case cfa_types::GBRG:
            if (is_even_row) {
               channel = is_even_col ? bayer_channel::GB : bayer_channel::B;
            } else {
               channel = is_even_col ? bayer_channel::R : bayer_channel::GR;
            }
            break;
         }

         float rout = 0.0f, gout = 0.0f, bout = 0.0f;

         if (channel == bayer_channel::R) {
            // red center pixel
            rout = W[2][2];
            gout = (4.0f * W[2][2] - W[0][2] - W[2][0] - W[4][2] - W[2][4] +
                    2.0f * (W[1][2] + W[3][2] + W[2][1] + W[2][3])) /
                   8.0f;
            bout = (6.0f * W[2][2] - 1.5f * (W[0][2] + W[2][0] + W[4][2] + W[2][4]) +
                    2.0f * (W[1][1] + W[1][3] + W[3][1] + W[3][3])) /
                   8.0f;
         } else if (channel == bayer_channel::B) {
            // blue center pixel
            bout = W[2][2];
            gout = (4.0f * W[2][2] - W[0][2] - W[2][0] - W[4][2] - W[2][4] +
                    2.0f * (W[1][2] + W[3][2] + W[2][1] + W[2][3])) /
                   8.0f;
            rout = (6.0f * W[2][2] - 1.5f * (W[0][2] + W[2][0] + W[4][2] + W[2][4]) +
                    2.0f * (W[1][1] + W[1][3] + W[3][1] + W[3][3])) /
                   8.0f;
         } else if (channel == bayer_channel::GR) {
            // green center pixel on red row (gr)
            gout = W[2][2];
            rout = (5.0f * W[2][2] - W[2][0] - W[1][1] - W[3][1] - W[1][3] - W[3][3] - W[2][4] +
                    0.5f * (W[0][2] + W[4][2]) + 4.0f * (W[2][1] + W[2][3])) /
                   8.0f;
            bout = (5.0f * W[2][2] - W[0][2] - W[1][1] - W[1][3] - W[4][2] - W[3][1] - W[3][3] +
                    0.5f * (W[2][0] + W[2][4]) + 4.0f * (W[1][2] + W[3][2])) /
                   8.0f;
         } else if (channel == bayer_channel::GB) {
            // green center pixel on blue row (gb)
            gout = W[2][2];
            bout = (5.0f * W[2][2] - W[2][0] - W[1][1] - W[3][1] - W[1][3] - W[3][3] - W[2][4] +
                    0.5f * (W[0][2] + W[4][2]) + 4.0f * (W[2][1] + W[2][3])) /
                   8.0f;
            rout = (5.0f * W[2][2] - W[0][2] - W[1][1] - W[1][3] - W[4][2] - W[3][1] - W[3][3] +
                    0.5f * (W[2][0] + W[2][4]) + 4.0f * (W[1][2] + W[3][2])) /
                   8.0f;
         }

         out[out_idx + 0] = static_cast<uint16_t>(std::clamp(rout, 0.0f, static_cast<float>(bit_range)));
         out[out_idx + 1] = static_cast<uint16_t>(std::clamp(gout, 0.0f, static_cast<float>(bit_range)));
         out[out_idx + 2] = static_cast<uint16_t>(std::clamp(bout, 0.0f, static_cast<float>(bit_range)));
      }
   }
}
