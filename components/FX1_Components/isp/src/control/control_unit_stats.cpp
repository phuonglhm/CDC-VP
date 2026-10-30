// SPDX-License-Identifier: Apache-2.0
// Statistics ownership and readout of the control unit, CSR/MAS v1.07:
// AEC (MAS §6.21), AWB (§6.22), AF (§6.23), result banks and indexed readout
// (§9.8), leased geometry (§9.9.9), simultaneous events (Table 9-198).
//
// - AEC and AF: two result banks each, ACQUIRE/RELEASE through TUNE_*_CMD
//   (combined: release first), 64-bit results through INDEX/DATA_LO/DATA_HI
//   (DATA_LO snapshots the high word), narrow compatibility windows bound to
//   the same lease. Reads without a lease return zero and set NO_LEASE.
// - AWB: its own bank protocol (U2): RELEASE wins over ACQUIRE, no errors.
// - A skipped measurement (illegal configuration or no FREE bank) is counted
//   (TUNE_*_DROPS, AWB_DROP_COUNT) and never stalls video.

#include "control/control_unit.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

std::uint32_t fld(std::uint32_t reg, std::uint32_t mask, std::uint32_t shift) { return (reg & mask) >> shift; }

constexpr std::uint32_t err_invalid = FX1_ISP_TUNE_ERROR_INVALID_MASK;
constexpr std::uint32_t err_no_lease = FX1_ISP_TUNE_ERROR_NO_LEASE_MASK;
constexpr std::uint32_t err_overflow = FX1_ISP_TUNE_ERROR_OVERFLOW_MASK;
constexpr std::uint32_t cmd_acquire = FX1_ISP_TUNE_AEC_CMD_ACQUIRE_MASK, cmd_release = FX1_ISP_TUNE_AEC_CMD_RELEASE_MASK;
static_assert(FX1_ISP_TUNE_AF_CMD_ACQUIRE_MASK == cmd_acquire && FX1_ISP_TUNE_AF_CMD_RELEASE_MASK == cmd_release,
              "AEC and AF share the command layout");

constexpr std::uint32_t xy(std::uint32_t x, std::uint32_t y) { return (x & 0xFFFFu) | (y << 16); }

// Logical 64-bit AEC results (MAS Table 9-193).
std::uint64_t aec_index(const result_banks<aec_payload>::bank &b, std::uint32_t i, bool &invalid) {
   const aec_payload &p = b.payload;
   if (i <= 8) {
      const std::uint64_t v[] = {p.sum, p.count, p.oe, p.ue, p.min, p.max, p.wtotal, p.wsum, p.overflow ? 1u : 0u};
      return v[i];
   }
   switch (i) {
   case 0x10: return p.roi_start;
   case 0x11: return p.roi_end;
   case 0x12: return p.steps;
   default: break;
   }
   if (i >= 0x100 && i < 0x140) {
      return p.hist[i - 0x100];
   }
   if (i >= 0x140 && i < 0x180) {
      return p.whist[i - 0x140];
   }
   if (i >= 0x200 && i < 0x200 + 8 * 768) {
      const std::size_t z = (i - 0x200) / 8;
      const unsigned k = (i - 0x200) % 8;
      if (z >= p.zones.size() || k >= 6) {
         return 0;  // beyond the captured grid, padding (V107-A23)
      }
      const aec_zone &e = p.zones[z];
      const std::uint64_t v[] = {e.sum, e.count, e.oe, e.ue, e.min, e.max};
      return v[k];
   }
   invalid = true;
   return 0;
}

// Logical 64-bit AF results (MAS Table 9-193, §9.9.9).
std::uint64_t af_index(const result_banks<af_payload>::bank &b, std::uint32_t i, bool &invalid) {
   const af_payload &p = b.payload;
   if (i == 0) {
      return p.overflow ? 1u : 0u;
   }
   if (i >= 0x10 && i <= 0x13) {
      const std::uint32_t v[] = {p.w, p.h, p.mask, p.mode};
      return v[i - 0x10];
   }
   if (i >= 0x100 && i < 0x140) {
      const af_window &w = p.win[(i - 0x100) / 4];
      const std::uint64_t v[] = {w.focus, w.luma, w.count, 0};
      return v[(i - 0x100) % 4];
   }
   if (i >= 0x200 && i < 0x220) {
      const auto &r = p.rects[(i - 0x200) / 2];
      return (i & 1u) ? xy(r[2], r[3]) : xy(r[0], r[1]);
   }
   invalid = true;
   return 0;
}

}  // namespace

bool control_unit::awb_grid_legal(std::uint32_t awb_ctrl, const geometry &g) {
   const std::uint32_t nx = fld(awb_ctrl, FX1_ISP_AWB_CTRL_NUM_ZONE_X_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_X_SHIFT);
   const std::uint32_t ny = fld(awb_ctrl, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_SHIFT);
   return g.width >= 32 && g.width <= 3840 && g.height >= 32 && g.height <= 2160 && g.width % 2 == 0 &&
          g.height % 2 == 0 && nx >= 1 && nx <= std::min<std::uint32_t>(64, g.width / 2) && ny >= 1 &&
          ny <= std::min<std::uint32_t>(32, g.height / 2);
}

