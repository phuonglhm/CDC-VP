// SPDX-License-Identifier: Apache-2.0
// Profile controller (MAS v1.07 §9.7-9.9; milestone V107-3): two banks of
// algorithm scalars, INIT/SEAL/APPLY/CANCEL/UNSEAL, the pending request and
// its admission barrier, frame tokens and references, the IDLE write class.
//
// The VP captures a frame's whole configuration once, at the accepted SOF,
// and frames of two epochs never overlap (APPLY waits for the old epoch to
// drain). The register file therefore holds the datapath value of every
// bank-owned field: activation copies the bank into it at the admitted SOF,
// and every in-flight frame belongs to that bank.

#include <algorithm>

#include "control/control_unit.h"
#include "control/profile_fields.h"
#include "dma/dma_rules.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

constexpr std::uint32_t err_locked = FX1_ISP_TUNE_ERROR_LOCKED_MASK;
constexpr std::uint32_t err_invalid = FX1_ISP_TUNE_ERROR_INVALID_MASK;
constexpr std::uint32_t err_not_ready = FX1_ISP_TUNE_ERROR_NOT_READY_MASK;
constexpr std::uint32_t err_pending = FX1_ISP_TUNE_ERROR_PENDING_MASK;
constexpr std::uint32_t err_missed = FX1_ISP_TUNE_ERROR_MISSED_MASK;
constexpr std::uint32_t err_mode = FX1_ISP_TUNE_ERROR_MODE_MASK;

constexpr std::uint32_t cmd_ops = FX1_ISP_TUNE_CMD_INIT_MASK | FX1_ISP_TUNE_CMD_SEAL_MASK |
                                  FX1_ISP_TUNE_CMD_APPLY_MASK | FX1_ISP_TUNE_CMD_CANCEL_MASK |
                                  FX1_ISP_TUNE_CMD_UNSEAL_MASK;

// IDLE-class words (MAS §9.9.3).
constexpr std::uint32_t idle_class[] = {
   FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, FX1_ISP_COMMON_BAYER_OFFSET,
   FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, FX1_ISP_OUTPUT_ENABLE_OFFSET,     FX1_ISP_LSC_MESH_NODES_OFFSET,
   FX1_ISP_TUNE_MODE_OFFSET,
   // Channel geometry (MAS v1.07 §6.20.1.1, §8.3; joins in V107-7).
   FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_CH1_RESIZER_CTRL_OFFSET, FX1_ISP_CH2_RESIZER_CTRL_OFFSET,
   FX1_ISP_CH0_CROP_CTRL_OFFSET, FX1_ISP_CH0_CROP_ORIGIN_OFFSET, FX1_ISP_CH0_CROP_SIZE_OFFSET,
   FX1_ISP_CH1_CROP_CTRL_OFFSET, FX1_ISP_CH1_CROP_ORIGIN_OFFSET, FX1_ISP_CH1_CROP_SIZE_OFFSET,
   FX1_ISP_CH2_CROP_CTRL_OFFSET, FX1_ISP_CH2_CROP_ORIGIN_OFFSET, FX1_ISP_CH2_CROP_SIZE_OFFSET,
};
// Input-domain words: rejected with MODE in profile mode even when idle (§9.9.6).
constexpr std::uint32_t input_domain[] = {
   FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, FX1_ISP_COMMON_BAYER_OFFSET,
   FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, FX1_ISP_LSC_MESH_NODES_OFFSET,
};

std::uint32_t field(std::uint32_t v, std::uint32_t mask) {
   return mask ? (v & mask) >> __builtin_ctz(mask) : 0u;
}
std::int64_t s16(std::uint32_t v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

template <std::size_t N>
bool listed(const std::uint32_t (&list)[N], std::uint32_t off) {
   return std::find(std::begin(list), std::end(list), off) != std::end(list);
}

}  // namespace

// ---- state ------------------------------------------------------------------

bool control_unit::profile_mode() const {
   return (regs_.peek(FX1_ISP_TUNE_MODE_OFFSET) & FX1_ISP_TUNE_MODE_ENABLE_MASK) != 0;
}

std::uint32_t control_unit::next_frame() const { return frame_counter_ + 1u; }

