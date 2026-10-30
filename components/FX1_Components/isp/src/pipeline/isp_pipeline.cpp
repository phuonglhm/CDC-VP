// SPDX-License-Identifier: Apache-2.0
#include "pipeline/isp_pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <stdexcept>

#include "control/control_unit.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp::pipe {

namespace {

std::uint32_t fld(std::uint32_t reg, std::uint32_t mask, std::uint32_t shift) { return (reg & mask) >> shift; }

std::int32_t sext12(std::uint32_t v) { return (v & 0x800u) ? static_cast<std::int32_t>(v) - 4096 : static_cast<std::int32_t>(v); }
std::int32_t sext(std::uint32_t v, unsigned bits) {
   const std::uint32_t sign = 1u << (bits - 1u);
   return static_cast<std::int32_t>((v ^ sign)) - static_cast<std::int32_t>(sign);
}

std::uint16_t sat12(std::int64_t v) { return static_cast<std::uint16_t>(std::clamp<std::int64_t>(v, 0, 4095)); }

std::int64_t mirror(std::int64_t i, std::int64_t n) { return border_index(i, n, border::mirror); }

}  // namespace

// ---- configuration ----------------------------------------------------------

pipeline_config snapshot_config(control_unit &ctl) {
   const register_file &r = ctl.regs();
   auto reg = [&](std::uint32_t off) { return r.peek(off); };
   pipeline_config c;
   c.bayer_pattern = reg(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;
   c.input_depth = reg(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK;

   c.blc_en = reg(FX1_ISP_BLC_CTRL_OFFSET) & FX1_ISP_BLC_CTRL_EN_MASK;
   const std::uint32_t sel = reg(FX1_ISP_BLC_GAIN_SEL_OFFSET);
   const std::uint32_t g = fld(sel, FX1_ISP_BLC_GAIN_SEL_GAIN_LEVEL_MASK, FX1_ISP_BLC_GAIN_SEL_GAIN_LEVEL_SHIFT);
   c.blc_range_scale = sel & FX1_ISP_BLC_GAIN_SEL_RANGE_SCALE_EN_MASK;
   const std::uint32_t base = FX1_ISP_BLC_OFS_G0_DFT_OFFSET + g * (FX1_ISP_BLC_OFS_G1_DFT_OFFSET - FX1_ISP_BLC_OFS_G0_DFT_OFFSET);
   c.blc_pedestal = reg(base) & 0xFFFu;
   for (unsigned ch = 0; ch < 4; ++ch) {
      c.blc_trim[ch] = reg(base + 4u * (ch + 1u)) & 0xFFFu;
   }
   c.blc_scale = reg(FX1_ISP_BLC_SCALE_G0_OFFSET + 4u * g) & FX1_ISP_BLC_SCALE_G0_SCALE_G0_MASK;

   const std::uint32_t nodes = reg(FX1_ISP_LSC_MESH_NODES_OFFSET);
   c.lsc_nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
   c.lsc_ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
   c.lsc_strength = reg(FX1_ISP_LSC_STRENGTH_OFFSET) & FX1_ISP_LSC_STRENGTH_STRENGTH_MASK;
   const unsigned active = ctl.lsc_active_profile();
   c.lsc_en = (reg(FX1_ISP_LSC_CTRL_OFFSET) & FX1_ISP_LSC_CTRL_EN_MASK) && ctl.lsc_profile_valid(active) &&
              ctl.lsc_geometry_valid();
   if (c.lsc_en) {
      c.lsc_mesh = ctl.lsc_profile(active);
   }
   c.bpc_en = reg(FX1_ISP_BPC_CTRL_OFFSET) & FX1_ISP_BPC_CTRL_EN_MASK;
   const std::uint32_t th = reg(FX1_ISP_BPC_THRESH_OFFSET);
   c.bpc_floor = th & FX1_ISP_BPC_THRESH_THRESH_FLOOR_MASK;
   c.bpc_k = (th & FX1_ISP_BPC_THRESH_THRESH_K_MASK) >> FX1_ISP_BPC_THRESH_THRESH_K_SHIFT;

   c.wb_en = reg(FX1_ISP_WB_CTRL_OFFSET) & FX1_ISP_WB_CTRL_EN_MASK;
   c.wb_gain = {reg(FX1_ISP_WB_GAIN_R_OFFSET) & 0xFFFu, reg(FX1_ISP_WB_GAIN_G_OFFSET) & 0xFFFu,
                reg(FX1_ISP_WB_GAIN_B_OFFSET) & 0xFFFu};
   c.dg_en = reg(FX1_ISP_DG_CTRL_OFFSET) & FX1_ISP_DG_CTRL_EN_MASK;
   c.dg_gain = reg(FX1_ISP_DG_GAIN_OFFSET) & FX1_ISP_DG_GAIN_GAIN_MASK;
   // Profile-mode extension controls (MAS v1.07 §9.9.2: legacy mode uses the
   // documented baseline). The register file holds their datapath value.
   const bool prof = ctl.profile_mode();
   c.wb_gain_gb = prof ? reg(FX1_ISP_TUNE_WB_GB_OFFSET) & FX1_ISP_TUNE_WB_GB_VALUE_MASK : c.wb_gain[1];
   const std::uint32_t margin = reg(FX1_ISP_TUNE_DEMOSAIC_MARGIN_OFFSET);
   c.dms_margin_green = margin & FX1_ISP_TUNE_DEMOSAIC_MARGIN_GREEN_MASK;
   c.dms_margin_rb = (margin & FX1_ISP_TUNE_DEMOSAIC_MARGIN_RB_MASK) >> FX1_ISP_TUNE_DEMOSAIC_MARGIN_RB_SHIFT;
   c.cnf_blend = reg(FX1_ISP_TUNE_CNF_BLEND_OFFSET) & FX1_ISP_TUNE_CNF_BLEND_VALUE_MASK;
   const std::uint32_t bctl = reg(FX1_ISP_TUNE_BPC_CTRL_OFFSET);
   c.bpc_color = bctl & FX1_ISP_TUNE_BPC_CTRL_COLOR_THRESH_EN_MASK;
   const std::uint32_t tg = reg(FX1_ISP_TUNE_BPC_G_THRESH_OFFSET), trb = reg(FX1_ISP_TUNE_BPC_RB_THRESH_OFFSET);
   c.bpc_k_g = tg & FX1_ISP_TUNE_BPC_G_THRESH_K_MASK;
   c.bpc_floor_g = (tg & FX1_ISP_TUNE_BPC_G_THRESH_FLOOR_MASK) >> FX1_ISP_TUNE_BPC_G_THRESH_FLOOR_SHIFT;
   c.bpc_k_rb = trb & FX1_ISP_TUNE_BPC_RB_THRESH_K_MASK;
   c.bpc_floor_rb = (trb & FX1_ISP_TUNE_BPC_RB_THRESH_FLOOR_MASK) >> FX1_ISP_TUNE_BPC_RB_THRESH_FLOOR_SHIFT;
   const geometry act = ctl.active_geometry();
   if (bctl & FX1_ISP_TUNE_BPC_CTRL_FACTORY_EN_MASK) {
      std::uint32_t count = 0;
      const std::uint32_t *list = ctl.bpc_factory_list(count);
      for (std::uint32_t i = 0; i < count; ++i) {
         c.bpc_factory.push_back(std::uint64_t{list[i] >> 16} * act.width + (list[i] & 0xFFFFu));
      }
   }
   c.lsc_sectors = reg(FX1_ISP_TUNE_LSC_CTRL_OFFSET) & FX1_ISP_TUNE_LSC_CTRL_SECTOR_EN_MASK;
   if (c.lsc_sectors) {
      for (unsigned axis = 0; axis < 2; ++axis) {
         const std::uint16_t *iv = ctl.lsc_sector_intervals(axis);
         const std::uint32_t n = axis ? c.lsc_ny : c.lsc_nx;
         c.lsc_sector[axis].assign(iv, iv + (n > 1 ? n - 1 : 0));
      }
   }

   c.ccm_en = reg(FX1_ISP_CCM_CTRL_OFFSET) & FX1_ISP_CCM_CTRL_EN_MASK;
   for (unsigned row = 0; row < 3; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
         c.ccm[row][col] = sext12(ctl.committed(FX1_ISP_CCM_CRR_OFFSET + 4u * (3u * row + col)) & 0xFFFu);
      }
      c.ccm_offset[row] = sext12(ctl.committed(FX1_ISP_CCM_OFS_R_OFFSET + 4u * row) & 0xFFFu);
   }

   c.gtm_en = reg(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_EN_MASK;
   // Manual mode reads the frame bank's manual curve, applied only when complete;
   // automatic mode the published ping-pong curve (MAS v1.07 §6.16.4.3).
   const bool gtm_manual = reg(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_MANUAL_MASK;
   c.gtm_curve_valid = gtm_manual ? ctl.gtm_manual_complete() : ctl.gtm_curve_valid();
   c.gtm_bank = gtm_manual ? &ctl.gtm_manual_curve() : &ctl.gtm_read_bank();
   c.gtm_roi_log2 = reg(FX1_ISP_GTM_ROI_LOG2_OFFSET) & FX1_ISP_GTM_ROI_LOG2_ROI_LOG2_MASK;
   c.nr_en = reg(FX1_ISP_NR_2D_CTRL_OFFSET) & FX1_ISP_NR_2D_CTRL_EN_MASK;
   c.nr_var = ctl.nr_variance();
   c.nr_estimate_valid = ctl.nr_estimate_valid();
   const std::uint32_t nctl = reg(FX1_ISP_TUNE_NR_CTRL_OFFSET);
   c.nr_mode = nctl & FX1_ISP_TUNE_NR_CTRL_MODE_MASK;
   c.nr_median = nctl & FX1_ISP_TUNE_NR_CTRL_MEDIAN_EN_MASK;
   c.nr_seed = reg(FX1_ISP_TUNE_NR_SEED_OFFSET) & FX1_ISP_TUNE_NR_SEED_VALUE_MASK;
   c.nr_manual = reg(FX1_ISP_TUNE_NR_VARIANCE_OFFSET) & FX1_ISP_TUNE_NR_VARIANCE_VALUE_MASK;
   c.nr_scale = reg(FX1_ISP_TUNE_NR_SCALE_OFFSET) & FX1_ISP_TUNE_NR_SCALE_VALUE_MASK;
   const std::uint32_t lim = reg(FX1_ISP_TUNE_NR_LIMITS_OFFSET);
   c.nr_min = lim & FX1_ISP_TUNE_NR_LIMITS_MIN_MASK;
   c.nr_max = (lim & FX1_ISP_TUNE_NR_LIMITS_MAX_MASK) >> FX1_ISP_TUNE_NR_LIMITS_MAX_SHIFT;
   c.nr_band_scale = {reg(FX1_ISP_TUNE_NR_LL_SCALE_OFFSET) & 0xFFFFu, reg(FX1_ISP_TUNE_NR_LH_SCALE_OFFSET) & 0xFFFFu,
                      reg(FX1_ISP_TUNE_NR_HL_SCALE_OFFSET) & 0xFFFFu, reg(FX1_ISP_TUNE_NR_HH_SCALE_OFFSET) & 0xFFFFu};
   c.nr_blend = reg(FX1_ISP_TUNE_NR_BLEND_OFFSET) & FX1_ISP_TUNE_NR_BLEND_VALUE_MASK;
   if (c.nr_mode == 2) {
      const std::uint16_t *k = ctl.nr_curve();
      std::copy(k, k + 17, c.nr_curve.begin());
   }
   c.live = &ctl;
   c.ee = ctl.ee_frame();
   c.cnf_en = reg(FX1_ISP_CNF_CTRL_OFFSET) & FX1_ISP_CNF_CTRL_EN_MASK;
   c.cnf_chroma_th = ctl.committed(FX1_ISP_CNF_CHROMA_TH_OFFSET) & FX1_ISP_CNF_CHROMA_TH_CHROMA_TH_MASK;
   c.cnf_luma_th = ctl.committed(FX1_ISP_CNF_LUMA_TH_OFFSET) & FX1_ISP_CNF_LUMA_TH_LUMA_TH_MASK;

   // An unconfirmed curve passes pixels through (MAS v1.07 §6.14.4).
   c.gamma_en = (reg(FX1_ISP_GAMMA_CTRL_OFFSET) & FX1_ISP_GAMMA_CTRL_EN_MASK) && ctl.gamma_frame_valid();
   c.gamma_lut = &ctl.gamma_lut();
   c.csc_std = reg(FX1_ISP_CSC_CTRL_OFFSET) & FX1_ISP_CSC_CTRL_STD_MASK;
   // The TUNE extension controls hold their datapath value: the field reset in
   // legacy mode, the active bank in profile mode (V107-3).
   const std::uint32_t cc = reg(FX1_ISP_TUNE_CSC_CTRL_OFFSET);
   c.csc_full_range = cc & FX1_ISP_TUNE_CSC_CTRL_FULL_RANGE_MASK;
   c.csc_look = cc & FX1_ISP_TUNE_CSC_CTRL_LOOK_EN_MASK;
   c.csc_contrast = reg(FX1_ISP_TUNE_CSC_CONTRAST_OFFSET) & FX1_ISP_TUNE_CSC_CONTRAST_VALUE_MASK;
   c.csc_brightness = sext(reg(FX1_ISP_TUNE_CSC_BRIGHTNESS_OFFSET) & FX1_ISP_TUNE_CSC_BRIGHTNESS_VALUE_MASK, 10);
   c.csc_m = {sext(reg(FX1_ISP_TUNE_CSC_UU_OFFSET) & FX1_ISP_TUNE_CSC_UU_VALUE_MASK, 18),
              sext(reg(FX1_ISP_TUNE_CSC_UV_OFFSET) & FX1_ISP_TUNE_CSC_UV_VALUE_MASK, 18),
              sext(reg(FX1_ISP_TUNE_CSC_VU_OFFSET) & FX1_ISP_TUNE_CSC_VU_VALUE_MASK, 18),
              sext(reg(FX1_ISP_TUNE_CSC_VV_OFFSET) & FX1_ISP_TUNE_CSC_VV_VALUE_MASK, 18)};

   for (unsigned ch = 0; ch < num_channels; ++ch) {
      c.channels[ch] = ctl.resizer_frame(ch);
   }
   c.outputs = ctl.output_enable_frame();

   c.aec = ctl.aec_frame_config();
   c.awb = ctl.awb_frame_config();
   c.af = ctl.af_frame_config();
   c.ofmt_stride_en = reg(FX1_ISP_OFMT_CTRL_OFFSET) & FX1_ISP_OFMT_CTRL_STRIDE_EN_MASK;
   c.wdr_en = reg(FX1_ISP_D_WDR_CTRL_OFFSET) & FX1_ISP_D_WDR_CTRL_EN_MASK;
   c.tnr_en = reg(FX1_ISP_TNR_3D_CTRL_OFFSET) & FX1_ISP_TNR_3D_CTRL_EN_MASK;
   return c;
}

// ---- pointwise kernels -------------------------------------------------------

std::uint16_t blc_pixel(const pipeline_config &c, std::uint16_t p, unsigned ch) {
   if (!c.blc_en) {
      return p;
   }
   const std::int64_t ofs = std::min<std::int64_t>(c.blc_pedestal + c.blc_trim[ch], 4095);  // CSR #38
   const std::int64_t d = std::max<std::int64_t>(p - ofs, 0);
   if (!c.blc_range_scale) {
      return static_cast<std::uint16_t>(d);
   }
   return sat12((d * c.blc_scale + (std::int64_t{1} << 15)) >> 16);  // ALG-BLC-01: round half up
}

std::uint16_t wb_pixel(const pipeline_config &c, std::uint16_t p, unsigned ch) {
   if (!c.wb_en) {
      return p;
   }
   const std::uint32_t gain = ch == 0 ? c.wb_gain[0] : ch == 3 ? c.wb_gain[2] : ch == 1 ? c.wb_gain[1] : c.wb_gain_gb;
   return sat12((std::int64_t{p} * gain) >> 8);  // floor, saturate (HAS p73)
}

std::uint16_t dg_pixel(const pipeline_config &c, std::uint16_t p) {
   return c.dg_en ? sat12((std::int64_t{p} * c.dg_gain) >> 8) : p;
}

rgb ccm_pixel(const pipeline_config &c, const rgb &p) {
   if (!c.ccm_en) {
      return p;
   }
   const std::int64_t in[3] = {p.r, p.g, p.b};
   std::uint16_t out[3];
   for (unsigned row = 0; row < 3; ++row) {
      std::int64_t acc = c.ccm_offset[row];  // Q3.9 at the accumulator scale (ALG-CCM-02)
      for (unsigned col = 0; col < 3; ++col) {
         acc += c.ccm[row][col] * in[col];
      }
      out[row] = sat12(acc >> 9);  // floor
   }
   return {out[0], out[1], out[2]};
}

yuv csc_pixel(const pipeline_config &cfg, const rgb &p) {
   // MAS v1.07 §6.15.3.3. LKR, LKB per standard; RSY, RSCB, RSCR and OY per
   // range and standard (Table 6-92), selected before the wide products.
   static constexpr std::int64_t lk[2][2] = {{19595, 7471}, {13933, 4732}};
   static constexpr std::int64_t rs[2][2][4] = {{{3505, 2023, 2557, 16}, {3505, 1932, 2276, 16}},  // limited
                                                {{4081, 2303, 2911, 0}, {4081, 2199, 2591, 0}}};   // full
   const unsigned st = cfg.csc_std & 1u;
   const std::int64_t *k = rs[cfg.csc_full_range ? 1 : 0][st];
   const std::int64_t r = p.r, g = p.g, b = p.b;
   const std::int64_t l = lk[st][0] * (r - g) + (g << 16) + lk[st][1] * (b - g);
   // R(n, d) = floor((n + floor(d/2)) / d), signed n: ties toward +infinity.
   auto rnd = [](std::int64_t n, unsigned log2d) { return (n + (std::int64_t{1} << (log2d - 1))) >> log2d; };
   auto sat8 = [](std::int64_t v) { return std::clamp<std::int64_t>(v, 0, 255); };
   const std::int64_t y0 = sat8(rnd(k[0] * l, 32) + k[3]);
   const std::int64_t u0 = sat8(rnd(k[1] * ((b << 16) - l), 32) + 128);
   const std::int64_t v0 = sat8(rnd(k[2] * ((r << 16) - l), 32) + 128);
   if (!cfg.csc_look) {
      return {static_cast<std::uint16_t>(y0), static_cast<std::uint16_t>(u0), static_cast<std::uint16_t>(v0)};
   }
   // Look, separately quantized; pedestal P = 0 full, 16 limited; matrix on the
   // original U0/V0 with one rounding (no 16..235/240 clamp).
   const std::int64_t ped = cfg.csc_full_range ? 0 : 16;
   const std::int64_t y1 = sat8(ped + rnd(std::int64_t{cfg.csc_contrast} * (y0 - ped), 8) + cfg.csc_brightness);
   const std::int64_t du = u0 - 128, dv = v0 - 128;
   const std::int64_t u1 = sat8(128 + rnd(cfg.csc_m[0] * du + cfg.csc_m[1] * dv, 14));
   const std::int64_t v1 = sat8(128 + rnd(cfg.csc_m[2] * du + cfg.csc_m[3] * dv, 14));
   return {static_cast<std::uint16_t>(y1), static_cast<std::uint16_t>(u1), static_cast<std::uint16_t>(v1)};
}

// ---- output channels (MAS v1.07 §6.20.1.1, §6.24.7) ---------------------------

channel_set channel_for_frame(const pipeline_config &cfg, const geometry &active, unsigned ch) {
   const channel_set &c = cfg.channels.at(ch);
   if (c.valid && c.key.width == active.width && c.key.height == active.height) {
      return c;
   }
   channel_set full;  // no admission-validated set for this raster: full active, unscaled
   full.valid = true;
   full.crop_w = active.width;
   full.crop_h = active.height;
   full.out = active;
   full.plan = plan_resizer(active.width, active.height, active.width, active.height);
   return full;
}

frame_geometry output_geometry(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                               unsigned ch) {
   const geometry a = control_unit::derive_active(in_width, in_height, cfg.bayer_pattern);
   const channel_set c = channel_for_frame(cfg, a, ch);
   return {c.out.width, c.out.height};
}

rgb8 rgb888_pixel(std::uint32_t std, bool full_range, std::uint16_t y, std::uint16_t u, std::uint16_t v) {
   // Table 6-89 (Q16): {Y scale, CrR, CbG, CrG, CbB} by range and standard.
   static constexpr std::int64_t k[2][2][5] = {
      {{76309, 104597, 25675, 53279, 132201}, {76309, 117489, 13975, 34925, 138438}},  // limited 601, 709
      {{65536, 91881, 22553, 46802, 116130}, {65536, 103206, 12276, 30679, 121609}},   // full 601, 709
   };
   const std::int64_t *c = k[full_range ? 1 : 0][std ? 1 : 0];
   const std::int64_t d = std::int64_t{u} - 128, e = std::int64_t{v} - 128;
   // Luma term: Y * 65536 (full) or (Y - 16) * 76309 (limited), no pre-clamp.
   const std::int64_t l = full_range ? std::int64_t{y} * 65536 : (std::int64_t{y} - 16) * c[0];
   // R(n, 65536) = floor((n + 32768) / 65536), ties toward +inf (arithmetic shift).
   auto out = [](std::int64_t n) { return static_cast<std::uint8_t>(std::clamp<std::int64_t>((n + 32768) >> 16, 0, 255)); };
   return {out(l + c[1] * e), out(l - c[2] * d - c[3] * e), out(l + c[4] * d)};
}

// ---- streaming blocks ------------------------------------------------------------

namespace {

using yuv_row = row<yuv>;

// LSC (HAS §6.8.6, Eq 2 and Eq 9-12): per-axis cell and weight tables for the
// frame, then per-pixel bilinear gain with three rounded stages, strength and
// a single final saturation.
class lsc_stage {
public:
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      c_ = &c;
      stats_.fill(0);
      if (!c.lsc_en || w < 2 || h < 2) {  // ALG-LSC-10: D < 2 runs as identity
         on_ = false;
         return;
      }
      on_ = true;
      axis(w, c.lsc_nx, c.lsc_sectors ? &c.lsc_sector[0] : nullptr, cx_, wx_);
      axis(h, c.lsc_ny, c.lsc_sectors ? &c.lsc_sector[1] : nullptr, cy_, wy_);
   }
   bool on() const { return on_; }
   std::uint16_t pixel(std::uint16_t p, std::uint32_t x, std::uint32_t y, unsigned ch) {
      const std::uint32_t nx = c_->lsc_nx;
      const std::uint32_t i = cx_[x], j = cy_[y];
      const std::int64_t wx = wx_[x], wy = wy_[y];
      auto G = [&](std::uint32_t ii, std::uint32_t jj) -> std::int64_t { return c_->lsc_mesh[4u * (jj * nx + ii) + ch]; };
      auto rnd16 = [](std::int64_t v) { return (v + (1 << 15)) >> 16; };
      const std::int64_t gt = rnd16((65536 - wx) * G(i, j) + wx * G(i + 1, j));           // Eq 10a
      const std::int64_t gb = rnd16((65536 - wx) * G(i, j + 1) + wx * G(i + 1, j + 1));   // Eq 10b
      const std::int64_t gm = rnd16((65536 - wy) * gt + wy * gb);                          // Eq 10c
      const std::int64_t prod = std::int64_t{c_->lsc_strength} * (gm - (1 << 18));
      const std::int64_t mag = ((prod < 0 ? -prod : prod) + (1 << 15)) >> 16;             // srnd16, Eq 11
      const std::int64_t ge = (1 << 18) + (prod < 0 ? -mag : mag);
      const std::int64_t pr = (std::int64_t{p} * ge + (1 << 17)) >> 18;                  // rnd18, Eq 12 (floor)
      if (pr > 4095) {
         ++stats_[ch];
      }
      return sat12(pr);
   }
   const std::array<std::uint32_t, 4> &overflow() const { return stats_; }

private:
   // Eq 2: n_k = floor(k*S/C); cell(X) = max{k in [0, C-1] : n_k <= X}; Eq 9
   // weight. With sectors (MAS v1.07 §6.8.3.9) node k is the sum of the first
   // k programmed intervals and the same cell and weight rule applies to the
   // interval's span (V107-A18).
   static void axis(std::uint32_t d, std::uint32_t n, const std::vector<std::uint32_t> *sectors,
                    std::vector<std::uint32_t> &cell, std::vector<std::int64_t> &w) {
      const std::uint64_t s = d - 1u, c = n - 1u;
      std::vector<std::int64_t> node(c + 1);
      for (std::uint64_t k = 0; k <= c; ++k) {
         node[k] = sectors ? (k ? node[k - 1] + (*sectors)[k - 1] : 0) : static_cast<std::int64_t>((k * s) / c);
      }
      cell.assign(d, 0);
      w.assign(d, 0);
      std::uint32_t k = 0;
      for (std::uint32_t x = 0; x < d; ++x) {
         while (k + 1 < c && node[k + 1] <= x) {
            ++k;
         }
         const std::int64_t n0 = node[k];
         const std::int64_t n1 = node[k + 1];
         const std::int64_t span = n1 - n0;
         cell[x] = k;
         w[x] = ((std::int64_t{x} - n0) * 65536 + (span >> 1)) / span;
      }
   }
   const pipeline_config *c_ = nullptr;
   bool on_ = false;
   std::vector<std::uint32_t> cx_, cy_;
   std::vector<std::int64_t> wx_, wy_;
   std::array<std::uint32_t, 4> stats_{};
};

// BPC static detection (HAS §6.9.6, Eq 2-4, Table 6-29): 8 same-colour
// neighbours at +-2, reflect-101 borders, all tests on uncorrected input.
class bpc_stage {
public:
   using sink = std::function<void(row<std::uint16_t> &&)>;
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      c_ = &c;
      w_ = w;
      cand_ = defect_ = 0;
      y_ = 0;
      next_ = 0;
      win_.begin(h);
   }
   void push(const row<std::uint16_t> &r, const sink &out) {
      if (!c_->bpc_en) {
         out(row<std::uint16_t>(r));
         return;
      }
      win_.add(r);
      drain(false, out);
   }
   void finish(const sink &out) {
      if (c_->bpc_en) {
         drain(true, out);
      }
   }
   std::uint32_t candidates() const { return cand_; }
   std::uint32_t defective() const { return defect_; }

private:
   void drain(bool ended, const sink &out) {
      while (win_.ready(ended)) {
         row<std::uint16_t> o(w_);
         const row<std::uint16_t> &centre = win_.at(0);
         for (std::uint32_t x = 0; x < w_; ++x) {
            std::int64_t sum = 0, lo = 4095, hi = 0;
            for (int dy = -2; dy <= 2; dy += 2) {
               const row<std::uint16_t> &rr = win_.at(dy);
               for (int dx = -2; dx <= 2; dx += 2) {
                  if (dx == 0 && dy == 0) {
                     continue;
                  }
                  const std::int64_t v = rr[static_cast<std::size_t>(mirror(std::int64_t{x} + dx, w_))];
                  sum += v;
                  lo = std::min(lo, v);
                  hi = std::max(hi, v);
               }
            }
            const std::int64_t avg8 = sum >> 3;                                            // truncation
            // Colour thresholds (MAS v1.07 §6.9.3.6): G sites and R/B sites have
            // their own K (UQ4.8) and floor; the full product is compared.
            const bool green = ((y_ & 1u) ^ (x & 1u)) != 0;
            const std::int64_t k = c_->bpc_color ? (green ? c_->bpc_k_g : c_->bpc_k_rb) : c_->bpc_k;
            const std::int64_t floor = c_->bpc_color ? (green ? c_->bpc_floor_g : c_->bpc_floor_rb) : c_->bpc_floor;
            const std::int64_t t = std::max<std::int64_t>(floor, (k * (hi - lo) + 128) >> 8);
            const std::int64_t cval = centre[x];
            const std::int64_t dev = cval > avg8 ? cval - avg8 : avg8 - cval;
            cand_ += dev > floor;
            // A listed factory defect is forced to the same correction and
            // counted as defective (strictly increasing raster list).
            const std::uint64_t pos = std::uint64_t{y_} * w_ + x;
            const bool listed = next_ < c_->bpc_factory.size() && c_->bpc_factory[next_] == pos;
            next_ += listed;
            const bool defect = listed || dev > t;
            defect_ += defect;
            o[x] = static_cast<std::uint16_t>(defect ? avg8 : cval);
         }
         win_.advance();
         ++y_;
         out(std::move(o));
      }
   }
   const pipeline_config *c_ = nullptr;
   std::uint32_t w_ = 0;
   std::uint32_t cand_ = 0, defect_ = 0;
   std::uint32_t y_ = 0;       // row of the centre being emitted
   std::size_t next_ = 0;      // next factory-list entry
   row_window<row<std::uint16_t>> win_{2, border::mirror};
};
using yuv_sink = std::function<void(yuv_row &&)>;

// Demosaic (HAS §6.12.6-6.12.7): 5x5 green reconstruction at scale 64, then
// 3x3 R/B reconstruction on the unrounded green plane, reflect-101 borders.
class demosaic_stage {
public:
   void set_margins(std::uint32_t green, std::uint32_t rb) {
      margin_green_ = green;
      margin_rb_ = rb;
   }
   using sink = std::function<void(row<rgb> &&)>;

   void begin(std::uint32_t w, std::uint32_t h) {
      if (w < 4 || h < 4) {
         throw std::runtime_error("demosaic: frame smaller than 4x4 (ALG-DMS-03)");
      }
      w_ = w;
      h_ = h;
      bayer_.begin(h);
      green_.begin(h);
   }
   void push(const row<std::uint16_t> &r, const sink &out) {
      bayer_.add(r);
      drain(false, out);
   }
   void finish(const sink &out) { drain(true, out); }

private:
   std::uint32_t margin_green_ = 0, margin_rb_ = 0;  // MAS v1.07 §6.12.3.7
   struct gb_row {
      row<std::uint16_t> bayer;
      row<std::int32_t> g64;
   };

   void drain(bool ended, const sink &out) {
      while (bayer_.ready(ended)) {
         green_.add(green_row());
         bayer_.advance();
         colour_rows(false, out);
      }
      if (ended) {
         colour_rows(true, out);
      }
   }

   gb_row green_row() const {
      const std::uint32_t y = bayer_.centre();
      gb_row out{bayer_.at(0), row<std::int32_t>(w_)};
      for (std::uint32_t x = 0; x < w_; ++x) {
         const bool green_site = ((y & 1u) ^ (x & 1u)) != 0;
         const std::int32_t c5 = bayer_.at(0)[x];
         if (green_site) {
            out.g64[x] = 64 * c5;
            continue;
         }
         auto P = [&](int dy, int dx) -> std::int32_t {
            return bayer_.at(dy)[static_cast<std::size_t>(mirror(static_cast<std::int64_t>(x) + dx, w_))];
         };
         const std::int32_t c1 = P(-2, 0), c9 = P(2, 0), c3 = P(0, -2), c7 = P(0, 2);
         const std::int32_t g2 = P(-1, 0), g8 = P(1, 0), g4 = P(0, -1), g6 = P(0, 1);
         const std::int32_t dh = std::abs(g4 - g6) + std::abs(2 * c5 - c3 - c7);
         const std::int32_t dv = std::abs(g2 - g8) + std::abs(2 * c5 - c1 - c9);
         if (std::abs(dh - dv) <= static_cast<std::int32_t>(margin_green_)) {  // MAS v1.07 §6.12.3.7
            out.g64[x] = 16 * (g2 + g8 + g4 + g6) + 8 * (4 * c5 - c1 - c9 - c3 - c7);
         } else if (dh > dv) {
            out.g64[x] = 32 * (g2 + g8) + 16 * (2 * c5 - c1 - c9);
         } else if (dh < dv) {
            out.g64[x] = 32 * (g4 + g6) + 16 * (2 * c5 - c3 - c7);
         } else {
            out.g64[x] = 16 * (g2 + g8 + g4 + g6) + 8 * (4 * c5 - c1 - c9 - c3 - c7);
         }
      }
      return out;
   }

   static std::uint16_t round64(std::int64_t v) {
      const std::int64_t r = v >= 0 ? (v + 32) >> 6 : -((-v + 32) >> 6);  // ties away from zero
      return sat12(r);
   }

   void colour_rows(bool ended, const sink &out) {
      while (green_.ready(ended)) {
         const std::uint32_t y = green_.centre();
         row<rgb> o(w_);
         for (std::uint32_t x = 0; x < w_; ++x) {
            // Tap n of Fig 6-25: 1..9 raster over (dy, dx) in {-1,0,1}^2.
            auto tap = [&](int n, bool green) -> std::int64_t {
               const int dy = (n - 1) / 3 - 1, dx = (n - 1) % 3 - 1;
               const gb_row &gr = green_.at(dy);
               const auto xi = static_cast<std::size_t>(mirror(static_cast<std::int64_t>(x) + dx, w_));
               return green ? std::int64_t{gr.g64[xi]} : std::int64_t{gr.bayer[xi]};
            };
            auto K = [&](int n) { return 64 * tap(n, false); };
            auto G = [&](int n) { return tap(n, true); };
            const std::int64_t g5 = G(5);
            auto col = [&] { return exact(K(2) + K(8) + 2 * g5 - G(2) - G(8), 2); };
            auto rowi = [&] { return exact(K(4) + K(6) + 2 * g5 - G(4) - G(6), 2); };
            auto diag = [&] {
               const std::int64_t dn = 64 * std::abs(tap(1, false) - tap(9, false)) + std::abs(2 * g5 - G(1) - G(9));
               const std::int64_t dp = 64 * std::abs(tap(3, false) - tap(7, false)) + std::abs(2 * g5 - G(3) - G(7));
               if (std::abs(dn - dp) <= std::int64_t{margin_rb_}) {
                  return exact(2 * (K(1) + K(3) + K(7) + K(9)) + 4 * g5 - G(1) - G(3) - G(7) - G(9), 8);
               }
               if (dn > dp) {
                  return exact(2 * (K(3) + K(7)) + 2 * g5 - G(3) - G(7), 4);
               }
               if (dn < dp) {
                  return exact(2 * (K(1) + K(9)) + 2 * g5 - G(1) - G(9), 4);
               }
               return exact(2 * (K(1) + K(3) + K(7) + K(9)) + 4 * g5 - G(1) - G(3) - G(7) - G(9), 8);
            };
            const unsigned ch = 2u * (y & 1u) + (x & 1u);
            const std::int64_t native = K(5);
            std::int64_t r64 = 0, b64 = 0;
            switch (ch) {
            case 0: r64 = native; b64 = diag(); break;   // R site
            case 1: r64 = rowi(); b64 = col(); break;    // Gr: red row-aligned, blue column-aligned
            case 2: r64 = col(); b64 = rowi(); break;    // Gb
            default: r64 = diag(); b64 = native; break;  // B site
            }
            o[x] = {round64(r64), round64(g5), round64(b64)};
         }
         green_.advance();
         out(std::move(o));
      }
   }

   static std::int64_t exact(std::int64_t num, std::int64_t d) {
      if (num % d != 0) {
         throw std::logic_error("demosaic: non-exact division");
      }
      return num / d;
   }

   std::uint32_t w_ = 0, h_ = 0;
   row_window<row<std::uint16_t>> bayer_{2, border::mirror};
   row_window<gb_row> green_{1, border::mirror};
};

std::int64_t fdiv(std::int64_t a, std::int64_t b) {  // floor division, b > 0
   const std::int64_t q = a / b;
   return (a % b != 0 && a < 0) ? q - 1 : q;
}

std::int64_t sround(std::int64_t x, unsigned s) {  // half away from zero (HAS p135)
   const std::int64_t m = ((x < 0 ? -x : x) + (std::int64_t{1} << (s - 1))) >> s;
   return x < 0 ? -m : m;
}

std::int64_t isqrt(std::int64_t v) {
   std::int64_t r = static_cast<std::int64_t>(std::sqrt(static_cast<double>(v)));
   while (r * r > v) --r;
   while ((r + 1) * (r + 1) <= v) ++r;
   return r;
}

using y_row = row<std::uint16_t>;  // 8-bit luma values

// GTM datapath (HAS §6.16.4.4-5) and ROI statistics (§6.16.4.2).
class gtm_stage {
public:
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      c_ = &c;
      y_ = 0;
      sum_ = 0;
      auto log2f = [](std::uint32_t v) { unsigned k = 0; while ((2u << k) <= v) ++k; return k; };
      kx_ = std::min(c.gtm_roi_log2, log2f(w));
      ky_ = std::min(c.gtm_roi_log2, log2f(h));
      x0_ = (w - (1u << kx_)) >> 1;  // ALG-GTM-03
      y0_ = (h - (1u << ky_)) >> 1;
   }
   void apply(yuv_row &r) {
      if (y_ >= y0_ && y_ < y0_ + (1u << ky_)) {
         for (std::uint32_t x = x0_; x < x0_ + (1u << kx_); ++x) {
            sum_ += r[x].y;  // input luma, regardless of EN
         }
      }
      ++y_;
      if (!c_->gtm_en || !c_->gtm_curve_valid) {
         return;
      }
      const gtm_curve &R = *c_->gtm_bank;
      for (yuv &p : r) {
         const std::int64_t a = p.y >> 2, f = p.y & 3;
         const std::int64_t ratio = R[a] + ((f * (std::int64_t{R[a + 1]} - R[a])) >> 2);
         const auto chroma = [&](std::int64_t c) {
            return static_cast<std::uint16_t>(std::clamp<std::int64_t>(128 + ((ratio * (c - 128) + 128) >> 8), 0, 255));
         };
         p = {static_cast<std::uint16_t>(std::min<std::int64_t>((ratio * p.y + 128) >> 8, 255)), chroma(p.u), chroma(p.v)};
      }
   }
   std::uint32_t yavg() const {
      return std::max<std::uint32_t>(static_cast<std::uint32_t>(sum_ >> (kx_ + ky_)), 1u);
   }

private:
   const pipeline_config *c_ = nullptr;
   std::uint32_t y_ = 0, kx_ = 0, ky_ = 0, x0_ = 0, y0_ = 0;
   std::uint64_t sum_ = 0;
};

// 2DNR (HAS §6.17.7): median 3x3, one-level Haar, Wiener on LL, BayesShrink on
// LH/HL/HH, inverse Haar; replicate borders; |HH| histogram for the next frame.
class nr2d_stage {
public:
   using sink = std::function<void(y_row &&)>;
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      en_ = c.nr_en;
      c_ = &c;
      v_ = c.nr_var;
      orig_.clear();
      w_ = w;
      h_ = h;
      wb_ = w / 2;
      hb_ = h / 2;
      med_.begin(h);
      sub_.begin(hb_);
      hist_.fill(0);
      have_even_ = false;
      odd_row_.clear();
      out_rows_ = 0;
   }
   void push(y_row &&r, const sink &out) {
      if (!en_) {
         out(std::move(r));
         return;
      }
      orig_.push_back(r);  // the pre-median input for the output blend
      med_.add(r);
      const sink blended = [&](y_row &&o) { emit(std::move(o), out); };
      medians(false, blended);
   }
   void finish(const sink &out) {
      if (!en_) {
         return;
      }
      const sink blended = [&](y_row &&o) { emit(std::move(o), out); };
      medians(true, blended);
      blocks(true, blended);
      if (!odd_row_.empty()) {
         blended(std::move(odd_row_));  // odd last row: median only (Table 6-56)
      }
   }
   std::uint32_t variance() const { return nr_variance(hist_, std::uint64_t{wb_} * hb_); }
   bool enabled() const { return en_; }

private:
   struct band_row {
      std::vector<std::int32_t> ll, lh, hl, hh;
      std::vector<std::array<std::int64_t, 4>> v;  // per group: LL, LH, HL, HH variance (§6.17.3.10)
      std::uint16_t odd_top = 0, odd_bot = 0;  // median of column W-1 when W is odd
   };
   // Output blend with the pre-median input of the same row (MAS v1.07 §6.17.3.10).
   void emit(y_row &&o, const sink &out) {
      const y_row orig = std::move(orig_.front());
      orig_.pop_front();
      const std::int64_t w = c_->nr_blend;
      if (w != 256) {
         for (std::size_t x = 0; x < o.size(); ++x) {
            o[x] = static_cast<std::uint16_t>(((256 - w) * orig[x] + w * o[x] + 128) >> 8);
         }
      }
      out(std::move(o));
   }
   // Base variance of a coefficient group: the automatic estimate of this
   // epoch (else SEED), the manual VARIANCE, or the 17-knot curve at
   // L = (A+B+C+D+2)>>2 of the Haar inputs (V107-A19/A20); then SCALE, the
   // limits and the band scales.
   std::array<std::int64_t, 4> group_variance(std::int64_t sum4) const {
      const pipeline_config &c = *c_;
      std::int64_t base = c.nr_mode == 1 ? c.nr_manual : c.nr_estimate_valid ? v_ : c.nr_seed;
      if (c.nr_mode == 2) {
         const std::int64_t l = (sum4 + 2) >> 2;
         const std::int64_t k = std::min<std::int64_t>(l >> 4, 15);
         const std::int64_t len = k == 15 ? 15 : 16, f = l - 16 * k;
         const std::int64_t d = std::int64_t{c.nr_curve[k + 1]} - c.nr_curve[k];
         base = c.nr_curve[k] + fdiv(f * d + len / 2, len);
      }
      const std::int64_t v = std::clamp<std::int64_t>((base * c.nr_scale + 128) >> 8, c.nr_min, c.nr_max);
      std::array<std::int64_t, 4> vb{};
      for (unsigned b = 0; b < 4; ++b) {
         vb[b] = std::min<std::int64_t>(65535, (v * c.nr_band_scale[b] + 128) >> 8);
      }
      return vb;
   }

   void medians(bool ended, const sink &out) {
      while (med_.ready(ended)) {
         y_row m(w_);
         for (std::uint32_t x = 0; x < w_; ++x) {
            std::array<std::uint16_t, 9> t{};
            unsigned k = 0;
            for (int dy = -1; dy <= 1; ++dy) {
               for (int dx = -1; dx <= 1; ++dx) {
                  const auto xi = static_cast<std::size_t>(std::clamp<std::int64_t>(std::int64_t{x} + dx, 0, w_ - 1));
                  t[k++] = med_.at(dy)[xi];
               }
            }
            std::nth_element(t.begin(), t.begin() + 4, t.end());
            // MEDIAN_EN = 0 feeds the original aligned sample to Haar (§6.17.3.10).
            m[x] = c_->nr_median ? t[4] : med_.at(0)[x];
         }
         const std::uint32_t y = med_.centre();
         med_.advance();
         if (y + 1 == h_ && (h_ & 1u)) {
            odd_row_ = std::move(m);
            continue;
         }
         if (!have_even_) {
            even_ = std::move(m);
            have_even_ = true;
            continue;
         }
         have_even_ = false;
         sub_.add(analysis(even_, m));
         blocks(false, out);
      }
   }
   band_row analysis(const y_row &top, const y_row &bot) {
      band_row b;
      b.ll.resize(wb_);
      b.lh.resize(wb_);
      b.hl.resize(wb_);
      b.hh.resize(wb_);
      b.v.resize(wb_);
      for (std::uint32_t i = 0; i < wb_; ++i) {
         const std::int32_t A = top[2 * i], B = top[2 * i + 1], C = bot[2 * i], D = bot[2 * i + 1];
         b.v[i] = group_variance(std::int64_t{A} + B + C + D);
         b.ll[i] = (A + B + C + D) >> 1;
         b.lh[i] = (A - B + C - D) >> 1;
         b.hl[i] = (A + B - C - D) >> 1;
         b.hh[i] = (A - B - C + D) >> 1;
         ++hist_[static_cast<std::size_t>(std::abs(b.hh[i]))];
      }
      if (w_ & 1u) {
         b.odd_top = top[w_ - 1];
         b.odd_bot = bot[w_ - 1];
      }
      return b;
   }
   template <class F>
   std::int64_t sum3(F &&f, std::uint32_t i) const {  // over the 3x3 of a band plane, replicate
      std::int64_t s = 0;
      for (int dy = -1; dy <= 1; ++dy) {
         for (int dx = -1; dx <= 1; ++dx) {
            const auto xi = static_cast<std::size_t>(std::clamp<std::int64_t>(std::int64_t{i} + dx, 0, wb_ - 1));
            s += f(sub_.at(dy), xi);
         }
      }
      return s;
   }
   std::int64_t shrink(const std::vector<std::int32_t> band_row::*band, std::uint32_t i, std::int64_t v) const {
      const std::int64_t q = sum3([&](const band_row &r, std::size_t xi) {
         const std::int64_t p = (r.*band)[xi];
         return p * p;
      }, i);
      const std::int64_t d9 = std::max<std::int64_t>(q - 9 * v, 0);
      const std::int64_t t = d9 > 0 ? std::min<std::int64_t>(65535, (768 * v) / isqrt(d9)) : 65535;
      const std::int64_t d = (sub_.at(0).*band)[i];
      const std::int64_t mag = std::max<std::int64_t>(0, fdiv(256 * std::abs(d) - t + 128, 256));
      return d < 0 ? -mag : mag;
   }
   void blocks(bool ended, const sink &out) {
      while (sub_.ready(ended)) {
         const band_row &c = sub_.at(0);
         y_row top(w_), bot(w_);
         for (std::uint32_t i = 0; i < wb_; ++i) {
            // Wiener on LL (Table 6-54).
            const std::int64_t s = sum3([](const band_row &r, std::size_t xi) { return std::int64_t{r.ll[xi]}; }, i);
            const std::int64_t q = sum3([](const band_row &r, std::size_t xi) {
               return std::int64_t{r.ll[xi]} * r.ll[xi];
            }, i);
            const std::int64_t V = 9 * q - s * s;
            const std::array<std::int64_t, 4> &vb = c.v[i];  // the centre group's variances (V107-A21)
            const std::int64_t rr = std::min<std::int64_t>(81 * vb[0], V);
            const std::int64_t g = V > 0 ? (256 * (V - rr)) / V : 0;
            const std::int64_t x = c.ll[i];
            const std::int64_t ll = std::clamp<std::int64_t>(fdiv(256 * s + g * (9 * x - s) + 1152, 2304), 0, 510);
            const std::int64_t lh = shrink(&band_row::lh, i, vb[1]), hl = shrink(&band_row::hl, i, vb[2]),
                               hh = shrink(&band_row::hh, i, vb[3]);
            auto sat8 = [](std::int64_t v) { return static_cast<std::uint16_t>(std::clamp<std::int64_t>(v, 0, 255)); };
            top[2 * i] = sat8((ll + lh + hl + hh) >> 1);
            top[2 * i + 1] = sat8((ll - lh + hl - hh) >> 1);
            bot[2 * i] = sat8((ll + lh - hl - hh) >> 1);
            bot[2 * i + 1] = sat8((ll - lh - hl + hh) >> 1);
         }
         if (w_ & 1u) {
            top[w_ - 1] = c.odd_top;
            bot[w_ - 1] = c.odd_bot;
         }
         sub_.advance();
         out(std::move(top));
         out(std::move(bot));
      }
   }

   bool en_ = false;
   const pipeline_config *c_ = nullptr;
   std::deque<y_row> orig_;
   std::uint32_t v_ = 0, w_ = 0, h_ = 0, wb_ = 0, hb_ = 0, out_rows_ = 0;
   row_window<y_row> med_{1, border::replicate};
   row_window<band_row> sub_{1, border::replicate};
   std::array<std::uint32_t, 256> hist_{};
   y_row even_, odd_row_;
   bool have_even_ = false;
};

