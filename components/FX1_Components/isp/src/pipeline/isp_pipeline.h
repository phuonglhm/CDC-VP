// SPDX-License-Identifier: Apache-2.0
// FX1 ISP pixel pipeline (M3): a chain of row-streaming blocks in the order of
// HAS Table 6-2. Pure C++, no SystemC. The configuration is a snapshot taken
// at the accepted SOF (HAS §9.4.3), with the exceptions the decisions define:
// gated sets use their committed values (DEC-24..27) and the Gamma/EE tables
// are live (HAS Table 9-4, DEC-25).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "control/stats_types.h"
#include "pipeline/gtm_curve.h"
#include "pipeline/row_stage.h"

namespace cdc::components::fx1_isp {

class control_unit;

namespace pipe {

struct pipeline_config {
   std::uint32_t bayer_pattern = 0;
   // BLC (HAS §6.7) for the selected profile.
   bool blc_en = false;
   bool blc_range_scale = true;
   std::uint32_t blc_pedestal = 0;
   std::array<std::uint32_t, 4> blc_trim{};  // R, Gr, Gb, B
   std::uint32_t blc_scale = 0x10000;
   // LSC (HAS §6.8): effective enable = EN && active profile valid && geometry
   // valid (CSR #86, #109, ALG-LSC-09); mesh over the Input Formatter output
   // (ALG-LSC-01); a copy of the active profile for the frame.
   bool lsc_en = false;
   std::uint32_t lsc_nx = 0, lsc_ny = 0;
   std::uint32_t lsc_strength = 0;  // UQ1.16, raw 17-bit code (ALG-LSC-02)
   std::vector<std::uint32_t> lsc_mesh;
   // BPC (HAS §6.9), static detection only (DEC-28).
   bool bpc_en = false;
   bool bpc_dynamic_requested = false;
   std::uint32_t bpc_floor = 0;
   std::uint32_t bpc_k = 0;  // UQ8.8
   // WB / DG (HAS §6.10-6.11), UQ4.8.
   bool wb_en = false;
   std::array<std::uint32_t, 3> wb_gain{0x100, 0x100, 0x100};  // R, G, B
   bool dg_en = false;
   std::uint32_t dg_gain = 0x100;
   // CCM (HAS §6.13), committed set (DEC-24), sign-extended Q3.9.
   bool ccm_en = false;
   std::array<std::array<std::int32_t, 3>, 3> ccm{};
   std::array<std::int32_t, 3> ccm_offset{};
   // Gamma (HAS §6.14): live 256-entry table.
   bool gamma_en = false;
   const std::array<std::uint16_t, 256> *gamma_lut = nullptr;
   // CSC (HAS §6.15, SPEC-03).
   std::uint32_t csc_std = 0;
   // GTM (HAS §6.16): read bank (live table), valid flag, ROI size.
   bool gtm_en = false;
   bool gtm_curve_valid = false;
   const gtm_curve *gtm_bank = nullptr;
   std::uint32_t gtm_roi_log2 = 11;
   // 2DNR (HAS §6.17): enable and the previous frame's noise variance.
   bool nr_en = false;
   std::uint32_t nr_var = 0;
   // EE (HAS §6.18): parameters and tables are read live (DEC-25).
   const control_unit *live = nullptr;
   // CNF (HAS §6.19): committed thresholds (DEC-27).
   bool cnf_en = false;
   std::uint32_t cnf_chroma_th = 0;
   std::uint32_t cnf_luma_th = 0;
   // Resizer (HAS §6.20): enable sampled at SOF, scale committed (DEC-24).
   bool resizer_en = false;
   std::uint32_t resizer_scale = 0;
   // Statistics (HAS §6.21-6.23): AEC active set (after commit), AWB and AF
   // enable sampled at SOF; AF EN is also read live per row (ALG-AF-01).
   aec_config aec;
   awb_config awb;
   bool af_en = false;
   // The control unit's single zone/histogram memories (DEC-30), updated row
   // by row; null in block tests (the stages then use their own).
   aec_memory *aec_mem = nullptr;
   awb_memory *awb_mem = nullptr;
   // Requested but not modelled: reported once per frame (DEC-39, M5-A1).
   bool ofmt_stride_en = false;   // OFMT_CTRL.stride_en: the ODMA applies its own stride
   bool wdr_en = false;           // D_WDR_CTRL.en: WDR unsupported (HAS §3.1)
   bool tnr_en = false;           // TNR_3D_CTRL.en: 3DNR unsupported (HAS §3.1)
};

// Builds the SOF snapshot from the register state.
// Non-const: the configuration carries the control unit's statistics memories.
pipeline_config snapshot_config(control_unit &ctl);

struct frame_geometry {
   std::uint32_t width;
   std::uint32_t height;
};

// Output geometry the pipeline produces for an input frame (Input Formatter
// crop, then Resizer), the same derivation as RESIZER_OUT_W/H.
frame_geometry output_geometry(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height);

// Per-frame results of the blocks with status outputs.
struct frame_stats {
   std::array<std::uint32_t, 4> lsc_overflow{};  // R, Gr, Gb, B: samples with pr > 4095 (ALG-LSC-08)
   std::uint32_t bpc_candidates = 0;             // dev > FLOOR (ALG-BPC-03)
   std::uint32_t bpc_defective = 0;              // dev > T
   std::uint32_t gtm_yavg = 1;                   // ROI mean of the GTM input luma (HAS §6.16.4.2)
   bool nr_measured = false;                     // 2DNR enabled for the frame
   std::uint32_t nr_var = 0;                     // variance measured on the frame (HAS §6.17.7.2)
   aec_result aec;                               // global results, valid when cfg.aec.en
   awb_result awb;                               // global results, valid when cfg.awb.en
   af_result af;                                 // rows accumulated while AF EN was set
   bool af_full = false;                         // EN set from SOF through the last row (ALG-AF-02)
};

// End-of-frame publication shared by the engine and the tests: block
// counters, the GTM curve build and the 2DNR variance hand-over.
void end_of_frame(control_unit &ctl, const pipeline_config &cfg, const frame_stats &st);

class isp_pipeline {
public:
   using y_sink = std::function<void(std::vector<std::uint8_t> &&)>;
   using uv_sink = std::function<void(std::vector<std::uint8_t> &&)>;
   using warning_sink = std::function<void(const std::string &)>;