bool control_unit::pending_at(std::uint64_t cycle) const {
   // A CANCEL on the admission edge loses: the request stays visible on it.
   return pending_.valid && (pending_.cancel == settled || cycle <= pending_.cancel);
}

bool control_unit::bank_active(unsigned b) const { return tune_active_valid_ && tune_active_bank_ == b; }

bool control_unit::bank_protected(unsigned b, std::uint64_t cycle) const {
   return pbanks_[b].sealed || bank_active(b) || pbanks_[b].refs > 0 || (pending_at(cycle) && pending_.bank == b);
}

unsigned control_unit::tune_bank() const {
   return regs_.peek(FX1_ISP_TUNE_BANK_OFFSET) & FX1_ISP_TUNE_BANK_VALUE_MASK;
}

std::uint32_t control_unit::profile_bank_value(unsigned bank, std::uint32_t offset) const {
   const auto it = pfield_index_.find(offset);
   return it == pfield_index_.end() ? 0u : pbanks_.at(bank).value[it->second];
}

void control_unit::tune_error(std::uint32_t mask) {
   regs_.hw_set(FX1_ISP_TUNE_ERROR_OFFSET, mask, current_cycle_);
}

bool control_unit::pipeline_idle() const {
   // MAS §9.9.3, on the pre-write edge: IDMA stopped or ISP disabled; no
   // admitted context unretired and no AXI transaction outstanding (engine
   // probe); recovery idle. The VP has no long-running automatic builder:
   // GTM curves, NR estimates and LSC preparation complete on their edge.
   const std::uint32_t dma = regs_.peek(FX1_ISP_DMA_CTRL_OFFSET);
   const std::uint32_t common = regs_.peek(FX1_ISP_COMMON_CTRL_OFFSET);
   const bool stopped = !(dma & FX1_ISP_DMA_CTRL_IDMA_EN_MASK) || !(common & FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
   // Frames admitted through accepted_sof() and not retired, and the engines'
   // probe (AXI transactions) when a TLM model is attached.
   const bool quiet = frame_tokens_.empty() && (!quiescent_ || quiescent_());
   return stopped && quiet && !soft_reset_active(current_cycle_);
}

bool control_unit::any_result_leased() const {
   return awb_banks_.leased >= 0 || aec_banks_.leased >= 0 || af_banks_.leased >= 0;  // MAS §9.9.3
}

// ---- hooks --------------------------------------------------------------------

void control_unit::profile_install_hooks() {
   for (std::size_t i = 0; i < num_profile_fields; ++i) {
      const profile_field &f = profile_fields[i];
      pfield_index_[f.offset] = i;
      // Readback of the TUNE_BANK-selected bank (Table 9-194: even when sealed).
      regs_.on_read(f.offset, [this, i](std::uint32_t stored) -> std::uint32_t {
         const profile_field &pf = profile_fields[i];
         if (!pf.extension && !profile_mode()) {
            return stored;
         }
         return (stored & ~pf.mask) | (pbanks_[tune_bank()].value[i] & pf.mask);
      });
   }
   for (const std::uint32_t off : idle_class) {
      regs_.on_pre_write(off, [this, off](std::uint32_t data, std::uint32_t lanes, std::uint64_t) -> std::uint32_t {
         const csr::reg_desc *d = regs_.find(off);
         const std::uint32_t m = d->rw_mask & lanes;
         const std::uint32_t stored = regs_.peek(off);
         if (((stored & ~m) | (data & m)) == stored) {
            return lanes;  // unchanged strobed bits: a no-op, never rejected (§9.9.3)
         }
         if (off == FX1_ISP_TUNE_MODE_OFFSET) {
            if (!pipeline_idle() || any_result_leased()) {
               tune_error(err_mode);
               return 0u;
            }
            return lanes;
         }
         if ((profile_mode() && listed(input_domain, off)) || !pipeline_idle()) {
            tune_error(err_mode);  // pinned LSC mesh state reports LOCKED from V107-4
            return 0u;
         }
         return lanes;
      });
   }
   regs_.on_write(FX1_ISP_TUNE_MODE_OFFSET, [this](const register_file::write_effect &fx) {
      const bool was = fx.old_value & FX1_ISP_TUNE_MODE_ENABLE_MASK;
      const bool now = fx.new_value & FX1_ISP_TUNE_MODE_ENABLE_MASK;
      if (was != now) {
         profile_mode_change(now);
      }
   });
   regs_.on_write(FX1_ISP_TUNE_CMD_OFFSET,
                  [this](const register_file::write_effect &fx) { profile_command(fx, current_cycle_); });
   regs_.on_write(FX1_ISP_TUNE_TABLE_CMD_OFFSET, [this](const register_file::write_effect &fx) {
      if (fx.w1p_pulse == 0) {
         return;
      }
      if (!profile_mode()) {
         tune_error(err_mode);  // legacy confirmation uses the block's own command (Table 9-195)
         return;
      }
      table_command(fx.w1p_pulse);  // bank payload confirmation (§9.9.5, V107-4)
   });
   // Legacy commands in profile mode (§9.7): AEC COMMIT is rejected with MODE.
   regs_.on_write(FX1_ISP_AEC_CTRL_OFFSET, [this](const register_file::write_effect &fx) {
      if (profile_mode() && (fx.w1s_set & FX1_ISP_AEC_CTRL_COMMIT_MASK)) {
         regs_.hw_ack(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);
         tune_error(err_mode);
      }
   });
   regs_.on_read(FX1_ISP_TUNE_NEXT_FRAME_OFFSET, [this](std::uint32_t) { return next_frame(); });
   regs_.on_read(FX1_ISP_TUNE_ACTIVE_ID_OFFSET, [this](std::uint32_t) { return tune_active_id_; });
   regs_.on_read(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET, [this](std::uint32_t) { return tune_active_frame_; });
   regs_.on_read(FX1_ISP_TUNE_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint64_t t = read_cycle_ == settled ? current_cycle_ : read_cycle_;
      const bool pend = pending_at(t);
      std::uint32_t v = tune_active_bank_ & FX1_ISP_TUNE_STATUS_ACTIVE_BANK_MASK;
      v |= tune_active_valid_ ? FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK : 0u;
      v |= pend ? FX1_ISP_TUNE_STATUS_PENDING_MASK : 0u;
      for (unsigned b = 0; b < 2; ++b) {
         const bool referenced = pbanks_[b].refs > 0;
         const bool bank_pend = pend && pending_.bank == b;
         if (bank_active(b) || referenced || pbanks_[b].sealed || bank_pend) {
            v |= b ? FX1_ISP_TUNE_STATUS_BANK1_BUSY_MASK : FX1_ISP_TUNE_STATUS_BANK0_BUSY_MASK;
         }
         if (pbanks_[b].sealed) {
            v |= b ? FX1_ISP_TUNE_STATUS_BANK1_VALID_MASK : FX1_ISP_TUNE_STATUS_BANK0_VALID_MASK;
         }
         if (bank_pend) {
            v |= b ? FX1_ISP_TUNE_STATUS_BANK1_PENDING_MASK : FX1_ISP_TUNE_STATUS_BANK0_PENDING_MASK;
         }
         if (referenced) {
            v |= b ? FX1_ISP_TUNE_STATUS_BANK1_REFERENCED_MASK : FX1_ISP_TUNE_STATUS_BANK0_REFERENCED_MASK;
         }
      }
      return v;
   });
}