// EE (HAS §6.18.6): dual-scale high-pass, gain chain, clamp. Enable, profile
// and scalars are the frame's (sampled at SOF); the profile's tables are read
// live row by row, so a write to an in-use table tears the frame (MAS §9.2).
class ee_stage {
public:
   using sink = std::function<void(y_row &&)>;
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      live_ = c.live;
      f_ = c.ee;
      w_ = w;
      h_ = h;
      win_.begin(h);
   }
   void push(y_row &&r, const sink &out) {
      win_.add(r);
      drain(false, out);
   }
   void finish(const sink &out) { drain(true, out); }

private:
   void drain(bool ended, const sink &out) {
      while (win_.ready(ended)) {
         out(compute_row());
         win_.advance();
      }
   }
   y_row compute_row() const {
      const std::uint32_t y = win_.centre();
      y_row o = win_.at(0);
      if (!f_.en || y < 2 || y + 2 >= h_) {
         return o;  // bypass (enable sampled at SOF) or border row
      }
      const auto &s = f_.scalar;
      const std::int64_t alpha = std::min<std::uint32_t>(s[ee_alpha] & 0xFFFFu, 0x8000u);
      const std::int64_t beta = std::min<std::uint32_t>(s[ee_beta] & 0xFFFFu, 0x8000u);
      const std::uint32_t clamp = s[ee_clamp];
      const std::int64_t cpos = clamp & 0xFFu, cneg = (clamp >> 8) & 0xFFu;
      const std::uint32_t feat = s[ee_feature_en];
      const std::uint32_t centre = s[ee_radial_center];
      const std::int64_t cx2 = centre & 0x1FFFu, cy2 = (centre >> 16) & 0x1FFFu;
      const std::int64_t t0 = s[ee_radial_r2_th0] & 0x7FFFFFFu;
      const std::int64_t t1 = s[ee_radial_r2_th1] & 0x7FFFFFFu;
      const std::int64_t t2 = s[ee_radial_r2_th2] & 0x7FFFFFFu;
      const std::uint32_t g01 = s[ee_radial_gain_01], g23 = s[ee_radial_gain_23];
      const std::int64_t gr[4] = {std::min<std::uint32_t>(g01 & 0xFFFFu, 0xC000u), std::min<std::uint32_t>(g01 >> 16, 0xC000u),
                                  std::min<std::uint32_t>(g23 & 0xFFFFu, 0xC000u), std::min<std::uint32_t>(g23 >> 16, 0xC000u)};
      const unsigned p = f_.profile;  // live tables of the frame's profile
      const auto &luma = live_->ee_table(p, 0), &act = live_->ee_table(p, 1), &cp = live_->ee_table(p, 2),
                 &cn = live_->ee_table(p, 3);
      for (std::uint32_t x = 2; x + 2 < w_; ++x) {
         auto P = [&](int dy, int dx) -> std::int64_t { return win_.at(dy)[x + static_cast<std::uint32_t>(dx + 2) - 2]; };
         const std::int64_t c = P(0, 0);
         std::int64_t n8 = 0, lo = 255, hi = 0;
         for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
               if (dy || dx) {
                  n8 += P(dy, dx);
               }
            }
         }
         for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
               lo = std::min(lo, P(dy, dx));
               hi = std::max(hi, P(dy, dx));
            }
         }
         const std::int64_t e_fine = sround(8 * c - n8, 3);
         const std::int64_t h_coarse = 16 * c - 2 * (P(-1, 0) + P(1, 0) + P(0, -1) + P(0, 1)) -
                                       (P(-1, -1) + P(-1, 1) + P(1, -1) + P(1, 1)) - (P(-2, 0) + P(2, 0) + P(0, -2) + P(0, 2));
         const std::int64_t e_coarse = sround(h_coarse, 4);
         const std::int64_t d0 = sround(alpha * e_fine + beta * e_coarse, 15);
         const std::int64_t gl = std::min<std::uint16_t>(luma[static_cast<std::size_t>(c >> 2)], 0x8000u);
         const std::int64_t a_ = std::max(std::abs(e_fine), std::abs(e_coarse));
         const std::int64_t ga = (feat & FX1_ISP_EE_FEATURE_EN_ACTIVITY_EN_MASK)
                                     ? std::min<std::uint16_t>(act[static_cast<std::size_t>(std::min<std::int64_t>(a_ >> 2, 63))], 0x8000u)
                                     : 0x8000;
         const std::size_t ic = static_cast<std::size_t>((hi - lo) >> 3);
         const std::int64_t gc = (feat & FX1_ISP_EE_FEATURE_EN_CONTRAST_EN_MASK)
                                     ? std::min<std::uint16_t>((d0 >= 0 ? cp : cn)[ic], 0x8000u)
                                     : 0x8000;
         std::int64_t g_r = 0x8000;
         if (feat & FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK) {
            const std::int64_t dx2 = 2 * std::int64_t{x} - cx2, dy2 = 2 * std::int64_t{y} - cy2;
            const std::int64_t rr = dx2 * dx2 + dy2 * dy2;
            g_r = rr < t0 ? gr[0] : rr < t1 ? gr[1] : rr < t2 ? gr[2] : gr[3];  // first match (ALG-EE-07)
         }
         const std::int64_t gf = ee_gain_chain(static_cast<std::uint32_t>(gl), static_cast<std::uint32_t>(ga),
                                               static_cast<std::uint32_t>(gc), static_cast<std::uint32_t>(g_r));
         const std::int64_t d1 = sround(d0 * gf, 15);
         const std::int64_t d2 = d1 >= 0 ? std::min(d1, cpos) : std::max(d1, -cneg);
         o[x] = static_cast<std::uint16_t>(std::clamp<std::int64_t>(c + d2, 0, 255));
      }
      return o;
   }
   const control_unit *live_ = nullptr;
   ee_frame_set f_{};
   std::uint32_t w_ = 0, h_ = 0;
   row_window<y_row> win_{2, border::replicate};  // borders are passed through, never filtered
};