void control_unit::stats_reset(bool external) {
   // Hard and COMMON soft reset clear the results, ownership, leases and loss
   // reports of the three engines and cancel the measurements in flight; the
   // configuration, the frame counter and a pending AEC commit stay (MAS
   // Table 9-187); i_rst_n clears everything.
   if (external) {
      aec_active_ = aec_sof_ = aec_config{};
      awb_sof_ = awb_config{};
      af_sof_ = af_config{};
      frame_counter_ = 0;
      frame_id_ = 0;
   }
   aec_banks_.clear();
   af_banks_.clear();
   awb_banks_.clear();
   awb_overrun_ = false;
   awb_drops_ = 0;
   awb_loss_cycle_ = settled;
   awb_loss_count_ = 0;
   awb_zone_cycle_ = settled;
}

aec_config control_unit::aec_set(const std::function<std::uint32_t(std::uint32_t)> &rd) const {
   // Register values: the active bank in profile mode; in legacy mode the
   // extension fields hold their reset, so the weighted histogram is off.
   struct {
      const std::function<std::uint32_t(std::uint32_t)> &rd;
      std::uint32_t peek(std::uint32_t off) const { return rd(off); }
   } r{rd};
   aec_config c;
   const std::uint32_t zc = r.peek(FX1_ISP_AEC_ZONE_CFG_OFFSET), zs = r.peek(FX1_ISP_AEC_ZONE_SIZE_OFFSET);
   const std::uint32_t cl = r.peek(FX1_ISP_AEC_SAMPLE_CLIP_OFFSET), th = r.peek(FX1_ISP_AEC_THRESH_OFFSET);
   c.en = r.peek(FX1_ISP_AEC_CTRL_OFFSET) & FX1_ISP_AEC_CTRL_EN_MASK;
   c.source = r.peek(FX1_ISP_TUNE_AEC_SOURCE_OFFSET) & FX1_ISP_TUNE_AEC_SOURCE_SOURCE_MASK;
   c.nx = fld(zc, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_X_MASK, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_X_SHIFT);
   c.ny = fld(zc, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_Y_MASK, FX1_ISP_AEC_ZONE_CFG_NUM_ZONE_Y_SHIFT);
   c.zw = fld(zs, FX1_ISP_AEC_ZONE_SIZE_ZONE_WIDTH_MASK, FX1_ISP_AEC_ZONE_SIZE_ZONE_WIDTH_SHIFT);
   c.zh = fld(zs, FX1_ISP_AEC_ZONE_SIZE_ZONE_HEIGHT_MASK, FX1_ISP_AEC_ZONE_SIZE_ZONE_HEIGHT_SHIFT);
   c.min_clip = fld(cl, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MIN_CLIP_MASK, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MIN_CLIP_SHIFT);
   c.max_clip = fld(cl, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MAX_CLIP_MASK, FX1_ISP_AEC_SAMPLE_CLIP_SAMPLE_MAX_CLIP_SHIFT);
   c.th_ue = fld(th, FX1_ISP_AEC_THRESH_TH_UE_MASK, FX1_ISP_AEC_THRESH_TH_UE_SHIFT);
   c.th_oe = fld(th, FX1_ISP_AEC_THRESH_TH_OE_MASK, FX1_ISP_AEC_THRESH_TH_OE_SHIFT);
   const std::uint32_t hc = r.peek(FX1_ISP_TUNE_HIST_CTRL_OFFSET);
   const std::uint32_t hs = r.peek(FX1_ISP_TUNE_HIST_START_OFFSET), he = r.peek(FX1_ISP_TUNE_HIST_END_OFFSET);
   const std::uint32_t st = r.peek(FX1_ISP_TUNE_HIST_STEP_OFFSET);
   c.hist.en = hc & FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK;
   c.hist.roi_en = hc & FX1_ISP_TUNE_HIST_CTRL_ROI_EN_MASK;
   c.hist.x0 = fld(hs, FX1_ISP_TUNE_HIST_START_X_MASK, FX1_ISP_TUNE_HIST_START_X_SHIFT);
   c.hist.y0 = fld(hs, FX1_ISP_TUNE_HIST_START_Y_MASK, FX1_ISP_TUNE_HIST_START_Y_SHIFT);
   c.hist.x1 = fld(he, FX1_ISP_TUNE_HIST_END_X_MASK, FX1_ISP_TUNE_HIST_END_X_SHIFT);
   c.hist.y1 = fld(he, FX1_ISP_TUNE_HIST_END_Y_MASK, FX1_ISP_TUNE_HIST_END_Y_SHIFT);
   c.hist.sx = fld(st, FX1_ISP_TUNE_HIST_STEP_X_MASK, FX1_ISP_TUNE_HIST_STEP_X_SHIFT);
   c.hist.sy = fld(st, FX1_ISP_TUNE_HIST_STEP_Y_MASK, FX1_ISP_TUNE_HIST_STEP_Y_SHIFT);
   for (unsigned k = 0; k < 25; ++k) {
      c.hist.w[k] = r.peek(FX1_ISP_TUNE_HIST_WEIGHT0_OFFSET + 4u * k) & FX1_ISP_TUNE_HIST_WEIGHT0_VALUE_MASK;
   }
   return c;
}

