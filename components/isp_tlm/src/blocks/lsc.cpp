#include "lsc.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

enum class bayer_channel { R = 0, GR = 1, GB = 2, B = 3 };

static inline float bilinear_interpolate(float G00, float G10, float G01, float G11, float dx, float dy) {
   return (1.0f - dx) * (1.0f - dy) * G00 + dx * (1.0f - dy) * G10 + (1.0f - dx) * dy * G01 + dx * dy * G11;
}

void lsc_block::process(const uint16_t *in,
                        uint16_t *out,
                        uint32_t w,
                        uint32_t h,
                        const lsc_config &cfg,
                        const float *lsc_mem_ptr,
                        cfa_types bayer_pattern,
                        uint8_t bit_depth) {
   if (cfg.is_enable) {
      if (lsc_mem_ptr == nullptr) {
         std::cerr << "lsc error: lsc_mem_ptr is null\n";
         std::memcpy(out, in, w * h * sizeof(uint16_t));
         return;
      }
      if (cfg.grid_width == 0 || cfg.grid_height == 0) {
         std::cerr << "lsc error: grid width or height is zero\n";
         std::memcpy(out, in, w * h * sizeof(uint16_t));
         return;
      }
   } else {
      std::memcpy(out, in, w * h * sizeof(uint16_t));
      return;
   }

   float box_w = static_cast<float>(w) / cfg.grid_width;
   float box_h = static_cast<float>(h) / cfg.grid_height;
   uint32_t nodes_per_channel = (cfg.grid_width + 1) * (cfg.grid_height + 1);
   float bit_range = static_cast<float>((1u << bit_depth) - 1);

   for (uint32_t i = 0; i < h; ++i) {
      bool is_even_row = (i & 1) == 0;
      int box_idy = static_cast<int>(i / box_h);
      if (box_idy >= static_cast<int>(cfg.grid_height)) {
         box_idy = cfg.grid_height - 1;
      }
      float dy = (static_cast<float>(i) - box_idy * box_h) / box_h;
      dy = std::clamp(dy, 0.0f, 1.0f);

      for (uint32_t j = 0; j < w; ++j) {
         bool is_even_col = (j & 1) == 0;
         int box_idx = static_cast<int>(j / box_w);
         if (box_idx >= static_cast<int>(cfg.grid_width)) {
            box_idx = cfg.grid_width - 1;
         }
         float dx = (static_cast<float>(j) - box_idx * box_w) / box_w;
         dx = std::clamp(dx, 0.0f, 1.0f);

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

         // base pointer for the channel's gain grid
         const float *grid = lsc_mem_ptr + static_cast<int>(channel) * nodes_per_channel;

         // get gains
         uint32_t pitch = cfg.grid_width + 1;
         float G00 = grid[box_idy * pitch + box_idx];
         float G10 = grid[box_idy * pitch + (box_idx + 1)];
         float G01 = grid[(box_idy + 1) * pitch + box_idx];
         float G11 = grid[(box_idy + 1) * pitch + (box_idx + 1)];

         float gain = bilinear_interpolate(G00, G10, G01, G11, dx, dy);
         float val = static_cast<float>(in[i * w + j]) * gain;
         out[i * w + j] = static_cast<uint16_t>(std::clamp(val, 0.0f, bit_range));
      }
   }
}