   isp_pipeline();
   ~isp_pipeline();

   // Starts a frame of in_width x in_height input samples (the IDMA working
   // set). Returns the NV12 output geometry.
   frame_geometry begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                        y_sink y, uv_sink uv, warning_sink warn = nullptr);
   // One input line of in_width 16-bit containers.
   void push(const std::uint16_t *samples);
   // After the last input line: flushes every block (bottom borders).
   void finish();
   // Valid after finish().
   const frame_stats &stats() const;

private:
   struct impl;
   std::unique_ptr<impl> impl_;
};

// Block kernels exposed for unit tests (frame-at-a-time wrappers are in the
// tests; the pipeline uses the streaming forms).
std::uint16_t blc_pixel(const pipeline_config &c, std::uint16_t p, unsigned ch);
std::uint16_t wb_pixel(const pipeline_config &c, std::uint16_t p, unsigned ch);
std::uint16_t dg_pixel(const pipeline_config &c, std::uint16_t p);
rgb ccm_pixel(const pipeline_config &c, const rgb &p);
yuv csc_pixel(std::uint32_t std_sel, const rgb &p);

// EE gain composition in the binding order L, A, C, R (HAS p137, ALG-EE-04).
std::uint32_t ee_gain_chain(std::uint32_t gl, std::uint32_t ga, std::uint32_t gc, std::uint32_t gr);
// 2DNR noise variance from the |HH| histogram of n blocks (HAS §6.17.7.2).
std::uint32_t nr_variance(const std::array<std::uint32_t, 256> &hist, std::uint64_t n);

// Single-block runners for block-level vectors (tests only): run one stage on
// a whole frame through its streaming implementation.
struct uv_pair {
   std::uint16_t u, v;
};
std::vector<std::uint16_t> run_nr2d(const std::vector<std::uint16_t> &y, std::uint32_t w, std::uint32_t h,
                                    std::uint32_t var_in, std::uint32_t &var_out);
std::vector<std::uint16_t> run_ee(const std::vector<std::uint16_t> &y, std::uint32_t w, std::uint32_t h,
                                  const control_unit &live);
std::vector<uv_pair> run_cnf(const std::vector<yuv> &img, std::uint32_t w, std::uint32_t h, std::uint32_t tc,
                             std::uint32_t ty);

struct resizer_plan {
   bool pass;             // FullRes / disabled / mode not applicable
   unsigned stages;       // k decimation stages
   std::uint32_t out_w, out_h;
   std::uint32_t dec_w, dec_h;  // bilinear input size
   std::uint32_t step_x, step_y, init_x, init_y;  // UQ16.16
};
resizer_plan plan_resizer(bool en, std::uint32_t scale, std::uint32_t in_w, std::uint32_t in_h);

}  // namespace pipe
}  // namespace cdc::components::fx1_isp
