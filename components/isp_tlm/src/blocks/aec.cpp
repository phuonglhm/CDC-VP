#include "aec.h"
#include <algorithm>
#include <cstdint>
#include <cmath>

const uint8_t bit_range_8 = 255;

void aec_block::process(const uint16_t *in, uint32_t w, uint32_t h, aec_config &cfg, uint8_t bit_depth) {
   if (!cfg.is_enable)
      return;

   double m2 = 0.0;
   double m3 = 0.0;
   double img_size = static_cast<double>(w * h);

   int shift = (bit_depth > 8) ? bit_depth - 8 : 0;

   for (int i = 0; i < w * h; ++i) {
      double r = static_cast<double>(in[3 * i] >> shift);
      double g = static_cast<double>(in[3 * i + 1] >> shift);
      double b = static_cast<double>(in[3 * i + 2] >> shift);
      double y = 0.299 * r + 0.587 * g + 0.144 * b;

      y = std::clamp(y, 0.0, static_cast<double>(bit_range_8));

      y -= cfg.center_illuminance;

      m2 += y * y;
      m3 += y * y * y;
   }

   m2 /= img_size;
   m3 /= img_size;

   double skewness = 0.0;
   if (m2 > 1e-6) {
      skewness = m3 * std::sqrt(img_size * (img_size - 1)) / std::pow(m2, 1.5) / (img_size - 2);
   }

   if (skewness < -cfg.histogram_skewness)
      cfg.ae_feedback = -1;
   else if (skewness > cfg.histogram_skewness)
      cfg.ae_feedback = 1;
   else
      cfg.ae_feedback = 0;
}
