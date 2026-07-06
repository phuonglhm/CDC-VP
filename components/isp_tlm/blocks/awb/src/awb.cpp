#include "awb.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float AWB_GRAY_TARGET = 0.5f;

float compute_channel_average(const std::uint16_t *in,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t rgb_offset) {
   std::uint64_t sum = 0;
   const std::uint32_t total_pixels = width * height;

   for (std::uint32_t p = 0; p < total_pixels; ++p) {
      sum += in[p * 3 + rgb_offset];
   }

   if (total_pixels == 0) {
      return 0.0f;
   }

   return static_cast<float>(sum) / static_cast<float>(total_pixels);
}

float compute_gray_world_gains(float avg_r, float avg_g, float avg_b) {
   if (avg_g <= 0.0f) {
      return 1.0f;
   }

   const float target = (avg_r + avg_g + avg_b) / 3.0f;

   float r_gain = avg_g / avg_r;
   float b_gain = avg_g / avg_b;

   r_gain = std::max(0.25f, std::min(4.0f, r_gain));
   b_gain = std::max(0.25f, std::min(4.0f, b_gain));

   return (r_gain + b_gain) / 2.0f;
}

} // namespace

void awb_block::process(const std::uint16_t *in,
                        std::uint32_t width,
                        std::uint32_t height,
                        awb_config &cfg,
                        std::uint8_t bit_depth) const {
   if (in == nullptr || width == 0 || height == 0) {
      return;
   }

   if (!cfg.is_enable) {
      cfg.r_gain_out = 1.0f;
      cfg.b_gain_out = 1.0f;
      return;
   }

   const std::uint32_t max_value = (1u << bit_depth) - 1;

   const float avg_r = compute_channel_average(in, width, height, 0);
   const float avg_g = compute_channel_average(in, width, height, 1);
   const float avg_b = compute_channel_average(in, width, height, 2);

   const float avg_r_norm = avg_r / static_cast<float>(max_value);
   const float avg_g_norm = avg_g / static_cast<float>(max_value);
   const float avg_b_norm = avg_b / static_cast<float>(max_value);

   if (avg_r_norm > 0.01f && avg_g_norm > 0.01f && avg_b_norm > 0.01f) {
      const float target = (avg_r_norm + avg_g_norm + avg_b_norm) / 3.0f;

      cfg.r_gain_out = target / avg_r_norm;
      cfg.b_gain_out = target / avg_b_norm;

      cfg.r_gain_out = std::max(0.25f, std::min(4.0f, cfg.r_gain_out));
      cfg.b_gain_out = std::max(0.25f, std::min(4.0f, cfg.b_gain_out));
   } else {
      cfg.r_gain_out = 1.0f;
      cfg.b_gain_out = 1.0f;
   }
}

void awb_block::process(const std::vector<std::uint16_t> &in,
                        std::uint32_t width,
                        std::uint32_t height,
                        awb_config &cfg,
                        std::uint8_t bit_depth) const {
   const std::size_t required = static_cast<std::size_t>(width) * height * 3;
   if (in.size() < required) {
      cfg.r_gain_out = 1.0f;
      cfg.b_gain_out = 1.0f;
      return;
   }
   process(in.data(), width, height, cfg, bit_depth);
}