// CNF (HAS §6.19.5): 5x5 selective chroma average, replicate (clamped) taps.
using uv = uv_pair;

class cnf_stage {
public:
   using sink = std::function<void(row<uv> &&)>;
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      en_ = c.cnf_en;
      tc_ = c.cnf_chroma_th;
      ty_ = c.cnf_luma_th;
      blend_ = c.cnf_blend;
      w_ = w;
      win_.begin(h);
   }
   void push(const yuv_row &r, const sink &out) {
      if (!en_) {
         row<uv> o(r.size());
         for (std::size_t x = 0; x < r.size(); ++x) {
            o[x] = {r[x].u, r[x].v};
         }
         out(std::move(o));
         return;
      }
      win_.add(r);
      drain(false, out);
   }
   void finish(const sink &out) {
      if (en_) {
         drain(true, out);
      }
   }

private:
   std::uint32_t blend_ = 256;
   void drain(bool ended, const sink &out) {
      while (win_.ready(ended)) {
         row<uv> o(w_);
         const yuv_row &cr = win_.at(0);
         for (std::uint32_t x = 0; x < w_; ++x) {
            const yuv &c = cr[x];
            std::int64_t n = 1, su = c.u, sv = c.v;
            for (int dy = -2; dy <= 2; ++dy) {
               for (int dx = -2; dx <= 2; ++dx) {
                  if (!dy && !dx) {
                     continue;
                  }
                  const auto xi = static_cast<std::size_t>(std::clamp<std::int64_t>(std::int64_t{x} + dx, 0, w_ - 1));
                  const yuv &p = win_.at(dy)[xi];
                  const std::int64_t dyl = std::abs(std::int64_t{p.y} - c.y);
                  const std::int64_t dc = std::abs(std::int64_t{p.u} - c.u) + std::abs(std::int64_t{p.v} - c.v);
                  if (dc <= tc_ && dyl <= ty_) {
                     ++n;
                     su += p.u;
                     sv += p.v;
                  }
               }
            }
            // Output blend with the aligned original (MAS v1.07 §6.19.3.6).
            const std::int64_t w = blend_;
            auto mix = [&](std::int64_t orig, std::int64_t filt) {
               return static_cast<std::uint16_t>(((256 - w) * orig + w * filt + 128) >> 8);
            };
            o[x] = {mix(c.u, (su + n / 2) / n), mix(c.v, (sv + n / 2) / n)};
         }
         win_.advance();
         out(std::move(o));
      }
   }
   bool en_ = false;
   std::int64_t tc_ = 0, ty_ = 0;
   std::uint32_t w_ = 0;
   row_window<yuv_row> win_{2, border::replicate};
};

}  // namespace