awb_config control_unit::awb_regs() const {
   const register_file &r = regs_;
   awb_config c;
   const std::uint32_t ac = r.peek(FX1_ISP_AWB_CTRL_OFFSET);
   c.en = ac & FX1_ISP_AWB_CTRL_EN_MASK;
   c.nx = fld(ac, FX1_ISP_AWB_CTRL_NUM_ZONE_X_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_X_SHIFT);
   c.ny = fld(ac, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_MASK, FX1_ISP_AWB_CTRL_NUM_ZONE_Y_SHIFT);
   c.under = r.peek(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET) & FX1_ISP_AWB_UNDEREXPOSED_LIMIT_UNDEREXPOSED_LIMIT_MASK;
   c.sat = r.peek(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET) & FX1_ISP_AWB_SATURATION_LIMIT_SATURATION_LIMIT_MASK;
   const std::uint32_t tc = r.peek(FX1_ISP_TUNE_AWB_CTRL_OFFSET);
   c.roi_en = tc & FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK;
   c.color_en = tc & FX1_ISP_TUNE_AWB_CTRL_COLOR_EN_MASK;
   c.mask = fld(tc, FX1_ISP_TUNE_AWB_CTRL_ELLIPSE_MASK_MASK, FX1_ISP_TUNE_AWB_CTRL_ELLIPSE_MASK_SHIFT);
   const std::uint32_t rs = r.peek(FX1_ISP_TUNE_AWB_ROI_START_OFFSET), re = r.peek(FX1_ISP_TUNE_AWB_ROI_END_OFFSET);
   c.roi = {rs & 0xFFFFu, rs >> 16, re & 0xFFFFu, re >> 16};
   const std::uint32_t lim[] = {FX1_ISP_TUNE_AWB_R_LIMITS_OFFSET, FX1_ISP_TUNE_AWB_G_LIMITS_OFFSET,
                                FX1_ISP_TUNE_AWB_B_LIMITS_OFFSET};
   for (unsigned k = 0; k < 3; ++k) {
      const std::uint32_t v = r.peek(lim[k]);
      c.limits[k] = {fld(v, FX1_ISP_TUNE_AWB_R_LIMITS_MIN_MASK, FX1_ISP_TUNE_AWB_R_LIMITS_MIN_SHIFT),
                     fld(v, FX1_ISP_TUNE_AWB_R_LIMITS_MAX_MASK, FX1_ISP_TUNE_AWB_R_LIMITS_MAX_SHIFT)};
   }
   c.min_g = r.peek(FX1_ISP_TUNE_AWB_MIN_G_OFFSET) & FX1_ISP_TUNE_AWB_MIN_G_VALUE_MASK;
   const std::uint32_t es = FX1_ISP_TUNE_AWB_E1_CENTER_OFFSET - FX1_ISP_TUNE_AWB_E0_CENTER_OFFSET;
   for (unsigned k = 0; k < 8; ++k) {
      awb_ellipse &e = c.ellipses[k];
      const std::uint32_t ctr = r.peek(FX1_ISP_TUNE_AWB_E0_CENTER_OFFSET + k * es);
      e.rg = ctr & 0xFFFFu;
      e.bg = ctr >> 16;
      e.a = r.peek(FX1_ISP_TUNE_AWB_E0_A_OFFSET + k * es) & 0xFFFFu;
      e.b = static_cast<std::int16_t>(r.peek(FX1_ISP_TUNE_AWB_E0_B_OFFSET + k * es) & 0xFFFFu);
      e.c = r.peek(FX1_ISP_TUNE_AWB_E0_C_OFFSET + k * es) & 0xFFFFu;
      e.r2 = r.peek(FX1_ISP_TUNE_AWB_E0_RADIUS2_OFFSET + k * es);
   }
   return c;
}

af_config control_unit::af_regs() const {
   const register_file &r = regs_;
   af_config c;
   c.en = r.peek(FX1_ISP_AF_CTRL_OFFSET) & FX1_ISP_AF_CTRL_EN_MASK;
   const std::uint32_t tc = r.peek(FX1_ISP_TUNE_AF_CTRL_OFFSET);
   c.mode = tc & FX1_ISP_TUNE_AF_CTRL_WINDOW_MODE_MASK;
   c.mask = fld(tc, FX1_ISP_TUNE_AF_CTRL_WINDOW_MASK_MASK, FX1_ISP_TUNE_AF_CTRL_WINDOW_MASK_SHIFT);
   c.noise = r.peek(FX1_ISP_TUNE_AF_NOISE_OFFSET) & FX1_ISP_TUNE_AF_NOISE_VALUE_MASK;
   const std::uint32_t ws = FX1_ISP_TUNE_AF_W1_START_OFFSET - FX1_ISP_TUNE_AF_W0_START_OFFSET;
   for (unsigned k = 0; k < 16; ++k) {
      const std::uint32_t s = r.peek(FX1_ISP_TUNE_AF_W0_START_OFFSET + k * ws);
      const std::uint32_t e = r.peek(FX1_ISP_TUNE_AF_W0_END_OFFSET + k * ws);
      c.rects[k] = {s & 0xFFFFu, s >> 16, e & 0xFFFFu, e >> 16};
   }
   return c;
}

void control_unit::awb_loss() {
   // One lost enabled measurement: OVERRUN and a saturating DROP_COUNT. Losses
   // on one edge are remembered so a simultaneous CLEAR_REPORT keeps them
   // (MAS §6.22.4.3: clear first, then new events).
   if (awb_loss_cycle_ != current_cycle_) {
      awb_loss_cycle_ = current_cycle_;
      awb_loss_count_ = 0;
   }
   ++awb_loss_count_;
   awb_overrun_ = true;
   if (awb_drops_ != std::numeric_limits<std::uint32_t>::max()) {
      ++awb_drops_;
   }
}

