#include "bnr.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

static std::vector<float> make_spatial_kernel(int size, float std_dev, int stride) {
   std::vector<float> kern(size * size);
   float sum = 0.0f;
   int half = (size - 1) / 2;
   for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
         float d2 = static_cast<float>((stride * (i - half)) * (stride * (i - half)) +
                                       (stride * (j - half)) * (stride * (j - half)));
         float val = std::exp(-d2 / (2.0f * std_dev * std_dev));
         kern[i * size + j] = val;
         sum += val;
      }
   }
   for (float &k : kern) {
      k /= sum;
   }
   return kern;
}

static void joint_bilateral_filter(const std::vector<float> &in_img,
                                   const std::vector<float> &guide_img,
                                   std::vector<float> &out_img,
                                   uint32_t w,
                                   uint32_t h,
                                   int spatial_size,
                                   float stddev_s,
                                   float stddev_r,
                                   int stride) {
   spatial_size += ((spatial_size & 1) == 0) ? 1 : 0;
   spatial_size = (spatial_size <= 0) ? 3 : spatial_size;

   std::vector<float> s_kern = make_spatial_kernel(spatial_size, stddev_s, stride);
   int half = (spatial_size - 1) / 2;

   auto get_pixel_mirror = [&](const std::vector<float> &buf, int r, int c) -> float {
      if (r < 0)
         r = -r;
      else if (r >= h)
         r = 2 * h - 2 - r;
      if (c < 0)
         c = -c;
      else if (c >= w)
         c = 2 * w - 2 - c;
      return buf[r * w + c];
   };

   for (int r = 0; r < static_cast<int>(h); ++r) {
      for (int c = 0; c < static_cast<int>(w); ++c) {
         float guide_center = guide_img[r * w + c];
         float sum_val = 0.0f;
         float sum_w = 0.0f;

         for (int i = 0; i < spatial_size; ++i) {
            int nr = r + i - half;
            for (int j = 0; j < spatial_size; ++j) {
               int nc = c + j - half;
               float guide_neigh = get_pixel_mirror(guide_img, nr, nc);
               float in_neigh = get_pixel_mirror(in_img, nr, nc);

               float range_diff = guide_center - guide_neigh;
               float range_w = std::exp(-(range_diff * range_diff) / (2.0f * stddev_r * stddev_r));
               float spatial_w = s_kern[i * spatial_size + j];

               float weight = spatial_w * range_w;
               sum_val += weight * in_neigh;
               sum_w += weight;
            }
         }

         out_img[r * w + c] = (sum_w > 0.0f) ? (sum_val / sum_w) : guide_center;
      }
   }
}