std::uint32_t ee_gain_chain(std::uint32_t gl, std::uint32_t ga, std::uint32_t gc, std::uint32_t gr) {
   auto qmul = [](std::uint64_t a, std::uint64_t b) {
      return static_cast<std::uint32_t>(std::min<std::uint64_t>(0xFFFF, (a * b + (1u << 14)) >> 15));
   };
   return qmul(qmul(qmul(gl, ga), gc), gr);
}

std::uint32_t nr_variance(const std::array<std::uint32_t, 256> &hist, std::uint64_t n) {
   std::uint64_t acc = 0, m = 0;
   if (n != 0) {
      for (; m < 255; ++m) {
         acc += hist[m];
         if (2 * acc >= n) {  // lower median: first k with cumulative >= ceil(n/2)
            break;
         }
      }
   }
   const std::uint64_t sq = (m * 97162u) >> 8;
   return static_cast<std::uint32_t>(std::min<std::uint64_t>((sq * sq) >> 16, 65535u));  // ALG-2DNR-07
}

namespace {

// One 2:1 decimation stage (MAS v1.07 §6.20.3.6): the vertical pass, then the
// horizontal pass, each (sum + 4) >> 3 with its own rounding (the order is
// normative; audit 16/10 R1), border taps clamped.
class decimator {
public:
   void begin(std::uint32_t w, std::uint32_t h) {
      w_ = w;
      h_ = h;
      rows_.clear();
      first_ = 0;
      received_ = 0;
      next_ = 0;
   }
   void push(const yuv_row &r, const yuv_sink &out) {
      rows_.push_back(r);
      ++received_;
      drain(false, out);
   }
   void finish(const yuv_sink &out) { drain(true, out); }

private:
   static std::uint16_t fir(std::uint32_t t0, std::uint32_t t1, std::uint32_t t2, std::uint32_t t3) {
      return static_cast<std::uint16_t>(std::min<std::uint32_t>((t0 + 3 * t1 + 3 * t2 + t3 + 4) >> 3, 255));
   }
   yuv_row horizontal(const yuv_row &r) const {
      yuv_row o(w_ / 2);
      const std::int64_t n = w_;
      auto at = [&](std::int64_t i) -> const yuv & { return r[static_cast<std::size_t>(std::clamp<std::int64_t>(i, 0, n - 1))]; };
      for (std::int64_t j = 0; j < std::int64_t{w_ / 2}; ++j) {  // signed: tap 2j-1 is -1 at j = 0
         const yuv &a = at(2 * j - 1), &b = at(2 * j), &c = at(2 * j + 1), &d = at(2 * j + 2);
         o[j] = {fir(a.y, b.y, c.y, d.y), fir(a.u, b.u, c.u, d.u), fir(a.v, b.v, c.v, d.v)};
      }
      return o;
   }
   void drain(bool ended, const yuv_sink &out) {
      while (next_ < h_ / 2) {
         const std::int64_t need = std::min<std::int64_t>(2 * std::int64_t{next_} + 2, h_ - 1);
         if (!ended && need >= received_) {
            return;
         }
         auto at = [&](std::int64_t i) -> const yuv_row & {
            i = std::clamp<std::int64_t>(i, 0, std::int64_t{h_} - 1);
            return rows_.at(static_cast<std::size_t>(i - first_));
         };
         const std::int64_t j = next_;
         const yuv_row &a = at(2 * j - 1), &b = at(2 * j), &c = at(2 * j + 1), &d = at(2 * j + 2);
         yuv_row vert(a.size());  // vertical pass, full width
         for (std::size_t x = 0; x < vert.size(); ++x) {
            vert[x] = {fir(a[x].y, b[x].y, c[x].y, d[x].y), fir(a[x].u, b[x].u, c[x].u, d[x].u),
                       fir(a[x].v, b[x].v, c[x].v, d[x].v)};
         }
         yuv_row o = horizontal(vert);
         ++next_;
         // Rows below 2*next-1 are not needed any more.
         while (!rows_.empty() && first_ < 2 * std::int64_t{next_} - 1) {
            rows_.pop_front();
            ++first_;
         }
         out(std::move(o));
      }
   }
   std::uint32_t w_ = 0, h_ = 0;
   std::deque<yuv_row> rows_;
   std::int64_t first_ = 0, received_ = 0;
   std::uint32_t next_ = 0;
};

// Bilinear DDA (HAS §6.20.9, Tables 6-68/6-69).
class bilinear {
public:
   void begin(const resizer_plan &p) {
      p_ = p;
      rows_.clear();
      first_ = 0;
      received_ = 0;
      next_ = 0;
   }
   void push(const yuv_row &r, const yuv_sink &out) {
      rows_.push_back(r);
      ++received_;
      drain(false, out);
   }
   void finish(const yuv_sink &out) { drain(true, out); }

private:
   void drain(bool ended, const yuv_sink &out) {
      while (next_ < p_.out_h) {
         const std::uint64_t yacc = std::uint64_t{p_.init_y} + std::uint64_t{next_} * p_.step_y;
         const std::int64_t y0 = static_cast<std::int64_t>(yacc >> 16);
         const std::int64_t y1 = std::min<std::int64_t>(y0 + 1, std::int64_t{p_.dec_h} - 1);
         if (!ended && y1 >= received_) {
            return;
         }
         const std::uint64_t beta = yacc & 0xFFFFu;
         const yuv_row &top = rows_.at(static_cast<std::size_t>(y0 - first_));
         const yuv_row &bot = rows_.at(static_cast<std::size_t>(y1 - first_));
         yuv_row o(p_.out_w);
         for (std::uint32_t ox = 0; ox < p_.out_w; ++ox) {
            const std::uint64_t xacc = std::uint64_t{p_.init_x} + std::uint64_t{ox} * p_.step_x;
            const std::size_t x0 = static_cast<std::size_t>(xacc >> 16);
            const std::size_t x1 = std::min<std::size_t>(x0 + 1, p_.dec_w - 1);
            const std::uint64_t alpha = xacc & 0xFFFFu;
            auto interp = [&](std::uint16_t p00, std::uint16_t p10, std::uint16_t p01, std::uint16_t p11) {
               const std::uint64_t ht = (65536u - alpha) * p00 + alpha * p10;
               const std::uint64_t hb = (65536u - alpha) * p01 + alpha * p11;
               const std::uint64_t v = (65536u - beta) * ht + beta * hb;
               return static_cast<std::uint16_t>(std::min<std::uint64_t>((v + (std::uint64_t{1} << 31)) >> 32, 255));
            };
            o[ox] = {interp(top[x0].y, top[x1].y, bot[x0].y, bot[x1].y),
                     interp(top[x0].u, top[x1].u, bot[x0].u, bot[x1].u),
                     interp(top[x0].v, top[x1].v, bot[x0].v, bot[x1].v)};
         }
         ++next_;
         // Rows below the next output's y0 are no longer needed (y0 is monotonic).
         const std::uint64_t ny = std::uint64_t{p_.init_y} + std::uint64_t{next_} * p_.step_y;
         const std::int64_t keep = std::min<std::int64_t>(static_cast<std::int64_t>(ny >> 16), std::int64_t{p_.dec_h} - 1);
         while (!rows_.empty() && first_ < keep && first_ < received_ - 1) {
            rows_.pop_front();
            ++first_;
         }
         out(std::move(o));
      }
   }
   resizer_plan p_{};
   std::deque<yuv_row> rows_;
   std::int64_t first_ = 0, received_ = 0;
   std::uint32_t next_ = 0;
};

class resizer_stage {
public:
   void begin(const resizer_plan &p, std::uint32_t in_w, std::uint32_t in_h) {
      p_ = p;
      std::uint32_t w = in_w, h = in_h;
      for (unsigned s = 0; s < 2; ++s) {
         dec_[s].begin(w, h);
         w /= 2;
         h /= 2;
      }
      bil_.begin(p);
   }
   void push(yuv_row &&r, const yuv_sink &out) {
      if (p_.pass) {
         out(std::move(r));
         return;
      }
      feed(0, std::move(r), out);
   }
   void finish(const yuv_sink &out) {
      if (p_.pass) {
         return;
      }
      for (unsigned s = 0; s < p_.stages; ++s) {
         dec_[s].finish([&](yuv_row &&r) { feed(s + 1, std::move(r), out); });
      }
      bil_.finish(out);
   }

private:
   void feed(unsigned stage, yuv_row &&r, const yuv_sink &out) {
      if (stage < p_.stages) {
         dec_[stage].push(r, [&](yuv_row &&o) { feed(stage + 1, std::move(o), out); });
      } else {
         bil_.push(r, out);
      }
   }
   resizer_plan p_{};
   decimator dec_[2];
   bilinear bil_;
};

// One output channel (MAS v1.07 §6.20.1.1): the post-IQ YUV444 crop (rows and
// columns outside it are consumed without output, local framing regenerated
// from the retained pixels), then the channel's Resizer.
class channel_path {
public:
   void begin(const channel_set &c) {
      c_ = c;
      y_ = 0;
      rsz_.begin(c.plan, c.crop_w, c.crop_h);
   }
   void push(const yuv_row &r, const yuv_sink &out) {
      const std::uint32_t y = y_++;
      if (y < c_.crop_y || y - c_.crop_y >= c_.crop_h) {
         return;
      }
      const auto first = r.begin() + c_.crop_x;
      rsz_.push(yuv_row(first, first + c_.crop_w), out);
   }
   void finish(const yuv_sink &out) { rsz_.finish(out); }

private:
   channel_set c_{};
   std::uint32_t y_ = 0;
   resizer_stage rsz_;
};

}  // namespace

