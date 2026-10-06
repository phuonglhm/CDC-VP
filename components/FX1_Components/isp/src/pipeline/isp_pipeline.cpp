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

std::uint16_t sat12(std::int64_t v) { return static_cast<std::uint16_t>(std::clamp<std::int64_t>(v, 0, 4095)); }

std::int64_t mirror(std::int64_t i, std::int64_t n) { return border_index(i, n, border::mirror); }

}  // namespace

// ---- configuration ----------------------------------------------------------

pipeline_config snapshot_config(control_unit &ctl) {
   const register_file &r = ctl.regs();
   auto reg = [&](std::uint32_t off) { return r.peek(off); };
   pipeline_config c;
   c.bayer_pattern = reg(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;

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
   c.bpc_dynamic_requested = reg(FX1_ISP_BPC_MODE_OFFSET) & FX1_ISP_BPC_MODE_DYNAMIC_DET_EN_MASK;
   const std::uint32_t th = reg(FX1_ISP_BPC_THRESH_OFFSET);
   c.bpc_floor = th & FX1_ISP_BPC_THRESH_THRESH_FLOOR_MASK;
   c.bpc_k = (th & FX1_ISP_BPC_THRESH_THRESH_K_MASK) >> FX1_ISP_BPC_THRESH_THRESH_K_SHIFT;

   c.wb_en = reg(FX1_ISP_WB_CTRL_OFFSET) & FX1_ISP_WB_CTRL_EN_MASK;
   c.wb_gain = {reg(FX1_ISP_WB_GAIN_R_OFFSET) & 0xFFFu, reg(FX1_ISP_WB_GAIN_G_OFFSET) & 0xFFFu,
                reg(FX1_ISP_WB_GAIN_B_OFFSET) & 0xFFFu};
   c.dg_en = reg(FX1_ISP_DG_CTRL_OFFSET) & FX1_ISP_DG_CTRL_EN_MASK;
   c.dg_gain = reg(FX1_ISP_DG_GAIN_OFFSET) & FX1_ISP_DG_GAIN_GAIN_MASK;

   c.ccm_en = reg(FX1_ISP_CCM_CTRL_OFFSET) & FX1_ISP_CCM_CTRL_EN_MASK;
   for (unsigned row = 0; row < 3; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
         c.ccm[row][col] = sext12(ctl.committed(FX1_ISP_CCM_CRR_OFFSET + 4u * (3u * row + col)) & 0xFFFu);
      }
      c.ccm_offset[row] = sext12(ctl.committed(FX1_ISP_CCM_OFS_R_OFFSET + 4u * row) & 0xFFFu);
   }

   c.gtm_en = reg(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_EN_MASK;
   c.gtm_curve_valid = ctl.gtm_curve_valid();
   c.gtm_bank = &ctl.gtm_read_bank();
   c.gtm_roi_log2 = reg(FX1_ISP_GTM_ROI_LOG2_OFFSET) & FX1_ISP_GTM_ROI_LOG2_ROI_LOG2_MASK;
   c.nr_en = reg(FX1_ISP_NR_2D_CTRL_OFFSET) & FX1_ISP_NR_2D_CTRL_EN_MASK;
   c.nr_var = ctl.nr_variance();
   c.live = &ctl;
   c.cnf_en = reg(FX1_ISP_CNF_CTRL_OFFSET) & FX1_ISP_CNF_CTRL_EN_MASK;
   c.cnf_chroma_th = ctl.committed(FX1_ISP_CNF_CHROMA_TH_OFFSET) & FX1_ISP_CNF_CHROMA_TH_CHROMA_TH_MASK;
   c.cnf_luma_th = ctl.committed(FX1_ISP_CNF_LUMA_TH_OFFSET) & FX1_ISP_CNF_LUMA_TH_LUMA_TH_MASK;

   c.gamma_en = reg(FX1_ISP_GAMMA_CTRL_OFFSET) & FX1_ISP_GAMMA_CTRL_EN_MASK;
   c.gamma_lut = &ctl.gamma_lut();
   c.csc_std = reg(FX1_ISP_CSC_CTRL_OFFSET) & FX1_ISP_CSC_CTRL_STD_MASK;

   c.resizer_en = reg(FX1_ISP_RESIZER_CTRL_OFFSET) & FX1_ISP_RESIZER_CTRL_EN_MASK;
   c.resizer_scale = fld(ctl.committed(FX1_ISP_RESIZER_CTRL_OFFSET), FX1_ISP_RESIZER_CTRL_SCALE_MASK,
                         FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);

   c.aec = ctl.aec_active();
   c.awb = ctl.awb_frame_config();
   c.af_en = ctl.af_frame_enabled();
   c.ofmt_stride_en = reg(FX1_ISP_OFMT_CTRL_OFFSET) & FX1_ISP_OFMT_CTRL_STRIDE_EN_MASK;
   c.wdr_en = reg(FX1_ISP_D_WDR_CTRL_OFFSET) & FX1_ISP_D_WDR_CTRL_EN_MASK;
   c.tnr_en = reg(FX1_ISP_TNR_3D_CTRL_OFFSET) & FX1_ISP_TNR_3D_CTRL_EN_MASK;
   c.aec_mem = &ctl.aec_zone_memory();
   c.awb_mem = &ctl.awb_zone_memory();
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
   const std::uint32_t gain = ch == 0 ? c.wb_gain[0] : ch == 3 ? c.wb_gain[2] : c.wb_gain[1];
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

yuv csc_pixel(std::uint32_t std_sel, const rgb &p) {
   // HAS Table 6-45, Q16: LKR, LKB, RSY, RSCB, RSCR.
   static constexpr std::int64_t k[2][5] = {{19595, 7471, 3505, 2023, 2557}, {13933, 4732, 3505, 1932, 2276}};
   const std::int64_t *c = k[std_sel & 1u];
   const std::int64_t r = p.r, g = p.g, b = p.b;
   const std::int64_t yp = c[0] * (r - g) + (g << 16) + c[1] * (b - g);
   const std::int64_t db = (b << 16) - yp;
   const std::int64_t dr = (r << 16) - yp;
   auto rnd = [](std::int64_t v) { return (v + (std::int64_t{1} << 31)) >> 32; };  // floor(x+1/2), ALG-CSC-01
   auto sat8 = [](std::int64_t v) { return static_cast<std::uint16_t>(std::clamp<std::int64_t>(v, 0, 255)); };
   return {sat8(rnd(c[2] * yp) + 16), sat8(rnd(c[3] * db) + 128), sat8(rnd(c[4] * dr) + 128)};
}

// ---- Resizer plan (HAS §6.20.9-6.20.10, DEC-23) ------------------------------

resizer_plan plan_resizer(bool en, std::uint32_t scale, std::uint32_t in_w, std::uint32_t in_h) {
   static constexpr std::uint32_t modes[16][2] = {{0, 0},       {3840, 2160}, {2560, 1440}, {2880, 1620},
                                                  {2304, 1296}, {1920, 1080}, {1280, 720},  {960, 540},
                                                  {640, 360},   {2592, 1944}, {2048, 1536}, {1600, 1200},
                                                  {1280, 960},  {800, 600},   {640, 480},   {0, 0}};
   resizer_plan p{};
   const std::uint32_t mw = modes[scale & 0xFu][0];
   const std::uint32_t mh = modes[scale & 0xFu][1];
   p.pass = !en || mw == 0 || mw > in_w || mh > in_h;
   if (p.pass) {
      p.out_w = in_w;
      p.out_h = in_h;
      return p;
   }
   p.out_w = mw;
   p.out_h = mh;
   unsigned k = 2;
   for (unsigned t = 0; t <= 2; ++t) {
      if ((in_w >> t) < 2u * mw && (in_h >> t) < 2u * mh) {
         k = t;
         break;
      }
   }
   while (k > 0 && ((in_w >> k) < mw || (in_h >> k) < mh)) {
      --k;  // DEC-23: never decimate below the output size
   }
   p.stages = k;
   p.dec_w = in_w >> k;
   p.dec_h = in_h >> k;
   auto step = [](std::uint32_t n, std::uint32_t o) {
      return static_cast<std::uint32_t>((std::uint64_t{n} * (1u << 17) + o) / (2u * o));  // round to nearest
   };
   auto init = [](std::uint32_t s) { return (s - 65536u + 1u) >> 1; };  // ALG-RSZ-03: half up
   p.step_x = step(p.dec_w, mw);
   p.step_y = step(p.dec_h, mh);
   p.init_x = init(p.step_x);
   p.init_y = init(p.step_y);
   return p;
}

frame_geometry output_geometry(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height) {
   const geometry a = control_unit::derive_active(in_width, in_height, cfg.bayer_pattern);
   const resizer_plan p = plan_resizer(cfg.resizer_en, cfg.resizer_scale, a.width, a.height);
   return {p.out_w, p.out_h};
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
      axis(w, c.lsc_nx, cx_, wx_);
      axis(h, c.lsc_ny, cy_, wy_);
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
   // Eq 2: n_k = floor(k*S/C); cell(X) = max{k in [0, C-1] : n_k <= X}; Eq 9 weight.
   static void axis(std::uint32_t d, std::uint32_t n, std::vector<std::uint32_t> &cell, std::vector<std::int64_t> &w) {
      const std::uint64_t s = d - 1u, c = n - 1u;
      cell.assign(d, 0);
      w.assign(d, 0);
      std::uint32_t k = 0;
      for (std::uint32_t x = 0; x < d; ++x) {
         while (k + 1 < c && (std::uint64_t{k + 1} * s) / c <= x) {
            ++k;
         }
         const std::int64_t n0 = static_cast<std::int64_t>((std::uint64_t{k} * s) / c);
         const std::int64_t n1 = static_cast<std::int64_t>((std::uint64_t{k + 1} * s) / c);
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
            const std::int64_t t = std::max<std::int64_t>(c_->bpc_floor, (std::int64_t{c_->bpc_k} * (hi - lo) + 128) >> 8);
            const std::int64_t cval = centre[x];
            const std::int64_t dev = cval > avg8 ? cval - avg8 : avg8 - cval;
            cand_ += dev > std::int64_t{c_->bpc_floor};
            const bool defect = dev > t;
            defect_ += defect;
            o[x] = static_cast<std::uint16_t>(defect ? avg8 : cval);
         }
         win_.advance();
         out(std::move(o));
      }
   }
   const pipeline_config *c_ = nullptr;
   std::uint32_t w_ = 0;
   std::uint32_t cand_ = 0, defect_ = 0;
   row_window<row<std::uint16_t>> win_{2, border::mirror};
};
using yuv_sink = std::function<void(yuv_row &&)>;

// Demosaic (HAS §6.12.6-6.12.7): 5x5 green reconstruction at scale 64, then
// 3x3 R/B reconstruction on the unrounded green plane, reflect-101 borders.
class demosaic_stage {
public:
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
         if (dh > dv) {
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
      v_ = c.nr_var;
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
      med_.add(r);
      medians(false, out);
   }
   void finish(const sink &out) {
      if (!en_) {
         return;
      }
      medians(true, out);
      blocks(true, out);
      if (!odd_row_.empty()) {
         out(std::move(odd_row_));  // odd last row: median only (Table 6-56)
      }
   }
   std::uint32_t variance() const { return nr_variance(hist_, std::uint64_t{wb_} * hb_); }
   bool enabled() const { return en_; }

private:
   struct band_row {
      std::vector<std::int32_t> ll, lh, hl, hh;
      std::uint16_t odd_top = 0, odd_bot = 0;  // median of column W-1 when W is odd
   };

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
            m[x] = t[4];
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
      for (std::uint32_t i = 0; i < wb_; ++i) {
         const std::int32_t A = top[2 * i], B = top[2 * i + 1], C = bot[2 * i], D = bot[2 * i + 1];
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
   std::int64_t shrink(const std::vector<std::int32_t> band_row::*band, std::uint32_t i) const {
      const std::int64_t q = sum3([&](const band_row &r, std::size_t xi) {
         const std::int64_t p = (r.*band)[xi];
         return p * p;
      }, i);
      const std::int64_t d9 = std::max<std::int64_t>(q - 9 * std::int64_t{v_}, 0);
      const std::int64_t t = d9 > 0 ? std::min<std::int64_t>(65535, (768 * std::int64_t{v_}) / isqrt(d9)) : 65535;
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
            const std::int64_t rr = std::min<std::int64_t>(81 * std::int64_t{v_}, V);
            const std::int64_t g = V > 0 ? (256 * (V - rr)) / V : 0;
            const std::int64_t x = c.ll[i];
            const std::int64_t ll = std::clamp<std::int64_t>(fdiv(256 * s + g * (9 * x - s) + 1152, 2304), 0, 510);
            const std::int64_t lh = shrink(&band_row::lh, i), hl = shrink(&band_row::hl, i), hh = shrink(&band_row::hh, i);
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
   std::uint32_t v_ = 0, w_ = 0, h_ = 0, wb_ = 0, hb_ = 0, out_rows_ = 0;
   row_window<y_row> med_{1, border::replicate};
   row_window<band_row> sub_{1, border::replicate};
   std::array<std::uint32_t, 256> hist_{};
   y_row even_, odd_row_;
   bool have_even_ = false;
};

// EE (HAS §6.18.6): dual-scale high-pass, gain chain, clamp; live parameters.
class ee_stage {
public:
   using sink = std::function<void(y_row &&)>;
   void begin(const pipeline_config &c, std::uint32_t w, std::uint32_t h) {
      live_ = c.live;
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
      const register_file &r = live_->regs();
      const std::uint32_t y = win_.centre();
      y_row o = win_.at(0);
      if (!(r.peek(FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_EN_MASK) || y < 2 || y + 2 >= h_) {
         return o;  // bypass (live, ALG-EE-02) or border row
      }
      const std::int64_t alpha = std::min<std::uint32_t>(r.peek(FX1_ISP_EE_ALPHA_OFFSET) & 0xFFFFu, 0x8000u);
      const std::int64_t beta = std::min<std::uint32_t>(r.peek(FX1_ISP_EE_BETA_OFFSET) & 0xFFFFu, 0x8000u);
      const std::uint32_t clamp = r.peek(FX1_ISP_EE_CLAMP_OFFSET);
      const std::int64_t cpos = clamp & 0xFFu, cneg = (clamp >> 8) & 0xFFu;
      const std::uint32_t feat = r.peek(FX1_ISP_EE_FEATURE_EN_OFFSET);
      const std::uint32_t centre = r.peek(FX1_ISP_EE_RADIAL_CENTER_OFFSET);
      const std::int64_t cx2 = centre & 0x1FFFu, cy2 = (centre >> 16) & 0x1FFFu;
      const std::int64_t t0 = r.peek(FX1_ISP_EE_RADIAL_R2_TH0_OFFSET) & 0x7FFFFFFu;
      const std::int64_t t1 = r.peek(FX1_ISP_EE_RADIAL_R2_TH1_OFFSET) & 0x7FFFFFFu;
      const std::int64_t t2 = r.peek(FX1_ISP_EE_RADIAL_R2_TH2_OFFSET) & 0x7FFFFFFu;
      const std::uint32_t g01 = r.peek(FX1_ISP_EE_RADIAL_GAIN_01_OFFSET), g23 = r.peek(FX1_ISP_EE_RADIAL_GAIN_23_OFFSET);
      const std::int64_t gr[4] = {std::min<std::uint32_t>(g01 & 0xFFFFu, 0xC000u), std::min<std::uint32_t>(g01 >> 16, 0xC000u),
                                  std::min<std::uint32_t>(g23 & 0xFFFFu, 0xC000u), std::min<std::uint32_t>(g23 >> 16, 0xC000u)};
      const auto &luma = live_->ee_table(0), &act = live_->ee_table(1), &cp = live_->ee_table(2), &cn = live_->ee_table(3);
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
            o[x] = {static_cast<std::uint16_t>((su + n / 2) / n), static_cast<std::uint16_t>((sv + n / 2) / n)};
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

// One 2:1 decimation stage (HAS p157-158, ALG-RSZ-02: horizontal pass then
// vertical pass, each (sum + 4) >> 3, border taps clamped).
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
      rows_.push_back(horizontal(r));
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
         yuv_row o(a.size());
         for (std::size_t x = 0; x < o.size(); ++x) {
            o[x] = {fir(a[x].y, b[x].y, c[x].y, d[x].y), fir(a[x].u, b[x].u, c[x].u, d[x].u),
                    fir(a[x].v, b[x].v, c[x].v, d[x].v)};
         }
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

}  // namespace

// AEC (HAS §6.21.6, ALG-F §1.3): post-BPC Bayer, channel by position parity,
// clip-qualified sums/counts per channel globally and per zone; green OE/UE,
// min/max and the 64-bin histogram count every green sample. Global sums go
// to the frame result (published at EOF); zone and histogram words are
// updated in the single memory row by row (DEC-30). Accumulators wrap; the
// CSR field widths are applied at readout.
class aec_stage {
public:
   // `mem` is the control unit's single zone/histogram memory, restarted at
   // the SOF (DEC-30); without one (block tests) the stage uses its own.
   void begin(const aec_config &c, aec_result &g, aec_memory *mem) {
      c_ = c;
      g_ = &g;
      y_ = 0;
      g = aec_result{};
      m_ = mem ? mem : &own_;
      if (!mem && c.en) {
         own_.restart(c);
      }
   }
   void push(const row<std::uint16_t> &in) {
      if (!c_.en) {
         return;
      }
      aec_result &g = *g_;
      aec_memory &m = *m_;
      const std::uint32_t y = y_++;
      const std::size_t zrow = m.zones ? std::size_t{std::min(y / c_.zh, c_.ny - 1)} * c_.nx : 0;
      for (std::uint32_t x = 0; x < in.size(); ++x) {
         const std::uint32_t p = in[x] & 0xFFFu;
         const unsigned ch = ((y & 1u) << 1) | (x & 1u);
         const std::size_t z = m.zones ? zrow + std::min(x / c_.zw, c_.nx - 1) : 0;
         if (p >= c_.min_clip && p <= c_.max_clip) {  // inclusive both ends
            g.gsum[ch] += p;
            ++g.gcnt[ch];
            if (m.zones) {
               m.zsum[z][ch] += p;
               ++m.zcnt[z][ch];
            }
         }
         if (ch == 1 || ch == 2) {  // Gr and Gb combined, not clip-qualified
            if (m.zones) {
               m.oe[z] += p > c_.th_oe;
               m.ue[z] += p < c_.th_ue;
               m.gmin[z] = std::min(m.gmin[z], p);
               m.gmax[z] = std::max(m.gmax[z], p);
            }
            ++m.hist[p >> 6];
         }
      }
   }

private:
   aec_config c_{};
   aec_result *g_ = nullptr;
   aec_memory *m_ = nullptr;
   aec_memory own_;
   std::uint32_t y_ = 0;
};

// AWB (HAS §6.22.5, ALG-F §2.3): post-Demosaic RGB, a pixel qualifies when
// UL < R, G, B < SL; equal zones by the closed form of HAS p181. Zone sums go
// to the single memory row by row, like AEC (DEC-30).
class awb_stage {
public:
   void begin(const awb_config &c, awb_result &g, awb_memory *mem, std::uint32_t w, std::uint32_t h) {
      c_ = c;
      g_ = &g;
      w_ = w;
      h_ = h;
      y_ = 0;
      g = awb_result{};
      m_ = mem ? mem : &own_;
      if (!mem && c.en) {
         own_.restart(c);
      }
   }
   void push(const row<rgb> &in) {
      if (!c_.en) {
         return;
      }
      awb_result &g = *g_;
      awb_memory &m = *m_;
      const std::uint64_t y = y_++;
      const std::size_t zrow =
          m.zones ? std::size_t{std::min<std::uint64_t>(((y + 1) * c_.ny - 1) / h_, c_.ny - 1)} * c_.nx : 0;
      const std::uint32_t ul = c_.under, sl = c_.sat;
      for (std::uint64_t x = 0; x < in.size(); ++x) {
         const rgb &p = in[x];
         if (p.r <= ul || p.g <= ul || p.b <= ul || p.r >= sl || p.g >= sl || p.b >= sl) {
            continue;
         }
         g.r += p.r;
         g.g += p.g;
         g.b += p.b;
         ++g.n;
         if (m.zones) {
            const std::size_t z = zrow + std::min<std::uint64_t>(((x + 1) * c_.nx - 1) / w_, c_.nx - 1);
            m.zsum[z][0] += p.r;
            m.zsum[z][1] += p.g;
            m.zsum[z][2] += p.b;
            ++m.zcnt[z];
         }
      }
   }

private:
   awb_config c_{};
   awb_result *g_ = nullptr;
   awb_memory *m_ = nullptr;
   awb_memory own_;
   std::uint32_t w_ = 0, h_ = 0, y_ = 0;
};

// AF (HAS §6.23.5, ALG-F §3.3): Sobel |gx| + |gy| on the CSC luma with
// replicate borders, summed over a fixed 4x4 grid. Observer only: the stream
// is not modified. EN is read live per evaluated row (ALG-AF-01).
class af_stage {
public:
   void begin(const pipeline_config &c, af_result &r, std::uint32_t w, std::uint32_t h) {
      c_ = &c;
      r_ = &r;
      w_ = w;
      h_ = h;
      full_ = c.af_en;
      r = af_result{};
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
   bool full() const { return full_; }

private:
   bool enabled() const {
      return c_->live ? (c_->live->regs().peek(FX1_ISP_AF_CTRL_OFFSET) & FX1_ISP_AF_CTRL_EN_MASK) != 0 : c_->af_en;
   }
   void drain(bool ended) {
      while (win_.ready(ended)) {
         const std::uint32_t yc = win_.centre();
         if (!enabled()) {
            full_ = false;
         } else {
            const y_row &a = win_.at(-1), &b = win_.at(0), &d = win_.at(1);
            const std::size_t zrow = std::size_t{yc} * 4u / h_ * 4u;
            for (std::uint32_t x = 0; x < w_; ++x) {
               const std::uint32_t xl = x ? x - 1 : 0, xr = std::min(x + 1, w_ - 1);
               const std::int32_t gx = (a[xr] + 2 * b[xr] + d[xr]) - (a[xl] + 2 * b[xl] + d[xl]);
               const std::int32_t gy = (d[xl] + 2 * d[x] + d[xr]) - (a[xl] + 2 * a[x] + a[xr]);
               r_->fv[zrow + std::size_t{x} * 4u / w_] += static_cast<std::uint32_t>(std::abs(gx) + std::abs(gy));
            }
         }
         win_.advance();
      }
   }
   const pipeline_config *c_ = nullptr;
   af_result *r_ = nullptr;
   std::uint32_t w_ = 0, h_ = 0;
   bool full_ = false;
   row_window<y_row> win_{1, border::replicate};
};

struct isp_pipeline::impl {
   pipeline_config cfg;
   std::uint32_t in_w = 0, in_h = 0, sh = 0, sv = 0;
   geometry active{};
   resizer_plan plan{};
   std::uint32_t y_in = 0;
   std::uint32_t y_bayer = 0;  // row index after the Input Formatter crop
   std::uint32_t y_out = 0;
   y_sink y_out_sink;
   uv_sink uv_out_sink;
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
   resizer_stage rsz;
   aec_stage aec;
   awb_stage awb;
   af_stage af;

   // WB and DG after BPC, then Demosaic.
   void bayer_row(row<std::uint16_t> &&r) {
      const std::uint32_t y = y_post_bpc++;
      aec.push(r);  // tap: post-BPC, pre-WB (HAS Table 9-6)
      for (std::uint32_t x = 0; x < r.size(); ++x) {
         const unsigned ch = 2u * (y & 1u) + (x & 1u);
         r[x] = dg_pixel(cfg, wb_pixel(cfg, r[x], ch));
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
         rsz.push(std::move(o), [this](yuv_row &&z) { ofmt(z); });
      }
   }
   void rgb_row(row<rgb> &&r) {
      awb.push(r);  // tap: post-Demosaic, pre-CCM
      yuv_row o(r.size());
      for (std::size_t x = 0; x < r.size(); ++x) {
         rgb p = ccm_pixel(cfg, r[x]);
         if (cfg.gamma_en) {  // HAS p95: index = top 8 bits, live table
            const auto &lut = *cfg.gamma_lut;
            p = {lut[p.r >> 4], lut[p.g >> 4], lut[p.b >> 4]};
         }
         o[x] = csc_pixel(cfg.csc_std, p);
      }
      af.push(o);  // inline after CSC, before GTM
      yuv_rows(std::move(o));
   }
   void ofmt(const yuv_row &r) {  // HAS §6.24: Y as is, UV = top-left cosited
      std::vector<std::uint8_t> yb(r.size());
      for (std::size_t x = 0; x < r.size(); ++x) {
         yb[x] = static_cast<std::uint8_t>(r[x].y);
      }
      y_out_sink(std::move(yb));
      if (y_out % 2u == 0) {
         std::vector<std::uint8_t> uv(r.size());
         for (std::size_t i = 0; i + 1 < r.size(); i += 2) {
            uv[i] = static_cast<std::uint8_t>(r[i].u);
            uv[i + 1] = static_cast<std::uint8_t>(r[i].v);
         }
         uv_out_sink(std::move(uv));
      }
      ++y_out;
   }
};

isp_pipeline::isp_pipeline() : impl_(std::make_unique<impl>()) {}
isp_pipeline::~isp_pipeline() = default;

frame_geometry isp_pipeline::begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                                   y_sink y, uv_sink uv, warning_sink warn) {
   impl &m = *impl_;
   m.cfg = cfg;
   m.in_w = in_width;
   m.in_h = in_height;
   m.sh = cfg.bayer_pattern & 1u;
   m.sv = (cfg.bayer_pattern >> 1) & 1u;
   m.active = control_unit::derive_active(in_width, in_height, cfg.bayer_pattern);
   m.plan = plan_resizer(cfg.resizer_en, cfg.resizer_scale, m.active.width, m.active.height);
   m.y_in = m.y_bayer = m.y_out = m.y_post_bpc = 0;
   m.stats = frame_stats{};
   m.y_out_sink = std::move(y);
   m.uv_out_sink = std::move(uv);
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
      if (cfg.bpc_en && cfg.bpc_dynamic_requested) {
         warn("BPC dynamic detection requested: not modelled, static detection used (DEC-28)");
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
   m.rsz.begin(m.plan, m.active.width, m.active.height);
   m.aec.begin(m.cfg.aec, m.stats.aec, m.cfg.aec_mem);
   m.awb.begin(m.cfg.awb, m.stats.awb, m.cfg.awb_mem, m.active.width, m.active.height);
   m.af.begin(m.cfg, m.stats.af, m.active.width, m.active.height);
   return {m.plan.out_w, m.plan.out_h};
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
      std::uint16_t p = samples[x + m.sh] & 0x0FFFu;  // ALG-IFMT-03
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
   m.rsz.finish([&](yuv_row &&o) { m.ofmt(o); });
   m.stats.gtm_yavg = m.gtm.yavg();
   m.stats.nr_measured = m.nr.enabled();
   m.stats.nr_var = m.nr.variance();
   m.stats.lsc_overflow = m.lsc.overflow();
   m.stats.bpc_candidates = m.bpc.candidates();
   m.stats.bpc_defective = m.bpc.defective();
   m.stats.af_full = m.af.full();
}

const frame_stats &isp_pipeline::stats() const { return impl_->stats; }

std::vector<std::uint16_t> run_nr2d(const std::vector<std::uint16_t> &y, std::uint32_t w, std::uint32_t h,
                                    std::uint32_t var_in, std::uint32_t &var_out) {
   pipeline_config c;
   c.nr_en = true;
   c.nr_var = var_in;
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
   if (cfg.bpc_en) {  // ALG-BPC-03 static counts, saturating; DEC-28: 0 when dynamic is requested
      const bool dyn = cfg.bpc_dynamic_requested;
      ctl.hw_write(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET, 0x3FFFu, dyn ? 0u : std::min<std::uint32_t>(st.bpc_candidates, 0x3FFFu));
      ctl.hw_write(FX1_ISP_BPC_NUM_DEFECTIVE_OFFSET, 0x3FFFu, dyn ? 0u : std::min<std::uint32_t>(st.bpc_defective, 0x3FFFu));
   }
   ctl.gtm_end_of_frame(st.gtm_yavg);  // stats and rebuild run regardless of EN (HAS p105)
   if (st.nr_measured) {
      ctl.nr_publish(st.nr_var);         // held while 2DNR is disabled (ALG-2DNR-09)
   }
   // Tap order AEC -> AWB -> AF; a block disabled for the frame keeps its
   // results (ALG-STAT-06); AF decides on its EN at the end of frame.
   ctl.stats_publish(cfg.aec.en ? &st.aec : nullptr, cfg.awb.en ? &st.awb : nullptr, &st.af, st.af_full);
}

}  // namespace cdc::components::fx1_isp::pipe