void control_unit::awb_admit(const geometry &g) {
   // At an enabled SOF reserve one FREE bank, bank 0 first; a bank freed on
   // this edge is not eligible yet. With an illegal grid or no FREE bank the
   // whole measurement is skipped and reported; the frame ID is consumed and
   // video continues (MAS §6.22.4.8).
   if (!awb_sof_.en) {
      awb_banks_.filling = -1;
      return;  // disabled frames leave banks and reporting unchanged
   }
   stat_meta m;
   m.frame_id = frame_id_;
   m.context_id = regs_.peek(FX1_ISP_AWB_CONTEXT_ID_OFFSET);
   m.profile_id = tune_active_id_;
   m.w = g.width;
   m.h = g.height;
   m.nx = awb_sof_.nx;
   m.ny = awb_sof_.ny;
   if (!awb_grid_legal(regs_.peek(FX1_ISP_AWB_CTRL_OFFSET), g) || !awb_banks_.admit(m, current_cycle_)) {
      awb_banks_.filling = -1;
      awb_loss();
      awb_sof_.en = false;  // the pipeline does not accumulate a skipped measurement
   }
}

bool control_unit::awb_lease_readable() const {
   // The lease is readable from the edge after ACQUIRE (user decision of
   // 2026-10-09: READ_VALID rises one core clock later; reads before return 0).
   return awb_banks_.leased >= 0 && (read_cycle_ == settled || read_cycle_ > awb_banks_.lease_cycle);
}

const result_banks<awb_payload>::bank *control_unit::awb_read_bank() const {
   return awb_lease_readable() ? awb_banks_.lease() : nullptr;
}

void control_unit::awb_result_ctrl(const register_file::write_effect &fx) {
   // W1P one-clock commands; they have no storage and read zero (MAS Table
   // 8-178, Table 6-135, §6.22.4.3).
   const std::uint32_t cmd = fx.w1p_pulse;
   if (cmd & FX1_ISP_AWB_RESULT_CTRL_CLEAR_REPORT_MASK) {
      awb_overrun_ = awb_loss_cycle_ == current_cycle_;
      awb_drops_ = awb_overrun_ ? awb_loss_count_ : 0u;
   }
   if (cmd & FX1_ISP_AWB_RESULT_CTRL_RELEASE_MASK) {
      // RELEASE wins over a simultaneous ACQUIRE; without a lease a no-op.
      if (awb_banks_.leased >= 0) {
         awb_banks_.release(current_cycle_);
      }
   } else if ((cmd & FX1_ISP_AWB_RESULT_CTRL_ACQUIRE_MASK) && awb_banks_.leased < 0) {
      // The oldest READY bank; no-op with a lease or nothing READY. A bank
      // published on this edge is acquirable from the following one.
      awb_banks_.acquire(current_cycle_);
   }
}

template <class P>
void control_unit::lease_command(result_banks<P> &e, const register_file::write_effect &fx) {
   // AE/AF commands (MAS §9.8, Table 9-198): RELEASE first, then ACQUIRE of
   // the oldest pre-edge READY bank. A missing lease on RELEASE and a second
   // ACQUIRE set NO_LEASE; ACQUIRE with nothing READY is no lease, no error.
   const std::uint32_t cmd = fx.w1p_pulse;
   if (cmd & cmd_release) {
      if (e.leased < 0) {
         tune_error(err_no_lease);
      } else {
         e.release(current_cycle_);
      }
   }
   if (cmd & cmd_acquire) {
      if (e.leased >= 0) {
         tune_error(err_no_lease);
      } else {
         e.acquire(current_cycle_);
      }
   }
}