channel_output run_channel(const pipeline_config &cfg, unsigned ch, const std::vector<yuv> &frame,
                           std::uint32_t w, std::uint32_t h) {
   const channel_set c = channel_for_frame(cfg, geometry{w, h}, ch);
   channel_output o;
   o.width = c.out.width;
   o.height = c.out.height;
   std::uint32_t oy = 0;
   const yuv_sink sink = [&](yuv_row &&r) {
      for (std::size_t x = 0; x < r.size(); ++x) {
         if (ch == 2) {  // RGB888: bytes R, G, B in ascending address order (Table 6-147)
            const rgb8 p = rgb888_pixel(cfg.csc_std, cfg.csc_full_range, r[x].y, r[x].u, r[x].v);
            o.rgb.insert(o.rgb.end(), {p.r, p.g, p.b});
         } else {
            o.y.push_back(static_cast<std::uint8_t>(r[x].y));
            if (oy % 2u == 0 && x % 2u == 0) {  // NV12: top-left chroma of each 2 x 2 (§6.24.3)
               o.uv.push_back(static_cast<std::uint8_t>(r[x].u));
               o.uv.push_back(static_cast<std::uint8_t>(r[x].v));
            }
         }
      }
      ++oy;
   };
   channel_path path;
   path.begin(c);
   for (std::uint32_t y = 0; y < h; ++y) {
      const auto first = frame.begin() + std::ptrdiff_t{y} * w;
      path.push(yuv_row(first, first + w), sink);
   }
   path.finish(sink);
   return o;
}

