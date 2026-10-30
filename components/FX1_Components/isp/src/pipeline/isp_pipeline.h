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

#include "control/channel_geometry.h"
#include "control/ee_types.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "control/stats_types.h"
#include "pipeline/gtm_curve.h"
#include "pipeline/row_stage.h"

namespace cdc::components::fx1_isp {

class control_unit;

namespace pipe {

struct pipeline_config {
   std::uint32_t bayer_pattern = 0;
   // COMMON_INPUT_FORMAT.DEPTH captured with the ingress frame: 0 RAW12, 1 RAW10
   // (MAS v1.07 §6.6.3.2). 2/3 never reach the pipeline (IDMA refuses, V107-A10).
   std::uint32_t input_depth = 0;
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
   // Nonuniform sectors (MAS v1.07 §6.8.3.9, profile mode): nodes - 1 intervals per axis.
   bool lsc_sectors = false;
   std::array<std::vector<std::uint32_t>, 2> lsc_sector{};
   std::vector<std::uint32_t> lsc_mesh;
   // BPC (MAS §6.9): spatial detection; DYNAMIC_DET_EN has no effect.
   bool bpc_en = false;
   // Colour thresholds and factory map (MAS v1.07 §6.9.3.6, profile mode).
   bool bpc_color = false;
   std::uint32_t bpc_k_g = 0x100, bpc_floor_g = 0, bpc_k_rb = 0x100, bpc_floor_rb = 0;  // K UQ4.8
   std::vector<std::uint64_t> bpc_factory;  // raster positions y * W + x, strictly increasing
   // Demosaic decision margins (MAS v1.07 §6.12.3.7): 0 in legacy mode.
   std::uint32_t dms_margin_green = 0, dms_margin_rb = 0;
   // CNF output blend weight 0..256 (MAS v1.07 §6.19.3.6): 256 in legacy mode.
   std::uint32_t cnf_blend = 256;
   std::uint32_t bpc_floor = 0;
   std::uint32_t bpc_k = 0;  // UQ8.8
   // WB / DG (HAS §6.10-6.11), UQ4.8.
   bool wb_en = false;
   std::array<std::uint32_t, 3> wb_gain{0x100, 0x100, 0x100};  // R, G (Gr), B
   std::uint32_t wb_gain_gb = 0x100;  // Gb: WB_GAIN_G in legacy mode, TUNE_WB_GB in profile mode (§6.10.3.6)
   bool dg_en = false;
   std::uint32_t dg_gain = 0x100;
   // CCM (HAS §6.13), committed set (DEC-24), sign-extended Q3.9.
   bool ccm_en = false;
   std::array<std::array<std::int32_t, 3>, 3> ccm{};
   std::array<std::int32_t, 3> ccm_offset{};
   // Gamma (HAS §6.14): live 256-entry table.
   bool gamma_en = false;
   const std::array<std::uint16_t, 256> *gamma_lut = nullptr;
   // CSC (MAS v1.07 §6.15.3.3): standard, range (TUNE_CSC_CTRL.FULL_RANGE,
   // reset 1 and applied in legacy mode too) and the profile-mode look.
   std::uint32_t csc_std = 0;
   bool csc_full_range = true;
   bool csc_look = false;
   std::uint32_t csc_contrast = 0x100;  // UQ4.8
   std::int32_t csc_brightness = 0;     // signed 10-bit
   std::array<std::int32_t, 4> csc_m{0x4000, 0, 0, 0x4000};  // UU, UV, VU, VV: signed Q4.14
   // GTM (HAS §6.16): read bank (live table), valid flag, ROI size.
   bool gtm_en = false;
   bool gtm_curve_valid = false;
   const gtm_curve *gtm_bank = nullptr;
   std::uint32_t gtm_roi_log2 = 11;
   // 2DNR (HAS §6.17): enable and the previous frame's noise variance.
   bool nr_en = false;
   std::uint32_t nr_var = 0;
   // Noise model, band strength and blend (MAS v1.07 §6.17.3.10); the reset
   // values (mode 0, seed 0, unity scales, full limits, median on, blend 256)
   // are the legacy behaviour.
   bool nr_estimate_valid = false;  // an automatic estimate of this epoch exists
   std::uint32_t nr_mode = 0, nr_seed = 0, nr_manual = 0, nr_scale = 0x100, nr_min = 0, nr_max = 0xFFFF;
   std::array<std::uint32_t, 4> nr_band_scale{0x100, 0x100, 0x100, 0x100};  // LL, LH, HL, HH
   bool nr_median = true;
   std::uint32_t nr_blend = 256;
   std::array<std::uint16_t, 17> nr_curve{};
   // EE (MAS §6.18.4.1): enable, profile and scalars sampled at SOF; the
   // profile's tables are read live through the control unit (DEC-25
   // superseded for the scalars).
   ee_frame_set ee;
   const control_unit *live = nullptr;
   // CNF (HAS §6.19): committed thresholds (DEC-27).
   bool cnf_en = false;
   std::uint32_t cnf_chroma_th = 0;
   std::uint32_t cnf_luma_th = 0;
   // Output channels (MAS v1.07 §6.20.1.1, §8.3): the geometry sets published
   // before the frame's accepted SOF, with the active size they were derived
   // for, and the OUTPUT_ENABLE mask of that SOF. A channel whose set is not
   // valid for the frame's raster (only reachable without admission control:
   // block tests, a debug SOF) passes the full active image unscaled (U4-A1).
   std::array<channel_set, num_channels> channels{};
   std::uint32_t outputs = 1;
   // Statistics (MAS v1.07 §6.21-6.23): the sets of the frame; `en` is set
   // only when the engine admitted the measurement into a FREE bank.
   aec_config aec;
   awb_config awb;
   af_config af;
   // Requested but not modelled: reported once per frame (DEC-39, M5-A1).
   bool ofmt_stride_en = false;   // OFMT_CTRL.stride_en: the ODMA applies its own stride
   bool wdr_en = false;           // D_WDR_CTRL.en: WDR unsupported (HAS §3.1)
   bool tnr_en = false;           // TNR_3D_CTRL.en: 3DNR unsupported (HAS §3.1)
};

// Builds the SOF snapshot from the register state.
pipeline_config snapshot_config(control_unit &ctl);

// Input-boundary adapter (MAS v1.07 Table 6-18): a right-aligned container to
// the internal RAW12 domain, RAW10 shifted left two exactly once. Container
// bits above the native depth are not part of the sample (V107-A8).
constexpr std::uint16_t normalize_input(std::uint16_t container, std::uint32_t depth) {
   return depth == FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_RAW10
              ? static_cast<std::uint16_t>((container & 0x03FFu) << 2)
              : static_cast<std::uint16_t>(container & 0x0FFFu);
}

struct frame_geometry {
   std::uint32_t width;
   std::uint32_t height;
};

// Output geometry channel `ch` produces for an input frame (Input Formatter
// crop, channel crop, Resizer), the same derivation as RESIZER_OUT_W/H.
frame_geometry output_geometry(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                               unsigned ch = 0);
// The crop and plan channel `ch` applies to a frame of the given active size.
channel_set channel_for_frame(const pipeline_config &cfg, const geometry &active, unsigned ch);

// CH2 YCbCr -> RGB888 (MAS v1.07 §6.15.3, Table 6-89, §6.24.7): the frame's
// standard (0 BT.601, 1 BT.709) and range; one rounding R(n, 65536) per output.
struct rgb8 {
   std::uint8_t r, g, b;
};
rgb8 rgb888_pixel(std::uint32_t std, bool full_range, std::uint16_t y, std::uint16_t u, std::uint16_t v);

// One channel of a whole post-IQ YUV444 frame (w x h, row-major) through its
// streaming crop, Resizer and formatter: NV12 planes for CH0/CH1, packed
// R, G, B bytes for CH2 (tests; the engine streams CH0).
struct channel_output {
   std::uint32_t width = 0, height = 0;
   std::vector<std::uint8_t> y, uv, rgb;
};
channel_output run_channel(const pipeline_config &cfg, unsigned ch, const std::vector<yuv> &frame,
                           std::uint32_t w, std::uint32_t h);

// Per-frame results of the blocks with status outputs.
struct frame_stats {
   std::array<std::uint32_t, 4> lsc_overflow{};  // R, Gr, Gb, B: samples with pr > 4095 (ALG-LSC-08)
   std::uint32_t bpc_candidates = 0;             // dev > FLOOR (ALG-BPC-03)
   std::uint32_t bpc_defective = 0;              // dev > T
   std::uint32_t gtm_yavg = 1;                   // ROI mean of the GTM input luma (HAS §6.16.4.2)
   bool nr_measured = false;                     // 2DNR enabled for the frame
   std::uint32_t nr_var = 0;                     // variance measured on the frame (HAS §6.17.7.2)
   aec_payload aec;                              // valid when cfg.aec.en
   awb_payload awb;                              // valid when cfg.awb.en
   af_payload af;                                // valid when cfg.af.en
};

// End-of-frame publication shared by the engine and the tests: block
// counters, the GTM curve build and the 2DNR variance hand-over.
void end_of_frame(control_unit &ctl, const pipeline_config &cfg, const frame_stats &st);

class isp_pipeline {
public:
   using y_sink = std::function<void(std::vector<std::uint8_t> &&)>;
   using uv_sink = std::function<void(std::vector<std::uint8_t> &&)>;
   using warning_sink = std::function<void(const std::string &)>;
   // The post-IQ YUV444 row of the active image, before the channel crops
   // (tests: input of run_channel for CH1/CH2 until V107-8 streams them).
   using yuv_tap = std::function<void(const std::vector<yuv> &)>;
   // The sinks of one output channel: NV12 luma / chroma rows (CH0, CH1) or
   // packed R, G, B rows in `y` (CH2). A channel without a `y` sink is off.
   struct channel_sinks {
      y_sink y;
      uv_sink uv;
   };

