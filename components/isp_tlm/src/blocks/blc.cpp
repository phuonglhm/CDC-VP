#include "blc.h"
#include "isp_types.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

void blc_block::process(const uint16_t *in,
                        uint16_t *out,
                        uint32_t w,
                        uint32_t h,
                        const blc_config &cfg,
                        cfa_types bayer_pattern,
                        uint8_t bit_depth) {
   if (!cfg.is_enable) {
      std::memcpy(out, in, w * h * sizeof(uint16_t));
      return;
   }

   uint32_t bit_range = pow(2, bit_depth) - 1;

   uint16_t b_sat_diff = (cfg.b_sat > cfg.b_offset) ? (cfg.b_sat - cfg.b_offset) : 1;
   uint16_t gb_sat_diff = (cfg.gb_sat > cfg.gb_offset) ? (cfg.gb_sat - cfg.gb_offset) : 1;
   uint16_t r_sat_diff = (cfg.r_sat > cfg.r_offset) ? (cfg.r_sat - cfg.r_offset) : 1;
   uint16_t gr_sat_diff = (cfg.gr_sat > cfg.gr_offset) ? (cfg.gr_sat - cfg.gr_offset) : 1;

   for (uint32_t i = 0; i < h; ++i) {
      bool is_even_row = (i & 1) == 0;
      for (uint32_t j = 0; j < w; ++j) {
         bool is_even_col = (j & 1) == 0;
         uint32_t ind = i * w + j;

         uint16_t offset = 0;
         uint16_t sat_diff = 1;

         switch (bayer_pattern) {
         case cfa_types::BGGR:
            if (is_even_row) {
               if (is_even_col) {
                  offset = cfg.b_offset;
                  sat_diff = b_sat_diff;
               } else {
                  offset = cfg.gb_offset;
                  sat_diff = gb_sat_diff;
               }
            } else {
               if (is_even_col) {
                  offset = cfg.gr_offset;
                  sat_diff = gr_sat_diff;
               } else {
                  offset = cfg.r_offset;
                  sat_diff = r_sat_diff;
               }
            }
            break;

         case cfa_types::GBRG:
            if (is_even_row) {
               if (is_even_col) {
                  offset = cfg.gb_offset;
                  sat_diff = gb_sat_diff;
               } else {
                  offset = cfg.b_offset;
                  sat_diff = b_sat_diff;
               }
            } else {
               if (is_even_col) {
                  offset = cfg.r_offset;
                  sat_diff = r_sat_diff;
               } else {
                  offset = cfg.gr_offset;
                  sat_diff = gr_sat_diff;
               }
            }
            break;

         case cfa_types::GRBG:
            if (is_even_row) {
               if (is_even_col) {
                  offset = cfg.gr_offset;
                  sat_diff = gr_sat_diff;
               } else {
                  offset = cfg.r_offset;
                  sat_diff = r_sat_diff;
               }
            } else {
               if (is_even_col) {
                  offset = cfg.b_offset;
                  sat_diff = b_sat_diff;
               } else {
                  offset = cfg.gb_offset;
                  sat_diff = gb_sat_diff;
               }
            }
            break;

         case cfa_types::RGGB:
            if (is_even_row) {
               if (is_even_col) {
                  offset = cfg.r_offset;
                  sat_diff = r_sat_diff;
               } else {
                  offset = cfg.gr_offset;
                  sat_diff = gr_sat_diff;
               }
            } else {
               if (is_even_col) {
                  offset = cfg.gb_offset;
                  sat_diff = gb_sat_diff;
               } else {
                  offset = cfg.b_offset;
                  sat_diff = b_sat_diff;
               }
            }
            break;
         }

         int32_t val = (int32_t)in[ind] - offset;
         if (val < 0)
            val = 0;

         if (cfg.is_linear) {
            val = (double)val / sat_diff * bit_range;
         }

         if (val > (int32_t)bit_range)
            val = bit_range;

         out[ind] = (uint16_t)val;
      }
   }
}