// AEC (MAS v1.07 §6.21.3): one RAW12 Bayer sample is the unit, at the
// frame's tap (post-BPC or post-DG). Clip-qualified sum/count, strict OE/UE,
// extrema and the 64-bin histogram over every sample, globally and per zone
// zx = min(x / WZ, NX - 1); the weighted histogram over the ROI lattice with
// the 5x5 spatial weights (§6.21.3.4). 64-bit saturating accumulators.
class aec_stage {
public:
   void begin(const aec_config &c, aec_payload &p, std::uint32_t w, std::uint32_t h) {
      c_ = c;
      p_ = &p;
      y_ = 0;
      p = aec_payload{};
      if (c.en) {
         p.restart(c, w, h);
      }
      x0_ = p.roi_start & 0xFFFFu;
      y0_ = p.roi_start >> 16;
      x1_ = p.roi_end & 0xFFFFu;
      y1_ = p.roi_end >> 16;
   }
   bool enabled() const { return c_.en; }
   void push(const row<std::uint16_t> &in) {
      if (!c_.en) {
         return;
      }
      aec_payload &p = *p_;
      const std::uint32_t y = y_++;
      const std::size_t zrow = std::size_t{std::min(y / c_.zh, c_.ny - 1)} * c_.nx;
      // Weighted lattice row: inside the ROI and on the vertical step.
      const bool wrow = c_.hist.en && y >= y0_ && y < y1_ && (y - y0_) % c_.hist.sy == 0;
      const std::uint32_t cy = wrow ? std::min(5u * (y - y0_) / (y1_ - y0_), 4u) : 0u;
      for (std::uint32_t x = 0; x < in.size(); ++x) {
         const std::uint32_t s = in[x] & 0xFFFu;
         aec_zone &z = p.zones[zrow + std::min(x / c_.zw, c_.nx - 1)];
         if (s >= c_.min_clip && s <= c_.max_clip) {  // inclusive both ends
            sat_add(p.sum, s, p.overflow);
            sat_add(p.count, 1, p.overflow);
            sat_add(z.sum, s, p.overflow);
            sat_add(z.count, 1, p.overflow);
         }
         if (s > c_.th_oe) {  // strict, independent of the clip qualification
            sat_add(p.oe, 1, p.overflow);
            sat_add(z.oe, 1, p.overflow);
         }
         if (s < c_.th_ue) {
            sat_add(p.ue, 1, p.overflow);
            sat_add(z.ue, 1, p.overflow);
         }
         p.min = std::min(p.min, s);
         p.max = std::max(p.max, s);
         z.min = std::min(z.min, s);
         z.max = std::max(z.max, s);
         sat_add(p.hist[s >> 6], 1, p.overflow);
         if (wrow && x >= x0_ && x < x1_ && (x - x0_) % c_.hist.sx == 0) {
            const std::uint32_t cx = std::min(5u * (x - x0_) / (x1_ - x0_), 4u);
            const std::uint32_t w = c_.hist.w[cy * 5 + cx];
            if (w) {  // zero weight contributes nothing
               sat_add(p.whist[s >> 6], w, p.overflow);
               sat_add(p.wtotal, w, p.overflow);
               sat_add(p.wsum, std::uint64_t{w} * s, p.overflow);
            }
         }
      }
   }

private:
   aec_config c_{};
   aec_payload *p_ = nullptr;
   std::uint32_t y_ = 0, x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

// AWB (MAS v1.07 §6.22.3): complete 2x2 post-BPC cells, R(2i,2j), Gr(2i+1,2j),
// Gb(2i,2j+1), B(2i+1,2j+1), formed at the bottom-row sample. A cell counts
// when all four samples are strictly inside the limits, then the component
// limits, the cell ROI and the ellipse union (§6.22.3.9) pass; G is the
// half-up mean of Gr and Gb. Zones over the cell raster Wc x Hc:
// zone = min(((i+1)*NX - 1) / Wc, NX - 1).
class awb_stage {
public:
   void begin(const awb_config &c, awb_payload &p, std::uint32_t w, std::uint32_t h) {
      c_ = c;
      p_ = &p;
      wc_ = w / 2;
      hc_ = h / 2;
      y_ = 0;
      p = awb_payload{};
      if (c.en) {
         p.mem.restart(c);
      }
   }
   void push(const row<std::uint16_t> &in) {
      if (!c_.en) {
         return;
      }
      const std::uint32_t y = y_++;
      if ((y & 1u) == 0) {
         top_ = in;  // the prior-row buffer holds R/Gr
         return;
      }
      const std::uint64_t j = y / 2;
      if (j >= hc_) {
         return;
      }
      const std::size_t zrow = std::size_t{std::min<std::uint64_t>(((j + 1) * c_.ny - 1) / hc_, c_.ny - 1)} * c_.nx;
      for (std::uint64_t i = 0; i < wc_; ++i) {
         const std::uint32_t r = top_[2 * i] & 0xFFFu, gr = top_[2 * i + 1] & 0xFFFu;
         const std::uint32_t gb = in[2 * i] & 0xFFFu, b = in[2 * i + 1] & 0xFFFu;
         if (std::min({r, gr, gb, b}) <= c_.under || std::max({r, gr, gb, b}) >= c_.sat) {
            continue;  // equality at either limit rejects the cell
         }
         const std::uint32_t g = (gr + gb + 1) >> 1;
         if (!qualify(r, g, b, static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j))) {
            continue;
         }
         awb_result &t = p_->global;
         t.r += r;
         t.g += g;
         t.b += b;
         ++t.n;
         const std::size_t z = zrow + std::min<std::uint64_t>(((i + 1) * c_.nx - 1) / wc_, c_.nx - 1);
         p_->mem.zsum[z][0] += r;
         p_->mem.zsum[z][1] += g;
         p_->mem.zsum[z][2] += b;
         ++p_->mem.zcnt[z];
      }
   }

private:
   bool qualify(std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t i, std::uint32_t j) const {
      const std::uint32_t v[3] = {r, g, b};
      for (unsigned k = 0; k < 3; ++k) {  // inclusive component limits
         if (v[k] < c_.limits[k][0] || v[k] > c_.limits[k][1]) {
            return false;
         }
      }
      if (c_.roi_en && !(i >= c_.roi[0] && i < c_.roi[2] && j >= c_.roi[1] && j < c_.roi[3])) {
         return false;  // half-open cell ROI
      }
      if (!c_.color_en) {
         return true;
      }
      if (g < c_.min_g || g == 0) {
         return false;
      }
      // rg = R(256 R, G), bg = R(256 B, G) with R(n, d) = floor((n + floor(d/2)) / d).
      const std::int64_t rg = (256 * std::int64_t{r} + g / 2) / g, bg = (256 * std::int64_t{b} + g / 2) / g;
      if (rg > 65535 || bg > 65535) {
         return false;  // rejected rather than clipped into a region
      }
      for (unsigned k = 0; k < 8; ++k) {  // union; an empty mask accepts nothing
         if (!(c_.mask & (1u << k))) {
            continue;
         }
         const awb_ellipse &e = c_.ellipses[k];
         const std::int64_t dr = rg - e.rg, db = bg - e.bg;
         const std::int64_t q = std::int64_t{e.a} * dr * dr + 2 * std::int64_t{e.b} * dr * db + std::int64_t{e.c} * db * db;
         if (q <= std::int64_t{e.r2} * 16384) {
            return true;
         }
      }
      return false;
   }
   awb_config c_{};
   awb_payload *p_ = nullptr;
   std::uint64_t wc_ = 0, hc_ = 0;
   std::uint32_t y_ = 0;
   row<std::uint16_t> top_;
};

