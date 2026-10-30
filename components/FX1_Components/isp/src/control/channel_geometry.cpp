// SPDX-License-Identifier: Apache-2.0
#include "control/channel_geometry.h"

#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

// RESIZER_CTRL.SCALE presets (MAS v1.07 Table 6-122); {0, 0}: FullRes (code
// 0) or reserved (15, 22..31).
constexpr geometry modes[32] = {
   {0, 0},       {3840, 2160}, {2560, 1440}, {2880, 1620}, {2304, 1296}, {1920, 1080}, {1280, 720}, {960, 540},
   {640, 360},   {2592, 1944}, {2048, 1536}, {1600, 1200}, {1280, 960},  {800, 600},   {640, 480},  {0, 0},
   {640, 640},   {640, 384},   {640, 352},   {512, 512},   {416, 416},   {320, 320},
};
// CH2 accepts FullRes, 640 x 360, 640 x 480 and the six AI presets (CSR v1.07).
constexpr std::uint32_t ch2_codes = (1u << 0) | (1u << 8) | (1u << 14) | (0x3Fu << 16);

constexpr std::uint32_t max_active_w = 3840, max_active_h = 2160, min_active = 32;

}  // namespace

// CHc_OUTPUT_LIMIT is a fixed RO word (its CSR reset value, Table 6-115).
geometry channel_limit(unsigned ch) {
   if (ch == 2) {
      return {(FX1_ISP_CH2_OUTPUT_LIMIT_RESET & FX1_ISP_CH2_OUTPUT_LIMIT_MAX_W_MASK) >> FX1_ISP_CH2_OUTPUT_LIMIT_MAX_W_SHIFT,
              (FX1_ISP_CH2_OUTPUT_LIMIT_RESET & FX1_ISP_CH2_OUTPUT_LIMIT_MAX_H_MASK) >> FX1_ISP_CH2_OUTPUT_LIMIT_MAX_H_SHIFT};
   }
   if (ch == 1) {
      return {(FX1_ISP_CH1_OUTPUT_LIMIT_RESET & FX1_ISP_CH1_OUTPUT_LIMIT_MAX_W_MASK) >> FX1_ISP_CH1_OUTPUT_LIMIT_MAX_W_SHIFT,
              (FX1_ISP_CH1_OUTPUT_LIMIT_RESET & FX1_ISP_CH1_OUTPUT_LIMIT_MAX_H_MASK) >> FX1_ISP_CH1_OUTPUT_LIMIT_MAX_H_SHIFT};
   }
   return {(FX1_ISP_CH0_OUTPUT_LIMIT_RESET & FX1_ISP_CH0_OUTPUT_LIMIT_MAX_W_MASK) >> FX1_ISP_CH0_OUTPUT_LIMIT_MAX_W_SHIFT,
           (FX1_ISP_CH0_OUTPUT_LIMIT_RESET & FX1_ISP_CH0_OUTPUT_LIMIT_MAX_H_MASK) >> FX1_ISP_CH0_OUTPUT_LIMIT_MAX_H_SHIFT};
}

bool resizer_target(unsigned ch, std::uint32_t scale, geometry &target) {
   if (scale >= 32u) {
      return false;
   }
   target = modes[scale];
   if (ch == 2 && !(ch2_codes & (1u << scale))) {
      return false;
   }
   return scale == 0u || target.width != 0u;
}

resizer_plan plan_resizer(std::uint32_t in_w, std::uint32_t in_h, std::uint32_t out_w, std::uint32_t out_h) {
   resizer_plan p{};
   p.out_w = out_w;
   p.out_h = out_h;
   p.dec_w = in_w;
   p.dec_h = in_h;
   p.pass = in_w == out_w && in_h == out_h;
   if (p.pass) {
      return p;
   }
   // The smallest k in {0, 1, 2} with floor(W / 2^k) < 2 Wout on both axes,
   // else 2; then reduced while a decimated axis is below its output
   // (MAS §6.20.3.6: bounded fallback, no upscaling).
   unsigned k = 2;
   for (unsigned t = 0; t <= 2; ++t) {
      if ((in_w >> t) < 2u * out_w && (in_h >> t) < 2u * out_h) {
         k = t;
         break;
      }
   }
   while (k > 0 && ((in_w >> k) < out_w || (in_h >> k) < out_h)) {
      --k;
   }
   p.stages = k;
   p.dec_w = in_w >> k;
   p.dec_h = in_h >> k;
   auto step = [](std::uint32_t n, std::uint32_t o) {
      return static_cast<std::uint32_t>((std::uint64_t{n} * (1u << 17) + o) / (2u * o));  // round to nearest
   };
   auto init = [](std::uint32_t s) { return (s - 65536u + 1u) >> 1; };  // half up
   p.step_x = step(p.dec_w, out_w);
   p.step_y = step(p.dec_h, out_h);
   p.init_x = init(p.step_x);
   p.init_y = init(p.step_y);
   return p;
}

channel_set derive_channel(const channel_key &k, const channel_set &previous) {
   channel_set s;
   s.key = k;
   if (!k.valid) {
      return s;  // the reset set: nothing validated, readback zero
   }
   const std::uint32_t cx = k.crop_en ? k.crop_x : 0u, cy = k.crop_en ? k.crop_y : 0u;
   const std::uint32_t cw = k.crop_en ? k.crop_w : k.width, chh = k.crop_en ? k.crop_h : k.height;
   geometry target{0, 0};
   const bool mode_ok = !k.en || resizer_target(k.ch, k.scale, target);
   const bool fullres = !k.en || k.scale == 0u;
   const geometry out = fullres ? geometry{cw, chh} : target;
   const geometry lim = channel_limit(k.ch);
   unsigned e = geo_ok;
   if (!k.input_ok || k.width < min_active || k.height < min_active || k.width > max_active_w ||
       k.height > max_active_h) {
      e = geo_input;
   } else if (k.crop_en && (cw == 0u || chh == 0u || std::uint64_t{cx} + cw > k.width ||
                            std::uint64_t{cy} + chh > k.height)) {
      e = geo_crop_bounds;
   } else if (k.crop_en && ((cx | cy | cw | chh) & 1u)) {
      e = geo_crop_odd;
   } else if (!mode_ok) {
      e = geo_mode;
   } else if (out.width > lim.width || out.height > lim.height) {
      e = geo_limit;
   } else if (out.width > cw || out.height > chh) {
      e = geo_exceeds_crop;
   }
   if (e != geo_ok) {
      s.error = e;  // readback of the last valid set remains (MAS §8.3)
      s.crop_x = previous.crop_x;
      s.crop_y = previous.crop_y;
      s.crop_w = previous.crop_w;
      s.crop_h = previous.crop_h;
      s.out = previous.out;
      return s;
   }
   s.valid = true;
   s.crop_x = cx;
   s.crop_y = cy;
   s.crop_w = cw;
   s.crop_h = chh;
   s.out = out;
   s.plan = plan_resizer(cw, chh, out.width, out.height);
   s.scaling = !s.plan.pass;
   return s;
}

}  // namespace cdc::components::fx1_isp