void control_unit::stats_sof() {
   frame_id_ = ++frame_counter_;  // counts frames started, bound to this frame (ALG-STAT-01)
   // AEC set: profile mode takes the active bank; legacy mode transfers the
   // shadow set (with TUNE_AEC_SOURCE) at the SOF after a commit. A commit
   // written on the SOF edge reaches the next frame (registered Q).
   if (profile_mode()) {
      aec_active_ = aec_regs();
   } else if (regs_.registered(FX1_ISP_AEC_CTRL_OFFSET, current_cycle_) & FX1_ISP_AEC_CTRL_COMMIT_MASK) {
      aec_active_ = aec_regs();
      hw_clear(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);  // self-clearing
   }
   const geometry g = active_geometry();
   stat_meta m;
   m.frame_id = frame_id_;
   m.profile_id = tune_active_id_;
   m.w = g.width;
   m.h = g.height;
   // AEC: a legal set and a FREE bank (pre-edge state), else the measurement
   // is skipped and counted (MAS §6.21.4, §9.8).
   aec_sof_ = aec_active_;
   if (aec_sof_.en) {
      stat_meta a = m;
      a.context_id = regs_.peek(FX1_ISP_AEC_CONTEXT_ID_OFFSET);
      a.source = aec_sof_.source;
      a.depth = regs_.peek(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK;
      a.nx = aec_sof_.nx;
      a.ny = aec_sof_.ny;
      a.zw = aec_sof_.zw;
      a.zh = aec_sof_.zh;
      if (aec_sof_.legal(g.width, g.height) && aec_banks_.admit(a, current_cycle_)) {
         regs_.hw_set(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK, current_cycle_);
      } else {
         aec_banks_.drop(current_cycle_);
         aec_sof_.en = false;
      }
   } else {
      aec_banks_.filling = -1;
   }
   // AF: EN sampled once per frame as it enters the pipeline (MAS §6.23.2).
   af_sof_ = af_regs();
   if (af_sof_.en) {
      stat_meta f = m;
      f.context_id = regs_.peek(FX1_ISP_AF_CONTEXT_ID_OFFSET);
      if (af_banks_.admit(f, current_cycle_)) {
         regs_.hw_set(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK, current_cycle_);
      } else {
         af_banks_.drop(current_cycle_);
         af_sof_.en = false;
      }
   } else {
      af_banks_.filling = -1;
   }
   // AWB: configuration sampled at SOF (MAS §6.22.4.1).
   awb_sof_ = awb_regs();
   awb_admit(g);
}

void control_unit::blocks_busy(bool busy) {
   // Frame granularity: every pixel block holds pixels from the accepted SOF
   // until the pipeline has emitted the frame's last row; a bypassed block
   // still carries the stream (DEC-38).
   static constexpr std::uint32_t regs[] = {
      FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET,
      FX1_ISP_GTM_STATUS_OFFSET,      FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET,
      FX1_ISP_CNF_STATUS_OFFSET,      FX1_ISP_RESIZER_STATUS_OFFSET,
   };
   static_assert(FX1_ISP_DEMOSAIC_STATUS_BUSY_MASK == 1u && FX1_ISP_CCM_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_GAMMA_STATUS_BUSY_MASK == 1u && FX1_ISP_GTM_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_NR_2D_STATUS_BUSY_MASK == 1u && FX1_ISP_EE_STATUS_BUSY_MASK == 1u &&
                     FX1_ISP_CNF_STATUS_BUSY_MASK == 1u && FX1_ISP_RESIZER_STATUS_BUSY_MASK == 1u,
                 "busy is bit 0 of every block status register");
   for (const std::uint32_t r : regs) {
      if (busy) {
         regs_.hw_set(r, 1u, current_cycle_);
      } else {
         hw_clear(r, 1u);
      }
   }
   // The CH1 / CH2 Resizers hold pixels of frames that enabled them (MAS v1.07 §9.5.1).
   static_assert(FX1_ISP_CH1_RESIZER_STATUS_BUSY_MASK == 1u && FX1_ISP_CH2_RESIZER_STATUS_BUSY_MASK == 1u,
                 "busy is bit 0");
   const std::uint32_t ch_status[] = {FX1_ISP_CH1_RESIZER_STATUS_OFFSET, FX1_ISP_CH2_RESIZER_STATUS_OFFSET};
   for (unsigned c = 1; c < num_channels; ++c) {
      if (busy && (outputs_frame_ & (1u << c))) {
         regs_.hw_set(ch_status[c - 1], 1u, current_cycle_);
      } else {
         hw_clear(ch_status[c - 1], 1u);
      }
   }
}

void control_unit::stats_abort() {
   // The interrupted measurements publish nothing and free their banks;
   // READY and LEASED banks survive (MAS Table 9-198).
   hw_clear(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK);
   hw_clear(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK);
   aec_banks_.abort();
   af_banks_.abort();
   awb_banks_.abort();
}

void control_unit::stats_publish(const aec_payload *aec, const awb_payload *awb, const af_payload *af) {
   // Tap order AEC -> AWB -> AF; each publication makes its bank READY, sets
   // its done bit and raises stats_ready_irq, also when the done bit was still
   // set from an unread earlier frame (HAS Table 9-2, ALG-STAT-11).
   const auto published = [this](std::uint32_t status, std::uint32_t done) {
      hw_set(status, done, current_cycle_);
      regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK, current_cycle_);
   };
   if (aec && aec_banks_.filling >= 0) {
      aec_banks_.publish(*aec, current_cycle_);
      hw_clear(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_BUSY_MASK);
      if (aec->overflow) {
         tune_error(err_overflow);
      }
      published(FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK);
   }
   if (awb && awb_banks_.filling >= 0) {
      awb_banks_.publish(*awb, current_cycle_);
      published(FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK);
   }
   hw_clear(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_BUSY_MASK);
   if (af && af_banks_.filling >= 0) {
      af_banks_.publish(*af, current_cycle_);
      if (af->overflow) {
         tune_error(err_overflow);
      }
      published(FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK);
   }
}

// A result word of the leased bank: zero without a (readable) lease; a bus
// read without a lease sets NO_LEASE. The lease is readable from the edge
// after ACQUIRE, as AWB's (registered Q, V107-A6).
template <class P, class F>
void control_unit::leased_word(const result_banks<P> &e, std::uint32_t offset, F word) {
   auto lease = [this, &e]() -> const typename result_banks<P>::bank * {
      return e.leased >= 0 && (read_cycle_ == settled || read_cycle_ > e.lease_cycle) ? e.lease() : nullptr;
   };
   regs_.on_read(offset, [lease, word](std::uint32_t) -> std::uint32_t {
      const auto *b = lease();
      return b ? static_cast<std::uint32_t>(word(*b)) : 0u;
   });
   regs_.on_bus_read(offset, [this, lease](std::uint64_t) {
      if (!lease()) {
         tune_error(err_no_lease);
      }
   });
}