// AF (MAS v1.07 §6.23.3): combined Sobel |gx| + |gy| of the CSC luma with
// replicate borders, minus NOISE floored at zero; per window the focus sum,
// the centre luma sum and the count, over the effective rectangles (uniform
// 4x4 grid or sixteen programmed ones; overlaps count independently).
// Observer only: the stream is not modified. EN is the value sampled at SOF.
class af_stage {
public:
   void begin(const af_config &c, af_payload &p, std::uint32_t w, std::uint32_t h) {
      c_ = c;
      p_ = &p;
      w_ = w;
      p = af_payload{};
      if (c.en) {
         p.restart(c, w, h);
      }
      win_.begin(h);
   }
   void push(const yuv_row &in) {
      y_row y(in.size());
      for (std::size_t x = 0; x < in.size(); ++x) {
         y[x] = in[x].y;
      }
      win_.add(y);
      drain(false);
   }
   void finish() { drain(true); }

private:
   void drain(bool ended) {
      while (win_.ready(ended)) {
         const std::uint32_t yc = win_.centre();
         if (c_.en) {
            const y_row &a = win_.at(-1), &b = win_.at(0), &d = win_.at(1);
            dsum_.assign(std::size_t{w_} + 1, 0);
            lsum_.assign(std::size_t{w_} + 1, 0);
            for (std::uint32_t x = 0; x < w_; ++x) {  // prefix sums of the row's metric and luma
               const std::uint32_t xl = x ? x - 1 : 0, xr = std::min(x + 1, w_ - 1);
               const std::int32_t gx = (a[xr] + 2 * b[xr] + d[xr]) - (a[xl] + 2 * b[xl] + d[xl]);
               const std::int32_t gy = (d[xl] + 2 * d[x] + d[xr]) - (a[xl] + 2 * a[x] + a[xr]);
               const std::int64_t m = std::max<std::int64_t>(std::abs(gx) + std::abs(gy) - std::int64_t{c_.noise}, 0);
               dsum_[x + 1] = dsum_[x] + static_cast<std::uint64_t>(m);
               lsum_[x + 1] = lsum_[x] + b[x];
            }
            for (unsigned z = 0; z < 16; ++z) {
               const auto &r = p_->rects[z];
               if (yc < r[1] || yc >= r[3] || r[0] >= r[2]) {
                  continue;
               }
               const std::uint32_t x1 = std::min(r[2], w_);
               if (r[0] >= x1) {
                  continue;
               }
               af_window &wz = p_->win[z];
               sat_add(wz.focus, dsum_[x1] - dsum_[r[0]], p_->overflow);
               sat_add(wz.luma, lsum_[x1] - lsum_[r[0]], p_->overflow);
               sat_add(wz.count, x1 - r[0], p_->overflow);
            }
         }
         win_.advance();
      }
   }
   af_config c_{};
   af_payload *p_ = nullptr;
   std::uint32_t w_ = 0;
   std::vector<std::uint64_t> dsum_, lsum_;
   row_window<y_row> win_{1, border::replicate};
};

struct isp_pipeline::impl {
   pipeline_config cfg;
   std::uint32_t in_w = 0, in_h = 0, sh = 0, sv = 0;
   geometry active{};
   std::array<channel_set, num_channels> sets{};
   std::array<isp_pipeline::channel_sinks, num_channels> sinks{};
   std::array<std::uint32_t, num_channels> y_out{};
   std::uint32_t y_in = 0;
   std::uint32_t y_bayer = 0;  // row index after the Input Formatter crop
   lsc_stage lsc;
   bpc_stage bpc;
   gtm_stage gtm;
   nr2d_stage nr;
   ee_stage ee;
   cnf_stage cnf;
   std::deque<y_row> join_y;    // luma path output (2DNR -> EE)
   std::deque<row<uv>> join_uv; // chroma path output (CNF)
   std::uint32_t y_post_bpc = 0;
   frame_stats stats;
   demosaic_stage dms;
   std::array<channel_path, num_channels> paths;
   isp_pipeline::yuv_tap tap;
   aec_stage aec;
   awb_stage awb;
   af_stage af;

   // WB and DG after BPC, then Demosaic.
   void bayer_row(row<std::uint16_t> &&r) {
      const std::uint32_t y = y_post_bpc++;
      awb.push(r);  // tap: post-BPC Bayer cells (MAS §6.22.1)
      if (cfg.aec.source == 0) {
         aec.push(r);  // AE tap: post-BPC (TUNE_AEC_SOURCE = 0)
      }
      for (std::uint32_t x = 0; x < r.size(); ++x) {
         const unsigned ch = 2u * (y & 1u) + (x & 1u);
         r[x] = dg_pixel(cfg, wb_pixel(cfg, r[x], ch));
      }
      if (cfg.aec.source != 0) {
         aec.push(r);  // AE tap: post-DG, after white balance (TUNE_AEC_SOURCE = 1)
      }
      dms.push(r, [&](row<rgb> &&o) { rgb_row(std::move(o)); });
   }
   // After GTM the luma path (2DNR -> EE) and the chroma path (CNF, which
   // compares against the GTM luma) run separately and are re-joined by row
   // before the Resizer: Y from EE, U/V from CNF (ALG-CNF-01).
   void yuv_rows(yuv_row &&r) {
      gtm.apply(r);
      y_row y(r.size());
      for (std::size_t x = 0; x < r.size(); ++x) {
         y[x] = r[x].y;
      }
      cnf.push(r, [this](row<uv> &&o) { join_uv.push_back(std::move(o)); join(); });
      nr.push(std::move(y), [this](y_row &&o) { luma(std::move(o)); });
   }
   void luma(y_row &&r) {
      ee.push(std::move(r), [this](y_row &&o) { join_y.push_back(std::move(o)); join(); });
   }
   void join() {
      while (!join_y.empty() && !join_uv.empty()) {
         const y_row &yy = join_y.front();
         const row<uv> &cc = join_uv.front();
         yuv_row o(yy.size());
         for (std::size_t x = 0; x < yy.size(); ++x) {
            o[x] = {yy[x], cc[x].u, cc[x].v};
         }
         join_y.pop_front();
         join_uv.pop_front();
         if (tap) {
            tap(o);
         }
         for (unsigned c = 0; c < num_channels; ++c) {  // the distributor: every enabled channel
            if (sinks[c].y) {
               paths[c].push(o, [this, c](yuv_row &&z) { ofmt(c, z); });
            }
         }
      }
   }
   void rgb_row(row<rgb> &&r) {
      yuv_row o(r.size());
      for (std::size_t x = 0; x < r.size(); ++x) {
         rgb p = ccm_pixel(cfg, r[x]);
         if (cfg.gamma_en) {  // HAS p95: index = top 8 bits, live table
            const auto &lut = *cfg.gamma_lut;
            p = {lut[p.r >> 4], lut[p.g >> 4], lut[p.b >> 4]};
         }
         o[x] = csc_pixel(cfg, p);
      }
      af.push(o);  // inline after CSC, before GTM
      yuv_rows(std::move(o));
   }
   void ofmt(unsigned c, const yuv_row &r) {
      if (c == 2) {  // CH2 RGB888 converter (MAS v1.07 §6.24.7): bytes R, G, B
         std::vector<std::uint8_t> b(3u * r.size());
         for (std::size_t x = 0; x < r.size(); ++x) {
            const rgb8 p = rgb888_pixel(cfg.csc_std, cfg.csc_full_range, r[x].y, r[x].u, r[x].v);
            b[3 * x] = p.r;
            b[3 * x + 1] = p.g;
            b[3 * x + 2] = p.b;
         }
         sinks[c].y(std::move(b));
         ++y_out[c];
         return;
      }
      // NV12 formatter (MAS §6.24): Y as is, UV = top-left cosited.
      std::vector<std::uint8_t> yb(r.size());
      for (std::size_t x = 0; x < r.size(); ++x) {
         yb[x] = static_cast<std::uint8_t>(r[x].y);
      }
      sinks[c].y(std::move(yb));
      if (y_out[c] % 2u == 0) {
         std::vector<std::uint8_t> uv(r.size());
         for (std::size_t i = 0; i + 1 < r.size(); i += 2) {
            uv[i] = static_cast<std::uint8_t>(r[i].u);
            uv[i + 1] = static_cast<std::uint8_t>(r[i].v);
         }
         sinks[c].uv(std::move(uv));
      }
      ++y_out[c];
   }
};