   isp_pipeline();
   ~isp_pipeline();

   // Starts a frame of in_width x in_height input samples (the IDMA working
   // set). Returns the NV12 output geometry.
   frame_geometry begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                        y_sink y, uv_sink uv, warning_sink warn = nullptr, yuv_tap tap = nullptr);
   // The same with a sink set per output channel (MAS v1.07 §9.5.1: the
   // distributor presents every pixel to each enabled channel).
   frame_geometry begin(const pipeline_config &cfg, std::uint32_t in_width, std::uint32_t in_height,
                        const std::array<channel_sinks, num_channels> &sinks, warning_sink warn = nullptr,
                        yuv_tap tap = nullptr);
   // Output geometry of channel `ch` for the frame begun.
   frame_geometry output(unsigned ch) const;
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
yuv csc_pixel(const pipeline_config &cfg, const rgb &p);

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
// The same stages with a whole configuration (V107-5 NR and CNF parameters).
std::vector<std::uint16_t> run_nr2d(const pipeline_config &c, const std::vector<std::uint16_t> &y, std::uint32_t w,
                                    std::uint32_t h, std::uint32_t &var_out);
std::vector<uv_pair> run_cnf(const pipeline_config &c, const std::vector<yuv> &img, std::uint32_t w,
                             std::uint32_t h);


}  // namespace pipe
}  // namespace cdc::components::fx1_isp