template <class P>
void control_unit::lease_install_hooks(result_banks<P> &e, std::uint32_t cmd, std::uint32_t index, std::uint32_t lo,
                                       std::uint32_t hi, std::uint32_t drops, std::uint32_t status,
                                       std::uint64_t (*value)(const typename result_banks<P>::bank &, std::uint32_t,
                                                              bool &)) {
   regs_.on_write(cmd, [this, &e](const register_file::write_effect &fx) { lease_command(e, fx); });
   regs_.on_write(index, [&e](const register_file::write_effect &) { e.snap_valid = false; });
   auto lease = [this, &e]() -> const typename result_banks<P>::bank * {
      return e.leased >= 0 && (read_cycle_ == settled || read_cycle_ > e.lease_cycle) ? e.lease() : nullptr;
   };
   // DATA_LO returns the low word and snapshots the high word; an index
   // outside Table 9-193 reads zero and sets INVALID.
   regs_.on_read(lo, [this, lease, index, value](std::uint32_t) -> std::uint32_t {
      const auto *b = lease();
      bool invalid = false;
      return b ? static_cast<std::uint32_t>(value(*b, regs_.peek(index), invalid)) : 0u;
   });
   regs_.on_bus_read(lo, [this, &e, lease, index, value](std::uint64_t) {
      const auto *b = lease();
      if (!b) {
         tune_error(err_no_lease);
         return;
      }
      bool invalid = false;
      e.snap_hi = static_cast<std::uint32_t>(value(*b, regs_.peek(index), invalid) >> 32);
      e.snap_valid = true;
      if (invalid) {
         tune_error(err_invalid);
      }
   });
   // DATA_HI returns that snapshot; without one, zero and NO_LEASE.
   regs_.on_read(hi, [lease, &e](std::uint32_t) { return lease() && e.snap_valid ? e.snap_hi : 0u; });
   regs_.on_bus_read(hi, [this, lease, &e](std::uint64_t) {
      if (!(lease() && e.snap_valid)) {
         tune_error(err_no_lease);
      }
   });
   // DROPS: a nonzero W1C clears the whole word; a drop on that edge wins.
   regs_.on_read(drops, [&e](std::uint32_t) { return e.drops; });
   regs_.on_write(drops, [this, &e](const register_file::write_effect &fx) {
      if (fx.data & fx.lane_mask) {
         e.clear_drops(current_cycle_);
      }
   });
   static_assert(FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK == FX1_ISP_TUNE_AF_STATUS_LEASE_VALID_MASK &&
                     FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_SHIFT == FX1_ISP_TUNE_AF_STATUS_READY_COUNT_SHIFT &&
                     FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK == FX1_ISP_TUNE_AF_STATUS_BUSY_MASK,
                 "AEC and AF share the status layout");
   // LEASE_VALID rises with readability, one edge after ACQUIRE (as AWB's
   // READ_VALID): a read on the ACQUIRE edge sees no lease yet.
   regs_.on_read(status, [&e, lease](std::uint32_t) -> std::uint32_t {
      return (lease() ? FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK : 0u) |
             (static_cast<std::uint32_t>(e.ready.size()) << FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_SHIFT) |
             (e.filling >= 0 ? FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK : 0u);
   });
}