isp_pipeline::isp_pipeline() : impl_(std::make_unique<impl>()) {}
isp_pipeline::~isp_pipeline() = default;

frame_geometry isp_pipeline::begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                                   y_sink y, uv_sink uv, warning_sink warn, yuv_tap tap) {
   std::array<channel_sinks, num_channels> s{};
   s[0] = {std::move(y), std::move(uv)};
   return begin(cfg, in_width, in_height, s, std::move(warn), std::move(tap));
}

frame_geometry isp_pipeline::output(unsigned ch) const {
   const channel_set &c = impl_->sets.at(ch);
   return {c.out.width, c.out.height};
}

frame_geometry isp_pipeline::begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                                   const std::array<channel_sinks, num_channels> &sinks, warning_sink warn,
                                   yuv_tap tap) {
   impl &m = *impl_;
   m.cfg = cfg;
   m.in_w = in_width;
   m.in_h = in_height;
   m.sh = cfg.bayer_pattern & 1u;
   m.sv = (cfg.bayer_pattern >> 1) & 1u;
   m.active = control_unit::derive_active(in_width, in_height, cfg.bayer_pattern);
   m.tap = std::move(tap);
   m.sinks = sinks;
   m.y_in = m.y_bayer = m.y_post_bpc = 0;
   m.y_out = {};
   m.stats = frame_stats{};
   if (warn) {
      if (cfg.ofmt_stride_en) {
         warn("OFMT stride padding requested: not modelled, the ODMA applies its own stride (DEC-39)");
      }
      if (cfg.wdr_en) {
         warn("D_WDR enabled: WDR is not supported (HAS 3.1), register storage only (M5-A1)");
      }
      if (cfg.tnr_en) {
         warn("TNR_3D enabled: 3DNR is not supported (HAS 3.1), register storage only (M5-A1)");
      }
   }
   m.lsc.begin(m.cfg, m.active.width, m.active.height);
   m.bpc.begin(m.cfg, m.active.width, m.active.height);
   m.gtm.begin(m.cfg, m.active.width, m.active.height);
   m.nr.begin(m.cfg, m.active.width, m.active.height);
   m.ee.begin(m.cfg, m.active.width, m.active.height);
   m.cnf.begin(m.cfg, m.active.width, m.active.height);
   m.join_y.clear();
   m.join_uv.clear();
   m.dms.begin(m.active.width, m.active.height);
   m.dms.set_margins(m.cfg.dms_margin_green, m.cfg.dms_margin_rb);
   for (unsigned c = 0; c < num_channels; ++c) {
      m.sets[c] = channel_for_frame(cfg, m.active, c);
      m.paths[c].begin(m.sets[c]);
   }
   m.aec.begin(m.cfg.aec, m.stats.aec, m.active.width, m.active.height);
   m.awb.begin(m.cfg.awb, m.stats.awb, m.active.width, m.active.height);
   m.af.begin(m.cfg.af, m.stats.af, m.active.width, m.active.height);
   return {m.sets[0].out.width, m.sets[0].out.height};
}

void isp_pipeline::push(const std::uint16_t *samples) {
   impl &m = *impl_;
   const std::uint32_t y = m.y_in++;
   if (y < m.sv || y - m.sv >= m.active.height) {
      return;  // Input Formatter: row cropped
   }
   row<std::uint16_t> bayer(m.active.width);
   const std::uint32_t yb = m.y_bayer++;
   for (std::uint32_t x = 0; x < m.active.width; ++x) {
      std::uint16_t p = normalize_input(samples[x + m.sh], m.cfg.input_depth);
      const unsigned ch = 2u * (yb & 1u) + (x & 1u);
      p = blc_pixel(m.cfg, p, ch);
      if (m.lsc.on()) {
         p = m.lsc.pixel(p, x, yb, ch);
      }
      bayer[x] = p;
   }
   m.bpc.push(bayer, [&](row<std::uint16_t> &&r) { m.bayer_row(std::move(r)); });
}

void isp_pipeline::finish() {
   impl &m = *impl_;
   m.bpc.finish([&](row<std::uint16_t> &&r) { m.bayer_row(std::move(r)); });
   m.dms.finish([&](row<rgb> &&r) { m.rgb_row(std::move(r)); });
   m.af.finish();
   m.cnf.finish([&](row<uv> &&o) { m.join_uv.push_back(std::move(o)); m.join(); });
   m.nr.finish([&](y_row &&o) { m.luma(std::move(o)); });
   m.ee.finish([&](y_row &&o) { m.join_y.push_back(std::move(o)); m.join(); });
   for (unsigned c = 0; c < num_channels; ++c) {
      if (m.sinks[c].y) {
         m.paths[c].finish([&m, c](yuv_row &&o) { m.ofmt(c, o); });
      }
   }
   m.stats.gtm_yavg = m.gtm.yavg();
   m.stats.nr_measured = m.nr.enabled();
   m.stats.nr_var = m.nr.variance();
   m.stats.lsc_overflow = m.lsc.overflow();
   m.stats.bpc_candidates = m.bpc.candidates();
   m.stats.bpc_defective = m.bpc.defective();
}

const frame_stats &isp_pipeline::stats() const { return impl_->stats; }

std::vector<std::uint16_t> run_nr2d(const std::vector<std::uint16_t> &y, std::uint32_t w, std::uint32_t h,
                                    std::uint32_t var_in, std::uint32_t &var_out) {
   pipeline_config c;
   c.nr_en = true;
   c.nr_var = var_in;
   c.nr_estimate_valid = true;  // block vectors chain the estimate (reference NR_LEGACY)
   return run_nr2d(c, y, w, h, var_out);
}

std::vector<std::uint16_t> run_nr2d(const pipeline_config &c, const std::vector<std::uint16_t> &y, std::uint32_t w,
                                    std::uint32_t h, std::uint32_t &var_out) {
   nr2d_stage st;
   st.begin(c, w, h);
   std::vector<std::uint16_t> out;
   auto sink = [&](y_row &&r) { out.insert(out.end(), r.begin(), r.end()); };
   for (std::uint32_t row_i = 0; row_i < h; ++row_i) {
      st.push(y_row(y.begin() + std::ptrdiff_t{row_i} * w, y.begin() + std::ptrdiff_t{row_i + 1} * w), sink);
   }
   st.finish(sink);
   var_out = st.variance();
   return out;
}

std::vector<std::uint16_t> run_ee(const std::vector<std::uint16_t> &y, std::uint32_t w, std::uint32_t h,
                                  const control_unit &live) {
   pipeline_config c;
   c.live = &live;
   c.ee = live.ee_frame();  // the set sampled at the last accepted SOF
   ee_stage st;
   st.begin(c, w, h);
   std::vector<std::uint16_t> out;
   auto sink = [&](y_row &&r) { out.insert(out.end(), r.begin(), r.end()); };
   for (std::uint32_t row_i = 0; row_i < h; ++row_i) {
      st.push(y_row(y.begin() + std::ptrdiff_t{row_i} * w, y.begin() + std::ptrdiff_t{row_i + 1} * w), sink);
   }
   st.finish(sink);
   return out;
}

std::vector<uv_pair> run_cnf(const std::vector<yuv> &img, std::uint32_t w, std::uint32_t h, std::uint32_t tc,
                             std::uint32_t ty) {
   pipeline_config c;
   c.cnf_en = true;
   c.cnf_chroma_th = tc;
   c.cnf_luma_th = ty;
   return run_cnf(c, img, w, h);
}

std::vector<uv_pair> run_cnf(const pipeline_config &c, const std::vector<yuv> &img, std::uint32_t w,
                             std::uint32_t h) {
   cnf_stage st;
   st.begin(c, w, h);
   std::vector<uv_pair> out;
   auto sink = [&](row<uv_pair> &&r) { out.insert(out.end(), r.begin(), r.end()); };
   for (std::uint32_t row_i = 0; row_i < h; ++row_i) {
      st.push(yuv_row(img.begin() + std::ptrdiff_t{row_i} * w, img.begin() + std::ptrdiff_t{row_i + 1} * w), sink);
   }
   st.finish(sink);
   return out;
}

void end_of_frame(control_unit &ctl, const pipeline_config &cfg, const frame_stats &st) {
   const std::uint32_t lsc_regs[4] = {FX1_ISP_LSC_STAT_OVF_CNT_R_OFFSET, FX1_ISP_LSC_STAT_OVF_CNT_GR_OFFSET,
                                      FX1_ISP_LSC_STAT_OVF_CNT_GB_OFFSET, FX1_ISP_LSC_STAT_OVF_CNT_B_OFFSET};
   for (unsigned c = 0; c < 4; ++c) {  // 21-bit counters; bypass frames publish 0 (ALG-LSC-08)
      ctl.hw_write(lsc_regs[c], 0x1FFFFFu, std::min<std::uint32_t>(st.lsc_overflow[c], 0x1FFFFFu));
   }
   if (cfg.bpc_en) {  // counted during an enabled frame, 14-bit saturating (MAS §6.9, DEC-28 superseded)
      ctl.hw_write(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET, 0x3FFFu, std::min<std::uint32_t>(st.bpc_candidates, 0x3FFFu));
      ctl.hw_write(FX1_ISP_BPC_NUM_DEFECTIVE_OFFSET, 0x3FFFu, std::min<std::uint32_t>(st.bpc_defective, 0x3FFFu));
   }
   ctl.gtm_end_of_frame(st.gtm_yavg);  // stats and rebuild run regardless of EN (HAS p105)
   ctl.ee_end_of_frame();              // radial gain range event, once per processed frame
   if (st.nr_measured) {
      ctl.nr_publish(st.nr_var);         // held while 2DNR is disabled (ALG-2DNR-09)
   }
   // Tap order AEC -> AWB -> AF; an engine that did not measure the frame
   // keeps its banks (ALG-STAT-06).
   ctl.stats_publish(cfg.aec.en ? &st.aec : nullptr, cfg.awb.en ? &st.awb : nullptr, cfg.af.en ? &st.af : nullptr);
}

}  // namespace cdc::components::fx1_isp::pipe