// Bank-owned bits of a CSR write go to the TUNE_BANK-selected bank (profile
// mode, or an extension field in either mode); the register file receives
// the write with those bits unchanged, so the datapath value stays.
std::uint32_t control_unit::profile_route_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lanes) {
   const auto it = pfield_index_.find(addr & ~3u);
   if (it == pfield_index_.end()) {
      return data;
   }
   const profile_field &f = profile_fields[it->second];
   if (!f.extension && !profile_mode()) {
      return data;
   }
   const std::uint32_t m = f.mask & lanes;
   const std::uint32_t live = regs_.peek(f.offset);
   if (m) {
      const unsigned b = tune_bank();
      std::uint32_t &stored = pbanks_[b].value[it->second];
      const std::uint32_t next = (stored & ~m) | (data & m);
      // TUNE_BPC_COUNT is a payload write (§9.9.5): every accepted write, also
      // of the stored value, clears BPC_CONFIRM; on a protected bank it is
      // LOCKED like the payload ports. Other fields: an unchanged write is a
      // no-op.
      const bool payload = f.offset == FX1_ISP_TUNE_BPC_COUNT_OFFSET;
      if (next != stored || payload) {
         if (bank_protected(b, current_cycle_)) {
            tune_error(err_locked);  // no effect; the AXI response stays OKAY
         } else {
            stored = next;
            if (payload) {
               pbanks_[b].payload_valid &= ~FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK;
            }
         }
      }
   }
   return (data & ~f.mask) | (live & f.mask);
}