void control_unit::stats_install_hooks() {
   using aec_bank = result_banks<aec_payload>::bank;
   using af_bank = result_banks<af_payload>::bank;
   // ---- AEC: wide port, identity and leased geometry (§9.8, §9.9.9) ----
   lease_install_hooks(aec_banks_, FX1_ISP_TUNE_AEC_CMD_OFFSET, FX1_ISP_TUNE_AEC_INDEX_OFFSET,
                       FX1_ISP_TUNE_AEC_DATA_LO_OFFSET, FX1_ISP_TUNE_AEC_DATA_HI_OFFSET,
                       FX1_ISP_TUNE_AEC_DROPS_OFFSET, FX1_ISP_TUNE_AEC_STATUS_OFFSET, &aec_index);
   leased_word(aec_banks_, FX1_ISP_AEC_FRAME_ID_OFFSET, [](const aec_bank &b) { return b.meta.frame_id; });
   leased_word(aec_banks_, FX1_ISP_TUNE_AEC_FRAME_OFFSET, [](const aec_bank &b) { return b.meta.frame_id; });
   leased_word(aec_banks_, FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET, [](const aec_bank &b) { return b.meta.context_id; });
   leased_word(aec_banks_, FX1_ISP_TUNE_AEC_PROFILE_OFFSET, [](const aec_bank &b) { return b.meta.profile_id; });
   leased_word(aec_banks_, FX1_ISP_TUNE_AEC_RESULT_META_OFFSET, [](const aec_bank &b) {
      constexpr std::uint32_t domain_raw12 = 1;  // normalized RAW12 Bayer (Table 8-184)
      return (b.meta.source << FX1_ISP_TUNE_AEC_RESULT_META_SOURCE_SHIFT) |
             (domain_raw12 << FX1_ISP_TUNE_AEC_RESULT_META_DOMAIN_SHIFT) |
             (b.meta.depth << FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_SHIFT) |
             FX1_ISP_TUNE_AEC_RESULT_META_LEASE_VALID_MASK;
   });
   leased_word(aec_banks_, FX1_ISP_TUNE_AE_FRAME_SIZE_OFFSET, [](const aec_bank &b) { return xy(b.meta.w, b.meta.h); });
   leased_word(aec_banks_, FX1_ISP_TUNE_AE_GRID_OFFSET, [](const aec_bank &b) { return xy(b.meta.nx, b.meta.ny); });
   leased_word(aec_banks_, FX1_ISP_TUNE_AE_ZONE_SIZE_OFFSET, [](const aec_bank &b) { return xy(b.meta.zw, b.meta.zh); });
   // ---- AEC narrow windows: modulo projections of the same lease (§6.21.3.3).
   // Selector 0 reads the aggregate Bayer sum/count; 1..3 return zero.
   auto agg = [this] { return (regs_.peek(FX1_ISP_AEC_CHANNEL_SEL_OFFSET) & FX1_ISP_AEC_CHANNEL_SEL_CHANNEL_SEL_MASK) == 0u; };
   auto zone = [this](const aec_bank &b) -> const aec_zone * {
      const std::uint32_t z = regs_.peek(FX1_ISP_AEC_ZONE_ADDR_OFFSET) & FX1_ISP_AEC_ZONE_ADDR_ZONE_ADDR_MASK;
      return z < b.payload.zones.size() ? &b.payload.zones[z] : nullptr;  // beyond the grid: zero
   };
   leased_word(aec_banks_, FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET,
               [agg](const aec_bank &b) { return agg() ? b.payload.sum & 0xFFFFFFFFu : 0u; });
   leased_word(aec_banks_, FX1_ISP_AEC_GLOBAL_SUM_HI_OFFSET,
               [agg](const aec_bank &b) { return agg() ? (b.payload.sum >> 32) & 1u : 0u; });
   leased_word(aec_banks_, FX1_ISP_AEC_GLOBAL_COUNT_OFFSET, [agg](const aec_bank &b) {
      return agg() ? b.payload.count & FX1_ISP_AEC_GLOBAL_COUNT_GLOBAL_COUNT_MASK : 0u;
   });
   leased_word(aec_banks_, FX1_ISP_AEC_ZONE_SUM_OFFSET, [agg, zone](const aec_bank &b) -> std::uint64_t {
      const aec_zone *z = zone(b);
      return z && agg() ? z->sum & FX1_ISP_AEC_ZONE_SUM_ZONE_SUM_MASK : 0u;
   });
   leased_word(aec_banks_, FX1_ISP_AEC_ZONE_COUNT_OFFSET, [agg, zone](const aec_bank &b) -> std::uint64_t {
      const aec_zone *z = zone(b);
      return z && agg() ? z->count & FX1_ISP_AEC_ZONE_COUNT_ZONE_COUNT_MASK : 0u;
   });
   leased_word(aec_banks_, FX1_ISP_AEC_ZONE_GREEN_OE_UE_OFFSET, [zone](const aec_bank &b) -> std::uint64_t {
      const aec_zone *z = zone(b);
      return z ? ((z->oe & 0xFFFFu) << 16) | (z->ue & 0xFFFFu) : 0u;
   });
   leased_word(aec_banks_, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET, [zone](const aec_bank &b) -> std::uint64_t {
      const aec_zone *z = zone(b);
      return z ? (std::uint64_t{z->max} << 12) | z->min : 0u;
   });
   leased_word(aec_banks_, FX1_ISP_AEC_HIST_DATA_OFFSET, [this](const aec_bank &b) -> std::uint64_t {
      const std::uint32_t i = regs_.peek(FX1_ISP_AEC_HIST_ADDR_OFFSET) & FX1_ISP_AEC_HIST_ADDR_HIST_ADDR_MASK;
      return b.payload.hist[i] & FX1_ISP_AEC_HIST_DATA_HIST_COUNT_MASK;
   });

   // ---- AF: wide port, identity; AF_STAT_DATA is the low word of a focus sum ----
   lease_install_hooks(af_banks_, FX1_ISP_TUNE_AF_CMD_OFFSET, FX1_ISP_TUNE_AF_INDEX_OFFSET,
                       FX1_ISP_TUNE_AF_DATA_LO_OFFSET, FX1_ISP_TUNE_AF_DATA_HI_OFFSET, FX1_ISP_TUNE_AF_DROPS_OFFSET,
                       FX1_ISP_TUNE_AF_STATUS_OFFSET, &af_index);
   leased_word(af_banks_, FX1_ISP_AF_FRAME_ID_OFFSET, [](const af_bank &b) { return b.meta.frame_id; });
   leased_word(af_banks_, FX1_ISP_TUNE_AF_FRAME_OFFSET, [](const af_bank &b) { return b.meta.frame_id; });
   leased_word(af_banks_, FX1_ISP_AF_RESULT_CONTEXT_ID_OFFSET, [](const af_bank &b) { return b.meta.context_id; });
   leased_word(af_banks_, FX1_ISP_TUNE_AF_PROFILE_OFFSET, [](const af_bank &b) { return b.meta.profile_id; });
   leased_word(af_banks_, FX1_ISP_AF_STAT_DATA_OFFSET, [this](const af_bank &b) -> std::uint64_t {
      const std::uint32_t z = regs_.peek(FX1_ISP_AF_STAT_ADDR_OFFSET) & FX1_ISP_AF_STAT_ADDR_ZONE_ADDR_MASK;
      return b.payload.win[z].focus & 0xFFFFFFFFu;
   });
   // SCORE_VALID: the leased bank holds published scores (V107-A24).
   regs_.on_read(FX1_ISP_AF_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      const bool leased = af_banks_.leased >= 0 && (read_cycle_ == settled || read_cycle_ > af_banks_.lease_cycle);
      return (stored & ~FX1_ISP_AF_STATUS_SCORE_VALID_MASK) | (leased ? FX1_ISP_AF_STATUS_SCORE_VALID_MASK : 0u);
   });

   // ---- AWB readout (MAS §6.22.4): only the leased bank, from the edge after
   // ACQUIRE; every result word reads 0 without a lease, without an error ----
   const auto global_word = [this](std::uint32_t off, std::uint64_t awb_result::*sum, bool high) {
      regs_.on_read(off, [this, sum, high](std::uint32_t) -> std::uint32_t {
         const auto *b = awb_read_bank();
         if (!b) {
            return 0u;
         }
         const std::uint64_t v = b->payload.global.*sum;
         return high ? static_cast<std::uint32_t>((v >> 32) & 0x7u) : static_cast<std::uint32_t>(v);
      });
   };
   global_word(FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET, &awb_result::r, true);
   global_word(FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET, &awb_result::r, false);
   global_word(FX1_ISP_AWB_GLOBAL_SUM_G_H_OFFSET, &awb_result::g, true);
   global_word(FX1_ISP_AWB_GLOBAL_SUM_G_L_OFFSET, &awb_result::g, false);
   global_word(FX1_ISP_AWB_GLOBAL_SUM_B_H_OFFSET, &awb_result::b, true);
   global_word(FX1_ISP_AWB_GLOBAL_SUM_B_L_OFFSET, &awb_result::b, false);
   regs_.on_read(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      return b ? b->payload.global.n & FX1_ISP_AWB_GLOBAL_COUNT_GLOBAL_COUNT_MASK : 0u;
   });
   regs_.on_read(FX1_ISP_AWB_FRAME_ID_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      return b ? b->meta.frame_id : 0u;
   });
   regs_.on_read(FX1_ISP_AWB_RESULT_CONTEXT_ID_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      return b ? b->meta.context_id : 0u;
   });
   regs_.on_read(FX1_ISP_TUNE_AWB_PROFILE_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      return b ? b->meta.profile_id : 0u;
   });
   regs_.on_read(FX1_ISP_TUNE_AWB_FRAME_SIZE_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      return b ? xy(b->meta.w, b->meta.h) : 0u;
   });
   // Zones of the captured grid; addresses outside it and untouched zones read
   // 0. The read port is synchronous: a read on the edge of the AWB_ZONE_ADDR
   // write returns the word of the previous address (MAS §6.22.4.3).
   regs_.on_write(FX1_ISP_AWB_ZONE_ADDR_OFFSET, [this](const register_file::write_effect &fx) {
      awb_zone_prev_ = fx.old_value & FX1_ISP_AWB_ZONE_ADDR_ZONE_ADDR_MASK;
      awb_zone_cycle_ = current_cycle_;
   });
   auto awb_zone = [this]() -> std::pair<const awb_memory *, long> {
      const auto *b = awb_read_bank();
      const bool stale = read_cycle_ != settled && awb_zone_cycle_ != settled && read_cycle_ <= awb_zone_cycle_;
      const std::uint32_t z = stale ? awb_zone_prev_
                                    : regs_.peek(FX1_ISP_AWB_ZONE_ADDR_OFFSET) & FX1_ISP_AWB_ZONE_ADDR_ZONE_ADDR_MASK;
      if (!b || z >= b->payload.mem.zcnt.size()) {
         return {nullptr, -1};
      }
      return {&b->payload.mem, static_cast<long>(z)};
   };
   const struct {
      std::uint32_t h, l;
      unsigned c;
   } awb_zone_regs[] = {{FX1_ISP_AWB_ZONE_SUM_R_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_R_L_OFFSET, 0},
                        {FX1_ISP_AWB_ZONE_SUM_G_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_G_L_OFFSET, 1},
                        {FX1_ISP_AWB_ZONE_SUM_B_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_B_L_OFFSET, 2}};
   for (const auto &zr : awb_zone_regs) {
      const unsigned c = zr.c;
      regs_.on_read(zr.h, [awb_zone, c](std::uint32_t) {
         const auto [m, z] = awb_zone();
         return m ? static_cast<std::uint32_t>((m->zsum[static_cast<std::size_t>(z)][c] >> 32) & 0x7u) : 0u;
      });
      regs_.on_read(zr.l, [awb_zone, c](std::uint32_t) {
         const auto [m, z] = awb_zone();
         return m ? static_cast<std::uint32_t>(m->zsum[static_cast<std::size_t>(z)][c]) : 0u;
      });
   }
   regs_.on_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, [awb_zone](std::uint32_t) {
      const auto [m, z] = awb_zone();
      return m ? m->zcnt[static_cast<std::size_t>(z)] & FX1_ISP_AWB_ZONE_COUNT_ZONE_COUNT_MASK : 0u;
   });
   // Ownership status and loss report (MAS Table 6-135).
   regs_.on_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET,
                  [this](const register_file::write_effect &fx) { awb_result_ctrl(fx); });
   regs_.on_read(FX1_ISP_AWB_RESULT_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const auto *b = awb_read_bank();
      std::uint32_t v = static_cast<std::uint32_t>(awb_banks_.ready.size())
                        << FX1_ISP_AWB_RESULT_STATUS_READY_COUNT_SHIFT;
      if (b) {
         v |= FX1_ISP_AWB_RESULT_STATUS_LEASED_MASK | FX1_ISP_AWB_RESULT_STATUS_READ_VALID_MASK;
         v |= static_cast<std::uint32_t>(awb_banks_.leased) << FX1_ISP_AWB_RESULT_STATUS_READ_BANK_SHIFT;
         v |= (b->meta.nx << FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_SHIFT) & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_MASK;
         v |= (b->meta.ny << FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_SHIFT) & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_MASK;
      }
      v |= awb_banks_.filling >= 0 ? FX1_ISP_AWB_RESULT_STATUS_WRITE_BUSY_MASK : 0u;
      v |= awb_overrun_ ? FX1_ISP_AWB_RESULT_STATUS_OVERRUN_MASK : 0u;
      return v;
   });
   regs_.on_read(FX1_ISP_AWB_DROP_COUNT_OFFSET, [this](std::uint32_t) { return awb_drops_; });
}

}  // namespace cdc::components::fx1_isp