void bnr_block::process(const uint16_t *in,
                        uint16_t *out,
                        uint32_t w,
                        uint32_t h,
                        const bnr_config &cfg,
                        cfa_types bayer_pattern,
                        uint8_t bit_depth) {
   if (!cfg.is_enable) {
      std::memcpy(out, in, w * h * sizeof(uint16_t));
      return;
   }

   uint32_t bit_range = (1u << bit_depth) - 1;
   float scale = 1.0f / bit_range;
   std::vector<float> norm_in(w * h);
   for (uint32_t i = 0; i < w * h; ++i) {
      norm_in[i] = static_cast<float>(in[i]) * scale;
   }

   auto get_pixel_mirror = [&](const std::vector<float> &buf, int r, int c) -> float {
      if (r < 0)
         r = -r;
      else if (r >= static_cast<int>(h))
         r = 2 * static_cast<int>(h) - 2 - r;
      if (c < 0)
         c = -c;
      else if (c >= static_cast<int>(w))
         c = 2 * static_cast<int>(w) - 2 - c;
      return buf[r * w + c];
   };

   // green channel interpolation for guidance
   std::vector<float> kern_filt_g(w * h);
   for (int i = 0; i < static_cast<int>(h); ++i) {
      for (int j = 0; j < static_cast<int>(w); ++j) {
         float sum = 0.0f;
         sum += -1.0f * get_pixel_mirror(norm_in, i - 2, j);
         sum += 2.0f * get_pixel_mirror(norm_in, i - 1, j);
         sum += -1.0f * get_pixel_mirror(norm_in, i, j - 2);
         sum += 2.0f * get_pixel_mirror(norm_in, i, j - 1);
         sum += 4.0f * get_pixel_mirror(norm_in, i, j);
         sum += 2.0f * get_pixel_mirror(norm_in, i, j + 1);
         sum += -1.0f * get_pixel_mirror(norm_in, i, j + 2);
         sum += 2.0f * get_pixel_mirror(norm_in, i + 1, j);
         sum += -1.0f * get_pixel_mirror(norm_in, i + 2, j);

         float val = sum / 8.0f;
         kern_filt_g[i * w + j] = std::clamp(val, 0.0f, 1.0f);
      }
   }

   // extract subimages and populate full interp_g
   std::vector<float> interp_g = norm_in;
   uint32_t sub_w = w / 2;
   uint32_t sub_h = h / 2;
   std::vector<float> in_img_r(sub_w * sub_h);
   std::vector<float> in_img_b(sub_w * sub_h);
   std::vector<float> interp_g_at_r(sub_w * sub_h);
   std::vector<float> interp_g_at_b(sub_w * sub_h);

   for (uint32_t i = 0; i < sub_h; ++i) {
      for (uint32_t j = 0; j < sub_w; ++j) {
         uint32_t r_idx = 0, b_idx = 0;
         switch (bayer_pattern) {
         case cfa_types::RGGB:
            r_idx = (2 * i) * w + (2 * j);
            b_idx = (2 * i + 1) * w + (2 * j + 1);
            break;
         case cfa_types::BGGR:
            r_idx = (2 * i + 1) * w + (2 * j + 1);
            b_idx = (2 * i) * w + (2 * j);
            break;
         case cfa_types::GRBG:
            r_idx = (2 * i) * w + (2 * j + 1);
            b_idx = (2 * i + 1) * w + (2 * j);
            break;
         case cfa_types::GBRG:
            r_idx = (2 * i + 1) * w + (2 * j);
            b_idx = (2 * i) * w + (2 * j + 1);
            break;
         }

         in_img_r[i * sub_w + j] = norm_in[r_idx];
         in_img_b[i * sub_w + j] = norm_in[b_idx];

         interp_g[r_idx] = kern_filt_g[r_idx];
         interp_g[b_idx] = kern_filt_g[b_idx];

         interp_g_at_r[i * sub_w + j] = kern_filt_g[r_idx];
         interp_g_at_b[i * sub_w + j] = kern_filt_g[b_idx];
      }
   }

   // calculate window dimensions
   int filt_size_g = cfg.filter_window;
   int filt_size_r = (cfg.filter_window + 1) / 2;
   int filt_size_b = (cfg.filter_window + 1) / 2;

   // run joint bilateral filtering
   std::vector<float> out_img_r(sub_w * sub_h);
   std::vector<float> out_img_g(w * h);
   std::vector<float> out_img_b(sub_w * sub_h);

   joint_bilateral_filter(in_img_r, interp_g_at_r, out_img_r, sub_w, sub_h, filt_size_r, cfg.r_std_dev_s,
                          cfg.r_std_dev_r, 2);
   joint_bilateral_filter(interp_g, interp_g, out_img_g, w, h, filt_size_g, cfg.g_std_dev_s, cfg.g_std_dev_r,
                          1);
   joint_bilateral_filter(in_img_b, interp_g_at_b, out_img_b, sub_w, sub_h, filt_size_b, cfg.b_std_dev_s,
                          cfg.b_std_dev_r, 2);

   // reconstruct bayer grid
   std::vector<float> bnr_out = out_img_g;
   for (uint32_t i = 0; i < sub_h; ++i) {
      for (uint32_t j = 0; j < sub_w; ++j) {
         uint32_t r_idx = 0, b_idx = 0;
         switch (bayer_pattern) {
         case cfa_types::RGGB:
            r_idx = (2 * i) * w + (2 * j);
            b_idx = (2 * i + 1) * w + (2 * j + 1);
            break;
         case cfa_types::BGGR:
            r_idx = (2 * i + 1) * w + (2 * j + 1);
            b_idx = (2 * i) * w + (2 * j);
            break;
         case cfa_types::GRBG:
            r_idx = (2 * i) * w + (2 * j + 1);
            b_idx = (2 * i + 1) * w + (2 * j);
            break;
         case cfa_types::GBRG:
            r_idx = (2 * i + 1) * w + (2 * j);
            b_idx = (2 * i) * w + (2 * j + 1);
            break;
         }
         bnr_out[r_idx] = out_img_r[i * sub_w + j];
         bnr_out[b_idx] = out_img_b[i * sub_w + j];
      }
   }

   // rescale and clamp
   for (uint32_t i = 0; i < w * h; ++i) {
      float val = bnr_out[i] * bit_range;
      out[i] = static_cast<uint16_t>(std::clamp(val, 0.0f, static_cast<float>(bit_range)));
   }
}