// ---- mode, commands -------------------------------------------------------------

void control_unit::profile_mode_change(bool enable) {
   // Accepted only when idle with no leased result (pre-write filter).
   pending_ = pending_apply{};
   tune_active_valid_ = false;
   tune_active_bank_ = 0;
   for (profile_bank &pb : pbanks_) {
      pb.sealed = false;  // leaving clears both seals; entering starts unsealed
      pb.refs = 0;
      pb.ee_pin = pb.lsc_pin = -1;
   }
   if (!enable) {
      // Leaving also invalidates the profile-local payload readiness (§9.7);
      // GAMMA_STATUS.LUT_LOADED and GTM manual bank 0 with it (§6.14.4.2,
      // §6.16.4.3). Resident SRAM and EE validity are kept.
      bank_payload_invalidate(0);
      bank_payload_invalidate(1);
      // Legacy datapath: extension controls apply their reset values (§9.9.2);
      // software reinitializes the legacy controls (§9.7).
      for (std::size_t i = 0; i < num_profile_fields; ++i) {
         const profile_field &f = profile_fields[i];
         if (f.extension) {
            regs_.hw_write(f.offset, f.mask, regs_.find(f.offset)->reset);
         }
      }
   }
}

void control_unit::profile_command(const register_file::write_effect &fx, std::uint64_t cycle) {
   const std::uint32_t cmd = fx.w1p_pulse;
   if (cmd == 0) {
      return;
   }
   if (!profile_mode()) {
      tune_error(err_mode);  // executes nothing, sets only MODE (§9.9.7)
      return;
   }
   const std::uint32_t ops = cmd & cmd_ops;
   const bool exact = cmd & FX1_ISP_TUNE_CMD_EXACT_MASK;
   if (__builtin_popcount(ops) != 1 || (exact && ops != FX1_ISP_TUNE_CMD_APPLY_MASK)) {
      tune_error(err_invalid);
      return;
   }
   const unsigned b = tune_bank();
   switch (ops) {
   case FX1_ISP_TUNE_CMD_INIT_MASK:
      if (bank_protected(b, cycle)) {
         tune_error(err_locked);
         return;
      }
      // Legacy algorithm scalars as they are now, extension defaults; no
      // command, status, live port or resident EE scalar is copied (§9.7).
      for (std::size_t i = 0; i < num_profile_fields; ++i) {
         const profile_field &f = profile_fields[i];
         pbanks_[b].value[i] = (f.extension ? regs_.find(f.offset)->reset : regs_.peek(f.offset)) & f.mask;
      }
      bank_payload_invalidate(b);  // bank-local payload readiness (§9.7)
      return;
   case FX1_ISP_TUNE_CMD_SEAL_MASK: {
      if (pbanks_[b].sealed) {
         return;  // already sealed: nothing to validate again
      }
      if (bank_protected(b, cycle)) {
         tune_error(err_locked);
         return;
      }
      const std::uint32_t errors = seal_errors(b);
      if (errors) {
         tune_error(errors);
         return;
      }
      pbanks_[b].sealed = true;
      // SEAL pins the selected complete residents until UNSEAL or exit (§9.9.1).
      pbanks_[b].ee_pin = (profile_bank_value(b, FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_EN_MASK)
                              ? static_cast<int>(field(profile_bank_value(b, FX1_ISP_EE_CTRL_OFFSET),
                                                       FX1_ISP_EE_CTRL_PROFILE_SEL_MASK))
                              : -1;
      pbanks_[b].lsc_pin = (profile_bank_value(b, FX1_ISP_LSC_CTRL_OFFSET) & FX1_ISP_LSC_CTRL_EN_MASK)
                               ? static_cast<int>(field(profile_bank_value(b, FX1_ISP_LSC_PROFILE_SEL_OFFSET),
                                                        FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK))
                               : -1;
      return;
   }
   case FX1_ISP_TUNE_CMD_APPLY_MASK: {
      // Error priority (§9.9.7): MODE, PENDING, NOT_READY, INVALID, MISSED.
      if (pending_at(cycle)) {
         tune_error(err_pending);
         return;
      }
      if (!pbanks_[b].sealed) {
         tune_error(err_not_ready);
         return;
      }
      const std::uint32_t target = regs_.peek(FX1_ISP_TUNE_APPLY_FRAME_OFFSET);
      const std::uint32_t d = exact ? target - next_frame() : 0u;
      if (bank_active(b) || d == 0x80000000u || (!tune_active_valid_ && d != 0u)) {
         tune_error(err_invalid);
         return;
      }
      if (d > 0x80000000u) {
         tune_error(err_missed);
         return;
      }
      pending_ = pending_apply{};
      pending_.valid = true;
      pending_.bank = b;
      pending_.id = pbanks_[b].value[pfield_index_.at(FX1_ISP_TUNE_PROFILE_ID_OFFSET)];
      pending_.exact = exact;
      pending_.target = exact ? target : 0u;
      pending_.cycle = cycle;  // eligible from the next edge
      return;
   }
   case FX1_ISP_TUNE_CMD_CANCEL_MASK:
      if (pending_at(cycle) && pending_.cancel == settled) {
         pending_.cancel = cycle;  // admission on this edge still wins
      }
      return;  // no pending request: no effect, no error
   case FX1_ISP_TUNE_CMD_UNSEAL_MASK:
      if (bank_active(b) || pbanks_[b].refs > 0 || (pending_at(cycle) && pending_.bank == b)) {
         tune_error(err_locked);
         return;
      }
      pbanks_[b].sealed = false;
      pbanks_[b].ee_pin = pbanks_[b].lsc_pin = -1;  // resident pins released
      return;
   default:
      return;
   }
}

// MAS v1.07 Table 9-196 (the AEC relations join with the v1.07 AE engine in V107-6).
std::uint32_t control_unit::seal_errors(unsigned b) const {
   auto v = [&](std::uint32_t off) { return profile_bank_value(b, off); };
   std::uint32_t e = 0;
   const geometry act = active_geometry();
   if (field(v(FX1_ISP_TUNE_NR_CTRL_OFFSET), FX1_ISP_TUNE_NR_CTRL_MODE_MASK) == 3u) {
      e |= err_invalid;
   }
   if (v(FX1_ISP_TUNE_NR_BLEND_OFFSET) > 256u || v(FX1_ISP_TUNE_CNF_BLEND_OFFSET) > 256u) {
      e |= err_invalid;
   }
   const std::uint32_t lim = v(FX1_ISP_TUNE_NR_LIMITS_OFFSET);
   if (field(lim, FX1_ISP_TUNE_NR_LIMITS_MIN_MASK) > field(lim, FX1_ISP_TUNE_NR_LIMITS_MAX_MASK)) {
      e |= err_invalid;
   }
   if (field(v(FX1_ISP_EE_CTRL_OFFSET), FX1_ISP_EE_CTRL_PROFILE_SEL_MASK) == 3u ||
       field(v(FX1_ISP_LSC_PROFILE_SEL_OFFSET), FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK) == 3u) {
      e |= err_invalid;
   }
   // AWB colour regions: A > 0, C > 0 and A*C > B^2 for each enabled ellipse;
   // A and C are unsigned Q2.14, B is signed Q1.14 (CSR v1.07 rows 934-938).
   const std::uint32_t awb = v(FX1_ISP_TUNE_AWB_CTRL_OFFSET);
   const std::uint32_t estride = FX1_ISP_TUNE_AWB_E1_A_OFFSET - FX1_ISP_TUNE_AWB_E0_A_OFFSET;
   for (unsigned k = 0; k < 8; ++k) {
      if (!(field(awb, FX1_ISP_TUNE_AWB_CTRL_ELLIPSE_MASK_MASK) & (1u << k))) {
         continue;
      }
      const std::int64_t a = v(FX1_ISP_TUNE_AWB_E0_A_OFFSET + k * estride) & 0xFFFFu;
      const std::int64_t bb = s16(v(FX1_ISP_TUNE_AWB_E0_B_OFFSET + k * estride));
      const std::int64_t c = v(FX1_ISP_TUNE_AWB_E0_C_OFFSET + k * estride) & 0xFFFFu;
      if (a <= 0 || c <= 0 || a * c <= bb * bb) {
         e |= err_invalid;
      }
   }
   // Half-open rectangle inside a w x h coordinate domain: pixels for AF and
   // the weighted histogram, complete 2x2 cells (W/2 x H/2) for the AWB ROI
   // (MAS v1.07 §6.22.3.9, §6.21.3.4).
   auto rect_bad = [](std::uint32_t start, std::uint32_t end, std::uint32_t w, std::uint32_t h) {
      const std::uint32_t x0 = start & 0xFFFFu, y0 = start >> 16, x1 = end & 0xFFFFu, y1 = end >> 16;
      return x0 >= x1 || y0 >= y1 || x1 > w || y1 > h;
   };
   if ((awb & FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK) &&
       rect_bad(v(FX1_ISP_TUNE_AWB_ROI_START_OFFSET), v(FX1_ISP_TUNE_AWB_ROI_END_OFFSET), act.width / 2,
                act.height / 2)) {
      e |= err_invalid;
   }
   if ((awb & FX1_ISP_TUNE_AWB_CTRL_COLOR_EN_MASK) && v(FX1_ISP_TUNE_AWB_MIN_G_OFFSET) == 0u) {
      e |= err_invalid;
   }
   const std::uint32_t af = v(FX1_ISP_TUNE_AF_CTRL_OFFSET);
   if (af & FX1_ISP_TUNE_AF_CTRL_WINDOW_MODE_MASK) {
      const std::uint32_t wstride = FX1_ISP_TUNE_AF_W1_START_OFFSET - FX1_ISP_TUNE_AF_W0_START_OFFSET;
      for (unsigned k = 0; k < 16; ++k) {
         if ((field(af, FX1_ISP_TUNE_AF_CTRL_WINDOW_MASK_MASK) & (1u << k)) &&
             rect_bad(v(FX1_ISP_TUNE_AF_W0_START_OFFSET + k * wstride),
                      v(FX1_ISP_TUNE_AF_W0_END_OFFSET + k * wstride), act.width, act.height)) {
            e |= err_invalid;
         }
      }
   }
   const std::uint32_t hist = v(FX1_ISP_TUNE_HIST_CTRL_OFFSET);
   if (hist & FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK) {
      const std::uint32_t step = v(FX1_ISP_TUNE_HIST_STEP_OFFSET);
      if (((hist & FX1_ISP_TUNE_HIST_CTRL_ROI_EN_MASK) &&
           rect_bad(v(FX1_ISP_TUNE_HIST_START_OFFSET), v(FX1_ISP_TUNE_HIST_END_OFFSET), act.width, act.height)) ||
          field(step, FX1_ISP_TUNE_HIST_STEP_X_MASK) == 0u || field(step, FX1_ISP_TUNE_HIST_STEP_Y_MASK) == 0u) {
         e |= err_invalid;
      }
   }
   // AWB grid for the fixed active image (§6.22.5): the same legality the
   // legacy SOF check applies.
   if ((v(FX1_ISP_AWB_CTRL_OFFSET) & FX1_ISP_AWB_CTRL_EN_MASK) && !awb_grid_legal(v(FX1_ISP_AWB_CTRL_OFFSET), act)) {
      e |= err_invalid;
   }
   // AEC (EN = 1): grid, zone-size, clip and threshold relations of §6.21.4
   // for the fixed active image, with the weighted ROI/steps (V107-6).
   const aec_config aec = aec_set(v);
   if (aec.en && !aec.legal(act.width, act.height)) {
      e |= err_invalid;
   }
   // Payload readiness (Table 9-196).
   if ((v(FX1_ISP_GAMMA_CTRL_OFFSET) & FX1_ISP_GAMMA_CTRL_EN_MASK) && !gamma_banks_[b].valid) {
      e |= err_not_ready;
   }
   const std::uint32_t gtm = v(FX1_ISP_GTM_CTRL_OFFSET);
   if ((gtm & FX1_ISP_GTM_CTRL_EN_MASK) && (gtm & FX1_ISP_GTM_CTRL_MANUAL_MASK) && !gtm_manual_[b].complete()) {
      e |= err_not_ready;
   }
   if (field(v(FX1_ISP_TUNE_NR_CTRL_OFFSET), FX1_ISP_TUNE_NR_CTRL_MODE_MASK) == 2u &&
       !(pbanks_[b].payload_valid & FX1_ISP_TUNE_TABLE_STATUS_NR_MASK)) {
      e |= err_not_ready;
   }
   if ((v(FX1_ISP_TUNE_BPC_CTRL_OFFSET) & FX1_ISP_TUNE_BPC_CTRL_FACTORY_EN_MASK) &&
       !(pbanks_[b].payload_valid & FX1_ISP_TUNE_TABLE_STATUS_BPC_MASK)) {
      e |= err_not_ready;
   }
   if ((v(FX1_ISP_TUNE_LSC_CTRL_OFFSET) & FX1_ISP_TUNE_LSC_CTRL_SECTOR_EN_MASK) &&
       !(pbanks_[b].payload_valid & FX1_ISP_TUNE_TABLE_STATUS_LSC_MASK)) {
      e |= err_not_ready;
   }
   if (v(FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_EN_MASK) {
      const unsigned sel = field(v(FX1_ISP_EE_CTRL_OFFSET), FX1_ISP_EE_CTRL_PROFILE_SEL_MASK);
      if (sel < ee_profiles && !ee_[sel].valid) {
         e |= err_not_ready;
      }
   }
   // EE radial (RADIAL_EN = 1) in the resident the bank's PROFILE_SEL selects,
   // whether or not EE is enabled (Table 9-196): CONFIRM checks no scalar
   // ranges (§9.9.1), so SEAL does.
   const unsigned ee_sel = field(v(FX1_ISP_EE_CTRL_OFFSET), FX1_ISP_EE_CTRL_PROFILE_SEL_MASK);
   if (ee_sel < ee_profiles && (ee_[ee_sel].scalar[ee_feature_en] & FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK)) {
      const auto &sc = ee_[ee_sel].scalar;
      const std::uint64_t x2 = field(sc[ee_radial_center], FX1_ISP_EE_RADIAL_CENTER_CENTER_X2_MASK);
      const std::uint64_t y2 = field(sc[ee_radial_center], FX1_ISP_EE_RADIAL_CENTER_CENTER_Y2_MASK);
      const std::uint32_t t0 = field(sc[ee_radial_r2_th0], FX1_ISP_EE_RADIAL_R2_TH0_R2_TH0_MASK);
      const std::uint32_t t1 = field(sc[ee_radial_r2_th1], FX1_ISP_EE_RADIAL_R2_TH1_R2_TH1_MASK);
      const std::uint32_t t2 = field(sc[ee_radial_r2_th2], FX1_ISP_EE_RADIAL_R2_TH2_R2_TH2_MASK);
      if (x2 + 2u > 2u * std::uint64_t{act.width} || y2 + 2u > 2u * std::uint64_t{act.height} || t0 > t1 ||
          t1 > t2) {  // CENTER_X2 > 2(W-1), CENTER_Y2 > 2(H-1)
         e |= err_invalid;
      }
   }
   if (v(FX1_ISP_LSC_CTRL_OFFSET) & FX1_ISP_LSC_CTRL_EN_MASK) {
      const unsigned sel = field(v(FX1_ISP_LSC_PROFILE_SEL_OFFSET), FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK);
      if (!lsc_profile_valid(sel) || !lsc_geometry_valid()) {
         e |= err_not_ready;
      }
   }
   return e;
}

// ---- admission, activation, retirement ------------------------------------------

control_unit::admission control_unit::admission_state(std::uint64_t cycle, unsigned in_flight) const {
   if (in_flight >= context_credits) {
      return admission::wait_event;  // context credits exhausted (§9.7)
   }
   if (recovery_busy(cycle)) {
      return admission::wait_edge;  // no admission while a recovery runs (§9.9.8)
   }
   // Every enabled channel needs a settled, valid geometry (MAS v1.07
   // §6.20.1.1, §8.3): a derivation in progress completes on a later edge;
   // an invalid setup waits for software.
   const std::uint32_t outputs = regs_.peek(FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      if (outputs & (1u << ch)) {
         const std::uint32_t st = geometry_status(ch, cycle);
         if (st & FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK) {
            return admission::wait_edge;
         }
         if (!(st & FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK)) {
            return admission::wait_event;
         }
      }
   }
   if (!profile_mode()) {
      return admission::go;
   }
   if ((regs_.peek(FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK) == 0u) {
      return admission::wait_event;  // no output: held without an error (§9.9.7)
   }
   if (pending_at(cycle)) {
      const bool due = !pending_.exact || next_frame() == pending_.target;
      if (due) {
         if (cycle <= pending_.cycle) {
            // Not yet eligible: the old profile may still admit on this edge.
            return tune_active_valid_ ? admission::go : admission::wait_edge;
         }
         return in_flight > 0 ? admission::wait_event : admission::go;  // drain barrier
      }
      // An exact future target: older sequences use the old profile.
   }
   return tune_active_valid_ ? admission::go : admission::wait_event;
}

void control_unit::profile_admit(std::uint64_t cycle) {
   if (!profile_mode()) {
      frame_tokens_.push_back(-1);
      return;
   }
   if (pending_at(cycle) && cycle > pending_.cycle && (!pending_.exact || next_frame() == pending_.target)) {
      // Activation at the accepted target SOF (the old epoch has drained).
      const unsigned b = pending_.bank;
      for (std::size_t i = 0; i < num_profile_fields; ++i) {
         regs_.hw_write(profile_fields[i].offset, profile_fields[i].mask, pbanks_[b].value[i]);
      }
      tune_active_valid_ = true;
      tune_active_bank_ = b;
      epoch_id_ = pending_.id;
      tune_active_frame_ = next_frame();
      pending_ = pending_apply{};
      // Automatic GTM and NR histories are scoped to the profile epoch (§9.7):
      // the new epoch starts without them. A manual GTM curve is software
      // payload, not history (its bank ownership lands in V107-4).
      if (!(regs_.peek(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_MANUAL_MASK)) {
         gtm_valid_ = false;
         gtm_pending_ = false;
      }
      nr_var_ = 0;
      nr_have_ = false;
   }
   frame_tokens_.push_back(static_cast<int>(tune_active_bank_));
   ++pbanks_[tune_active_bank_].refs;
   tune_active_id_ = epoch_id_;
}

void control_unit::frame_retired() {
   if (!live_frames_.empty()) {
      live_frames_.pop_front();  // frames retire in admission order
   }
   if (frame_tokens_.empty()) {
      return;  // retired by a reset already
   }
   // Frames of one epoch share one bank (activation waits for the drain), so
   // retirement order does not matter for the reference count.
   const int b = frame_tokens_.front();
   frame_tokens_.pop_front();
   if (b >= 0 && pbanks_[static_cast<unsigned>(b)].refs > 0) {
      --pbanks_[static_cast<unsigned>(b)].refs;
   }
}

void control_unit::profile_reset(reset_scope scope, bool hard) {
   // Every reset aborts the in-flight frames: their references retire once.
   frame_tokens_.clear();
   for (profile_bank &pb : pbanks_) {
      pb.refs = 0;
   }
   pending_ = pending_apply{};  // DMA recovery cancels it without MISSED (§9.9.7)
   if (scope == reset_scope::dma && !hard) {
      return;  // the sealed active profile survives DMA recovery
   }
   // Hard/common reset: active and pending state, seals and table validity
   // (§9.7, §9.1: a common soft reset requires reloading and applying a
   // profile). Hard reset also returns the bank scalars to their resets.
   tune_active_valid_ = false;
   tune_active_bank_ = 0;
   tune_active_id_ = 0;
   tune_active_frame_ = 0;
   ee_active_valid_ = false;  // EE_STATUS.active_valid clears on hard/common reset (§9.9.1)
   epoch_id_ = 0;
   const bool table_reset = hard || profile_mode();  // a legacy common reset keeps table validity
   if (table_reset) {
      bank_payload_invalidate(0);
      bank_payload_invalidate(1);
      ee_invalidate_all();  // profile-mode common reset and hard reset (§9.9.1)
   }
   for (profile_bank &pb : pbanks_) {
      pb.sealed = false;
      pb.ee_pin = pb.lsc_pin = -1;
      if (hard) {
         pb.value.resize(num_profile_fields);
         for (std::size_t i = 0; i < num_profile_fields; ++i) {
            pb.value[i] = regs_.find(profile_fields[i].offset)->reset & profile_fields[i].mask;
         }
      }
   }
}

}  // namespace cdc::components::fx1_isp
