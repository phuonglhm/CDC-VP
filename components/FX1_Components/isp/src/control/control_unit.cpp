// SPDX-License-Identifier: Apache-2.0
#include "control/control_unit.h"

#include <algorithm>

#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

namespace {

// Registers whose writes are commands for block models outside the control
// unit (the DMA engines). Without an installed block hook they
// are only recorded.
constexpr std::uint32_t command_registers[] = {
   FX1_ISP_COMMON_CTRL_OFFSET,    FX1_ISP_DMA_CTRL_OFFSET,       FX1_ISP_IDMA_BUF_VALID_OFFSET,
   FX1_ISP_ODMA_BUF_FREE_OFFSET,  FX1_ISP_ODMA_BUF_DONE_OFFSET,
};

// Error sources of COMMON_STATUS.ERROR / COMMON_IRQ_STATUS.ERROR_IRQ (MAS
// v1.07 Table 9-188, DEC-15 superseded): DMA_ERR.ALIGN, AXI_RD and AXI_WR,
// any LSC_ERROR bit, EE_STATUS.ERROR and any TUNE_ERROR bit; ERROR_IRQ also
// on every accepted recovery. Flow-control conditions (IDMA_UNDERRUN,
// ODMA_OVERFLOW) and statistics drops (BPC candidate overflow) set neither.
struct error_source {
   std::uint32_t offset;
   std::uint32_t mask;
};
constexpr error_source error_sources[] = {
   {FX1_ISP_LSC_ERROR_OFFSET, 0x3Fu},
   {FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK},
   {FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT | FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT |
                                FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT},
   {FX1_ISP_TUNE_ERROR_OFFSET, 0xFFu},
};

// Statistics publication events feeding STATS_READY_IRQ (HAS Table 9-2).
constexpr error_source stats_sources[] = {
   {FX1_ISP_AEC_STATUS_OFFSET, FX1_ISP_AEC_STATUS_STAT_DONE_MASK},
   {FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK},
   {FX1_ISP_AF_STATUS_OFFSET, FX1_ISP_AF_STATUS_FRAME_DONE_MASK},
};

// Hardware state a soft reset preserves (DEC-14); everything else that is not
// RW configuration returns to its reset value.
constexpr std::uint32_t soft_reset_preserved[] = {
   FX1_ISP_IDMA_FRAME_COUNT_OFFSET, FX1_ISP_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_AEC_FRAME_ID_OFFSET,
   FX1_ISP_AWB_FRAME_ID_OFFSET,     FX1_ISP_AF_FRAME_ID_OFFSET,      FX1_ISP_LSC_PROFILE_STATUS_OFFSET,
   FX1_ISP_CH1_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_CH2_ODMA_FRAME_COUNT_OFFSET,
   // Recovery status and records are outside the pulse domain (MAS v1.07 §9.9.8).
   FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET,
   FX1_ISP_COMMON_ABORT0_FRAME_OFFSET, FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET, FX1_ISP_COMMON_ABORT1_FRAME_OFFSET,
   FX1_ISP_COMMON_ABORT1_OUTCOME_OFFSET, FX1_ISP_COMMON_ABORT2_FRAME_OFFSET, FX1_ISP_COMMON_ABORT2_OUTCOME_OFFSET,
   FX1_ISP_COMMON_ABORT3_FRAME_OFFSET, FX1_ISP_COMMON_ABORT3_OUTCOME_OFFSET,
};

// DMA-scope recovery keeps the DONE bits and published metadata of buffers
// that completed before it, DMA_ERR and DMA_IRQ_STAT (MAS v1.07 §9.9.8); the
// common scope clears them.
bool dma_scope_kept(std::uint32_t off) {
   if (off == FX1_ISP_DMA_ERR_OFFSET || off == FX1_ISP_DMA_IRQ_STAT_OFFSET || off == FX1_ISP_ODMA_BUF_DONE_OFFSET ||
       off == FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET || off == FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET) {
      return true;
   }
   const auto in = [off](std::uint32_t first, std::uint32_t words) { return off >= first && off < first + 4u * words; };
   return in(FX1_ISP_ODMA_BUF_TAG0_OFFSET, 16) || in(FX1_ISP_CH1_ODMA_BUF_TAG0_OFFSET, 16) ||
          in(FX1_ISP_CH2_ODMA_BUF_TAG0_OFFSET, 16);
}

// Per-channel geometry registers (CSR v1.07): the Resizer page of CH0
// (0x2000), CH1 (0x3200) and CH2 (0x3400), and the crop/status block of each
// channel (0x8B40 + 0x40 * ch).
struct channel_regs {
   std::uint32_t resizer_ctrl, out_w, out_h, crop_ctrl, crop_origin, crop_size, status, active_origin, active_size;
};
constexpr channel_regs ch_regs[num_channels] = {
   {FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_RESIZER_OUT_W_OFFSET, FX1_ISP_RESIZER_OUT_H_OFFSET,
    FX1_ISP_CH0_CROP_CTRL_OFFSET, FX1_ISP_CH0_CROP_ORIGIN_OFFSET, FX1_ISP_CH0_CROP_SIZE_OFFSET,
    FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH0_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH0_ACTIVE_CROP_SIZE_OFFSET},
   {FX1_ISP_CH1_RESIZER_CTRL_OFFSET, FX1_ISP_CH1_RESIZER_OUT_W_OFFSET, FX1_ISP_CH1_RESIZER_OUT_H_OFFSET,
    FX1_ISP_CH1_CROP_CTRL_OFFSET, FX1_ISP_CH1_CROP_ORIGIN_OFFSET, FX1_ISP_CH1_CROP_SIZE_OFFSET,
    FX1_ISP_CH1_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH1_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH1_ACTIVE_CROP_SIZE_OFFSET},
   {FX1_ISP_CH2_RESIZER_CTRL_OFFSET, FX1_ISP_CH2_RESIZER_OUT_W_OFFSET, FX1_ISP_CH2_RESIZER_OUT_H_OFFSET,
    FX1_ISP_CH2_CROP_CTRL_OFFSET, FX1_ISP_CH2_CROP_ORIGIN_OFFSET, FX1_ISP_CH2_CROP_SIZE_OFFSET,
    FX1_ISP_CH2_GEOMETRY_STATUS_OFFSET, FX1_ISP_CH2_ACTIVE_CROP_ORIGIN_OFFSET, FX1_ISP_CH2_ACTIVE_CROP_SIZE_OFFSET},
};
static_assert(FX1_ISP_CH1_RESIZER_CTRL_SCALE_MASK == FX1_ISP_RESIZER_CTRL_SCALE_MASK &&
                  FX1_ISP_CH2_RESIZER_CTRL_SCALE_MASK == FX1_ISP_RESIZER_CTRL_SCALE_MASK &&
                  FX1_ISP_CH1_CROP_SIZE_W_MASK == FX1_ISP_CH0_CROP_SIZE_W_MASK &&
                  FX1_ISP_CH2_CROP_ORIGIN_Y_MASK == FX1_ISP_CH0_CROP_ORIGIN_Y_MASK,
              "the three channels share one register layout");

// No configuration set is gated by an `updated` bit since CSR/MAS 07/10:
// CCM and CNF are sampled at the accepted SOF (owner Q1, U1); the Resizer
// publishes a derived set (MAS §6.20.4.3, U4). DEC-24/26/27 are superseded.

// Power-up content of an internal RAM word (MAS §6.5.2: undefined after
// reset). A fixed, deterministic but meaningless pattern, the same as the
// reference's (reference/fx1_isp_ref/regs.py, poison), so that a block enabled
// without its table gives a visibly wrong picture (user decision 2026-10-09).
std::uint16_t ram_poison(unsigned i, std::uint32_t mask) {
   return static_cast<std::uint16_t>(((i * 0x9E37u) ^ 0xA5A5u) & mask);
}

std::uint32_t field(std::uint32_t reg, std::uint32_t mask, std::uint32_t shift) {
   return (reg & mask) >> shift;
}

}  // namespace

control_unit::control_unit(unsigned soft_reset_cycles) : soft_reset_cycles_(soft_reset_cycles) {
   install_hooks();
   external_reset();
   release_reset(0);  // a standalone control unit starts out of reset at edge 0
}

void control_unit::external_reset() {
   regs_.reset();
   profile_reset(reset_scope::common, true);
   commands_.clear();  // the commands' state was reset with the registers
   time_base_epoch_ = settled;  // FRAME_COUNTER held at zero until the release
   soft_reset_end_ = 0;
   rec_ = recovery_state{};  // i_rst_n clears the recovery status and records
   rec_seq_ = 0;
   rec_records_ = {};
   rec_count_ = 0;
   live_frames_.clear();
   // LUT contents are undefined after i_rst_n (MAS §6.5.2; CSR-16 superseded):
   // poison pattern, identical to the reference.
   for (unsigned b = 0; b < gamma_banks_.size(); ++b) {
      gamma_banks_[b] = table_bank{};
      for (unsigned i = 0; i < gamma_banks_[b].v.size(); ++i) {
         gamma_banks_[b].v[i] = ram_poison(256u * b + i, 0xFFFu);  // bank 0 as the single table had
      }
   }
   for (unsigned b = 0; b < gtm_manual_.size(); ++b) {
      gtm_manual_[b] = gtm_manual{};
      for (unsigned i = 0; i < gtm_manual_[b].v.size(); ++i) {
         gtm_manual_[b].v[i] = ram_poison(65u * (b + 2u) + i, 0xFFFFu);
      }
   }
   for (table_payload &t : tbank_) {
      t = table_payload{};  // contents undefined; only the write flags matter
   }
   ee_active_profile_ = 0;
   ee_active_valid_ = false;
   // EE: every profile's scalars take the CSR reset values (MAS p189); its
   // tables the poison pattern, profile 0 at the indices the single table had.
   for (unsigned p = 0; p < ee_profiles; ++p) {
      for (unsigned k = 0; k < ee_num_scalars; ++k) {
         ee_[p].scalar[k] = regs_.peek(FX1_ISP_EE_ALPHA_OFFSET + 4u * k);
      }
      for (unsigned t = 0; t < ee_[p].lut.size(); ++t) {
         for (unsigned i = 0; i < ee_[p].lut[t].size(); ++i) {
            ee_[p].lut[t][i] = ram_poison(256u * p + 64u * t + i, 0xFFFFu);
         }
      }
      ee_[p].written.reset();
      ee_[p].valid = false;
      ee_[p].write_cycle = settled;
   }
   ee_frame_ = ee_frame_set{};
   ee_frame_.scalar = ee_[0].scalar;
   for (unsigned b = 0; b < gtm_banks_.size(); ++b) {
      for (unsigned i = 0; i < gtm_banks_[b].size(); ++i) {
         gtm_banks_[b][i] = ram_poison(65u * b + i, 0xFFFFu);
      }
   }
   gtm_read_ = 0;
   gtm_valid_ = false;
   gtm_pending_ = false;
   gtm_sof_ = gtm_capture{};
   nr_var_ = 0;
   nr_have_ = false;
   stats_reset(true);
   lsc_ = lsc_state{};  // i_rst_n: validity and load state cleared, meshes undefined
   // Resizer: the reset set is published; derivation restarts from the live
   // configuration (MAS §6.20.4.3-4).
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      rz_[ch] = resizer_ctrl{};
      rz_[ch].live = resizer_live_key(ch);
      rz_frame_[ch] = resizer_set{};
   }
   outputs_frame_ = 1;
}

void control_unit::install_hooks() {
   profile_install_hooks();
   for (const std::uint32_t off : command_registers) {
      regs_.on_write(off, [this](const register_file::write_effect &fx) { dispatch(fx, current_cycle_); });
   }

   tables_install_hooks();  // gamma, GTM manual, EE residents, bank payloads (V107-4)

   // LSC coefficient load sequencer and status (HAS §6.8.8, CSR rows 92-117).
   regs_.on_write(FX1_ISP_LSC_LOAD_CTRL_OFFSET, [this](const register_file::write_effect &fx) { lsc_load_ctrl(fx); });
   regs_.on_write(FX1_ISP_LSC_COEF_DATA_OFFSET, [this](const register_file::write_effect &fx) { lsc_coef_data(fx); });
   regs_.on_write(FX1_ISP_LSC_PROFILE_SEL_OFFSET,
                  [this](const register_file::write_effect &fx) { lsc_profile_sel(fx); });
   regs_.on_write(FX1_ISP_LSC_MESH_NODES_OFFSET, [this](const register_file::write_effect &fx) {
      if (fx.new_value != fx.old_value) {
         lsc_.valid.fill(false);  // a geometry change invalidates every profile (p62)
      }
   });
   regs_.on_read(FX1_ISP_LSC_LOAD_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      const std::uint32_t expected = lsc_expected_count();
      const bool full = expected > 0 && lsc_.count == expected;
      return (lsc_.count & 0xFFFFu) | (full ? FX1_ISP_LSC_LOAD_STATUS_LOAD_FULL_MASK : 0u) |
             (lsc_.busy ? FX1_ISP_LSC_LOAD_STATUS_LOAD_BUSY_MASK : 0u) |
             (lsc_.dest << FX1_ISP_LSC_LOAD_STATUS_LOAD_PROFILE_SHIFT);
   });
   regs_.on_read(FX1_ISP_LSC_PROFILE_STATUS_OFFSET, [this](std::uint32_t) -> std::uint32_t {
      std::uint32_t v = 0;
      for (unsigned i = 0; i < lsc_profiles; ++i) {
         v |= lsc_.valid[i] ? 1u << i : 0u;
      }
      v |= lsc_.active << FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_SHIFT;
      v |= lsc_profile_valid(lsc_.active) ? FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_VALID_MASK : 0u;
      v |= lsc_geometry_valid() ? FX1_ISP_LSC_PROFILE_STATUS_GEOMETRY_VALID_MASK : 0u;
      return v;
   });

   // Derived geometry: the published set of each channel, as seen at the
   // edge of the read (MAS v1.07 §6.20.4.2, §8.3).
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      const channel_regs &r = ch_regs[ch];
      regs_.on_read(r.out_w, [this, ch](std::uint32_t) { return resizer_published(ch, read_cycle_).out.width; });
      regs_.on_read(r.out_h, [this, ch](std::uint32_t) { return resizer_published(ch, read_cycle_).out.height; });
      regs_.on_read(r.status, [this, ch](std::uint32_t) { return geometry_status(ch, read_cycle_); });
      regs_.on_read(r.active_origin, [this, ch](std::uint32_t) {
         const resizer_set s = resizer_published(ch, read_cycle_);
         return s.crop_x | (s.crop_y << FX1_ISP_CH0_ACTIVE_CROP_ORIGIN_Y_SHIFT);
      });
      regs_.on_read(r.active_size, [this, ch](std::uint32_t) {
         const resizer_set s = resizer_published(ch, read_cycle_);
         return s.crop_w | (s.crop_h << FX1_ISP_CH0_ACTIVE_CROP_SIZE_H_SHIFT);
      });
   }

   // FRAME_COUNTER at the edge of the read; a settled read (no cycle) sees
   // the last edge the control unit has processed.
   // Capability discovery reports implemented behaviour only (V107-A1).
   regs_.on_read(FX1_ISP_TUNE_CAPS_OFFSET, [](std::uint32_t) { return implemented_caps; });

   regs_.on_read(FX1_ISP_FRAME_COUNTER_OFFSET, [this](std::uint32_t) {
      return frame_counter(read_cycle_ == settled ? current_cycle_ : read_cycle_);
   });

   // COMMON_STATUS.error is the live OR of the error flags (DEC-15).
   regs_.on_read(FX1_ISP_COMMON_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      stored &= ~FX1_ISP_COMMON_STATUS_ERROR_MASK;
      return stored | (error_flags() ? FX1_ISP_COMMON_STATUS_ERROR_MASK : 0u);
   });

   regs_.on_hw_set([this](std::uint32_t offset, std::uint32_t newly) { on_hw_set(offset, newly); });
   stats_install_hooks();
   recovery_install_hooks();

   // Channel geometry keys (MAS v1.07 §6.20.4.3, §8.3): the active geometry
   // (frame size, Bayer trim, input format) is part of every channel's key;
   // each channel adds its crop and Resizer control.
   for (const std::uint32_t off : {FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET,
                                   FX1_ISP_COMMON_BAYER_OFFSET, FX1_ISP_COMMON_INPUT_FORMAT_OFFSET}) {
      regs_.on_write(off, [this](const register_file::write_effect &fx) {
         if (fx.new_value != fx.old_value) {
            ee_invalidate_all();  // an input-domain change clears every EE resident (MAS v1.07 §9.9.1)
         }
         resizer_key_written();
      });
   }
   for (const channel_regs &r : ch_regs) {
      for (const std::uint32_t off : {r.resizer_ctrl, r.crop_ctrl, r.crop_origin, r.crop_size}) {
         regs_.on_write(off, [this](const register_file::write_effect &) { resizer_key_written(); });
      }
   }
   // Enabling a channel whose settled geometry is invalid is an invalid setup.
   regs_.on_write(FX1_ISP_OUTPUT_ENABLE_OFFSET, [this](const register_file::write_effect &fx) {
      const std::uint32_t newly = fx.new_value & ~fx.old_value & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
      for (unsigned ch = 0; ch < num_channels; ++ch) {
         const resizer_ctrl &c = rz_[ch];
         if ((newly & (1u << ch)) && c.live == c.published.key && !c.published.valid) {
            tune_error(FX1_ISP_TUNE_ERROR_INVALID_MASK);
         }
      }
   });
}

void control_unit::csr_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                             std::uint64_t cycle) {
   advance_to(cycle);
   current_cycle_ = cycle;
   data = profile_route_write(addr, data, lane_mask);  // bank-owned bits (V107-3)
   regs_.sw_write(addr, data, lane_mask, cycle);
}

void control_unit::advance_to(std::uint64_t cycle) {
   geometry_sync(cycle);
   recovery_sync(cycle);
   current_cycle_ = cycle;  // hardware events that follow happen on this edge
   if (cycle >= soft_reset_end_) {
      regs_.set_command_block(false);
   }
}

void control_unit::dispatch(const register_file::write_effect &fx, std::uint64_t cycle) {
   const bool common_reset =
       fx.offset == FX1_ISP_COMMON_CTRL_OFFSET && (fx.w1sc_pulse & FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
   const bool dma_reset =
       fx.offset == FX1_ISP_DMA_CTRL_OFFSET && (fx.w1sc_pulse & FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
   const bool soft_reset = common_reset || dma_reset;
   if (fx.offset == FX1_ISP_COMMON_CTRL_OFFSET && (fx.w1s_set & FX1_ISP_COMMON_CTRL_FRAME_START_MASK)) {
      // Compatibility only (CSR v1.07): accepted, self-clears, no effect on
      // the input DMA, admission or any other state (V107-2).
      regs_.hw_ack(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_FRAME_START_MASK);
   }
   if (soft_reset) {
      // A recovery request (MAS v1.07 §9.9.8); rejected with PENDING while
      // one is busy (a rewrite no longer restarts the pulse).
      recovery_request(common_reset ? reset_scope::common : reset_scope::dma, common_reset ? rc_common : rc_dma, cycle);
   }
   const auto hook = block_hooks_.find(fx.offset);
   if (hook != block_hooks_.end() && hook->second) {
      hook->second(fx, cycle);
   } else if (!soft_reset) {
      // A soft-reset write is consumed here; it is not a pending command.
      commands_.push_back({cycle, fx});
   }
}

void control_unit::apply_soft_reset(std::uint64_t cycle, reset_scope scope) {
   // MAS Table 183 (p316-317). Both sources: RW configuration, LUT/mesh
   // contents, LSC profile validity and the frame counters survive (FRAME_IDs
   // keep counting, so the aborted frame's number is skipped); buffer
   // ownership and the frame in flight are discarded.
   //   common: every other hardware-owned bit (status, results, interrupt and
   //           error state, commands) returns to reset, except a pending
   //           AEC_CTRL.commit (owner answer Q4: a soft reset does not cancel
   //           it; it is consumed at the next accepted SOF).
   //   dma:    only the DMA block (from DMA_CTRL) and the pipeline busy flag;
   //           statistics, the 2DNR estimate, an LSC load and the pipeline
   //           commands are untouched (assumption U1-A1: COMMON_IRQ_STATUS is
   //           not a DMA interrupt register and is kept).
   const bool common = scope == reset_scope::common;
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      const csr::reg_desc &d = csr::registers[i];
      if (std::find(std::begin(soft_reset_preserved), std::end(soft_reset_preserved), d.offset) !=
          std::end(soft_reset_preserved)) {
         continue;
      }
      // DMA scope: the DMA blocks 0x3000..0x7FFF; the TUNE/extension pages
      // (0x8000 up) belong to the common scope (MAS v1.07 §9.7, §9.9.8).
      if (!common && (d.offset < FX1_ISP_DMA_CTRL_OFFSET || d.offset >= 0x8000u || dma_scope_kept(d.offset))) {
         continue;
      }
      if (ownership_word(d.offset)) {
         continue;  // cleared at the final retirement (recovery_final)
      }
      std::uint32_t hw_owned = d.ro_mask | d.w1c_mask | d.w1s_mask;
      if (d.offset == FX1_ISP_AEC_CTRL_OFFSET) {
         hw_owned &= ~FX1_ISP_AEC_CTRL_COMMIT_MASK;
      }
      if (hw_owned) {
         regs_.hw_hold(d.offset, hw_owned, false, cycle);
         regs_.hw_write(d.offset, hw_owned, d.reset);
      }
   }
   if (common) {
      // Resizer: the published set returns to its reset value and the
      // derivation restarts from the live configuration after the pulse
      // (MAS §6.20.4.4, Table 124 "Abort", "Restart after an abort").
      for (unsigned ch = 0; ch < num_channels; ++ch) {
         resizer_ctrl &c = rz_[ch];
         c.published = resizer_set{};
         c.calc = false;  // the dividers restart too
         c.invalid_pending = false;
         c.not_before = c.free_at = cycle + soft_reset_cycles_;
         c.live = resizer_live_key(ch);
         c.live_since = c.not_before;
      }
      commands_.clear();  // pending commands are part of the state being reset
      nr_var_ = 0;  // ALG-2DNR-09: a measured statistic, cleared like the published results
      nr_have_ = false;
      stats_reset(false);
      lsc_.busy = false;  // a load in progress is discarded; meshes and validity survive
      lsc_.count = 0;
      lsc_.staging.clear();
   } else {
      // The affected frame is abandoned: the pipeline is flushed to a frame
      // boundary and returns to idle (MAS §6.5.3, §9.1.1). The busy flags of
      // that frame drop with it, as on any other abort; the results it had
      // not yet published are never published, earlier ones are kept. The
      // engines of the old epoch are aborted by the hook below, so none of
      // them can clear the busy flags of a later frame.
      hw_clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
      blocks_busy(false);
      stats_abort();
   }
   profile_reset(scope, false);
}

void control_unit::hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
   current_cycle_ = cycle;  // derived COMMON_IRQ_STATUS sets carry the same cycle
   regs_.hw_set(offset, mask, cycle);
}

void control_unit::hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle) {
   current_cycle_ = cycle;
   regs_.hw_hold(offset, mask, level, cycle);
}

void control_unit::accepted_sof(std::uint64_t cycle) {
   current_cycle_ = cycle;
   profile_admit(cycle);  // activation of a due APPLY, frame token (V107-3)
   gtm_sof_.manual = regs_.peek(FX1_ISP_GTM_CTRL_OFFSET) & FX1_ISP_GTM_CTRL_MANUAL_MASK;
   gtm_sof_.key = regs_.peek(FX1_ISP_GTM_KEY_OFFSET) & FX1_ISP_GTM_KEY_KEY_MASK;
   gtm_sof_.lwhite = regs_.peek(FX1_ISP_GTM_LWHITE_OFFSET) & FX1_ISP_GTM_LWHITE_LWHITE_MASK;
   hw_clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
   // The frame takes the Resizer set published before this edge; a set
   // published on the SOF edge is the next frame's (MAS §6.20.4.3).
   geometry_sync(cycle);
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      rz_frame_[ch] = rz_[ch].published;
   }
   outputs_frame_ = regs_.peek(FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
   if (gtm_pending_) {  // bank swap on the SOF beat (HAS p112)
      gtm_read_ ^= 1u;
      gtm_pending_ = false;
      gtm_valid_ = true;
   }
   stats_sof();
   live_frames_.push_back({frame_id_, outputs_frame_, 0});
   ee_sof();
   // BPC counters clear at every accepted SOF and count during an enabled
   // frame (MAS §6.9 p73; the model writes the frame's counts at its
   // end, assumption U2-A3). Counting while BPC_EN = 0 is not done (U0-Q4).
   regs_.hw_write(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET, 0x3FFFu, 0);
   regs_.hw_write(FX1_ISP_BPC_NUM_DEFECTIVE_OFFSET, 0x3FFFu, 0);
   // LSC_PROFILE_SEL becomes the active profile at the accepted SOF only if
   // that profile is valid then (MAS Table 31, Figure 29). The selector was
   // checked when written, but a LOAD_BEGIN or an abort since then can have
   // invalidated it: the previous profile stays active and PROFILE_SEL_REJECT
   // is reported at every SOF while the selector differs from it. A later
   // successful validate lets the pending selection take effect.
   const unsigned sel = regs_.peek(FX1_ISP_LSC_PROFILE_SEL_OFFSET) & FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK;
   if (sel != lsc_.active) {
      if (sel < lsc_profiles && lsc_.valid[sel] && !(lsc_.busy && sel == lsc_.dest)) {
         lsc_.active = sel;
      } else {
         lsc_error(FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
      }
   }
}

void control_unit::on_hw_set(std::uint32_t offset, std::uint32_t newly) {
   for (const auto &src : error_sources) {
      if (src.offset == offset && (newly & src.mask)) {
         regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, current_cycle_);
      }
   }
   for (const auto &src : stats_sources) {
      if (src.offset == offset && (newly & src.mask)) {
         regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK,
                      current_cycle_);
      }
   }
}

std::uint32_t control_unit::error_flags() const {
   std::uint32_t any = 0;
   for (const auto &src : error_sources) {
      any |= regs_.peek(src.offset) & src.mask;
   }
   return any;
}

bool control_unit::irq_level() const {
   const std::uint32_t common = regs_.peek(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & regs_.peek(FX1_ISP_COMMON_IRQ_EN_OFFSET);
   const std::uint32_t dma = regs_.peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & regs_.peek(FX1_ISP_DMA_IRQ_EN_OFFSET);
   // The output group (MAS v1.07 Table 9-188): DONE[2:0] and ERROR[6:4] per channel.
   const std::uint32_t out = regs_.peek(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET) & regs_.peek(FX1_ISP_OUTPUT_IRQ_EN_OFFSET);
   return (common | dma | out) != 0;
}

geometry control_unit::derive_active(std::uint32_t width, std::uint32_t height, std::uint32_t bayer_pattern) {
   const std::uint32_t sh = bayer_pattern & 1u;         // bit 0: horizontal shift
   const std::uint32_t sv = (bayer_pattern >> 1) & 1u;  // bit 1: vertical shift
   return {width > sh ? (width - sh) / 2u * 2u : 0u, height > sv ? (height - sv) / 2u * 2u : 0u};
}

geometry control_unit::active_geometry() const {
   return derive_active(regs_.peek(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & FX1_ISP_COMMON_FRAME_WIDTH_H_ACTIVE_MASK,
                        regs_.peek(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET) & FX1_ISP_COMMON_FRAME_HEIGHT_V_ACTIVE_MASK,
                        regs_.peek(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK);
}

// ---- Resizer configuration controller (MAS §6.20.4.3, Figure 135, Table 123) ----

namespace {

// Only a scaling key needs the dividers; a pass-through (FullRes, disabled
// scaler, target = crop) or an invalid key publishes on the next edge
// (MAS §6.20.4.3; an invalid result needs no division, V107-A27).
bool resizer_pass_through(const resizer_key &k) { return !derive_channel(k).scaling; }

}  // namespace

void control_unit::resizer_ctrl::advance(std::uint64_t t) {
   // Event-ordered: the earliest of (A) a derivation completing at E35, (B) a
   // pass-through set due, (C) a derivation starting; each edge e affects
   // observations after e. Pass-through has priority and never waits for the
   // dividers (MAS §6.20.4.3); a derivation saves the key only on its E0, so
   // writes before that edge merge into one key (review U4-R2/R3).
   constexpr std::uint64_t never = std::numeric_limits<std::uint64_t>::max();
   for (;;) {
      const bool differs = live != published.key;
      const bool bypass = resizer_pass_through(live);
      const std::uint64_t ea = calc ? e0 + resizer_derivation_cycles : never;
      const std::uint64_t eb = differs && bypass ? live_since + 1 : never;
      const std::uint64_t ec = differs && !bypass && !calc ? std::max(live_since, free_at) : never;
      const std::uint64_t e = std::min({ea, eb, ec});
      if (e >= t) {
         return;
      }
      if (e == ea) {
         // E35: publish only if the saved key still matches the live key; a
         // stale result (or one overtaken by pass-through) is dropped.
         calc = false;
         free_at = ea;
         if (calc_key == live) {
            published = derive_channel(calc_key, published);
         }
      } else if (e == eb) {
         published = derive_channel(live, published);
         if (!published.valid) {
            invalid_pending = true;  // sticky TUNE_ERROR.INVALID (MAS §8.3)
            invalid_edge = e;
         }
      } else {
         calc = true;  // E0: save the key and start both dividers
         e0 = ec;
         calc_key = live;
      }
   }
}

resizer_key control_unit::resizer_live_key(unsigned ch) const {
   const channel_regs &r = ch_regs[ch];
   const geometry a = active_geometry();
   const std::uint32_t ctrl = regs_.peek(r.resizer_ctrl);
   const std::uint32_t org = regs_.peek(r.crop_origin), size = regs_.peek(r.crop_size);
   const std::uint32_t depth = field(regs_.peek(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET),
                                     FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK, FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_SHIFT);
   resizer_key k;
   k.valid = true;
   k.ch = ch;
   k.width = a.width;
   k.height = a.height;
   k.input_ok = depth < 2u;  // RAW12 or RAW10; 2/3 reserved (MAS v1.07 Table 6-18)
   k.crop_en = (regs_.peek(r.crop_ctrl) & FX1_ISP_CH0_CROP_CTRL_ENABLE_MASK) != 0;
   k.crop_x = field(org, FX1_ISP_CH0_CROP_ORIGIN_X_MASK, FX1_ISP_CH0_CROP_ORIGIN_X_SHIFT);
   k.crop_y = field(org, FX1_ISP_CH0_CROP_ORIGIN_Y_MASK, FX1_ISP_CH0_CROP_ORIGIN_Y_SHIFT);
   k.crop_w = field(size, FX1_ISP_CH0_CROP_SIZE_W_MASK, FX1_ISP_CH0_CROP_SIZE_W_SHIFT);
   k.crop_h = field(size, FX1_ISP_CH0_CROP_SIZE_H_MASK, FX1_ISP_CH0_CROP_SIZE_H_SHIFT);
   k.scale = field(ctrl, FX1_ISP_RESIZER_CTRL_SCALE_MASK, FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);
   k.en = (ctrl & FX1_ISP_RESIZER_CTRL_EN_MASK) != 0;
   return k;
}

void control_unit::resizer_key_written() {
   // The write lands after edge current_cycle_: everything up to that edge
   // used the previous key. Several writes before one edge give one key.
   geometry_sync(current_cycle_ + 1);
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      resizer_ctrl &c = rz_[ch];
      const resizer_key k = resizer_live_key(ch);
      if (k == c.live) {
         continue;
      }
      c.live = k;
      // Stable from the next edge, and never before the end of a reset pulse
      // (MAS Table 124: derivation restarts from the live configuration).
      c.live_since = std::max(current_cycle_ + 1, c.not_before);
   }
}

void control_unit::geometry_sync(std::uint64_t t) {
   // An invalid setup of an enabled channel sets the sticky TUNE_ERROR.INVALID
   // on its publication edge; a disabled channel's geometry admits nothing
   // and reports only its ERROR level (V107-A30).
   const std::uint32_t outputs = regs_.peek(FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
   for (unsigned ch = 0; ch < num_channels; ++ch) {
      resizer_ctrl &c = rz_[ch];
      c.advance(t);
      if (c.invalid_pending) {
         c.invalid_pending = false;
         if (outputs & (1u << ch)) {
            regs_.hw_set(FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_INVALID_MASK, c.invalid_edge);
         }
      }
   }
}

resizer_set control_unit::resizer_published(unsigned ch, std::uint64_t cycle) const {
   resizer_ctrl c = rz_.at(ch);  // observation only: advance a copy
   c.advance(cycle);
   return c.published;
}

std::uint32_t control_unit::geometry_status(unsigned ch, std::uint64_t cycle) const {
   resizer_ctrl c = rz_.at(ch);
   c.advance(cycle);
   if (c.live != c.published.key) {
      return FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK;  // BUSY implies VALID = 0 and ERROR = 0
   }
   return (c.published.valid ? FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK : 0u) |
          (c.published.error << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT);
}

void control_unit::set_block_hook(std::uint32_t offset, block_hook hook) {
   block_hooks_[offset] = std::move(hook);
}

// ---- LSC coefficient load (HAS §6.8.8, Table 6-23; ALG-LSC-04..11) ----------------

bool control_unit::lsc_geometry_valid() const {
   const std::uint32_t nodes = regs_.peek(FX1_ISP_LSC_MESH_NODES_OFFSET);
   const std::uint32_t nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
   const std::uint32_t ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
   const geometry act = active_geometry();
   return nx >= 2 && ny >= 2 && nx <= lsc_mesh_max && ny <= lsc_mesh_max && nx <= act.width && ny <= act.height;
}

std::uint32_t control_unit::lsc_expected_count() const {
   const std::uint32_t nodes = regs_.peek(FX1_ISP_LSC_MESH_NODES_OFFSET);
   const std::uint32_t nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
   const std::uint32_t ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
   return 4u * nx * ny;
}

void control_unit::lsc_error(std::uint32_t mask) { hw_set(FX1_ISP_LSC_ERROR_OFFSET, mask, current_cycle_); }

void control_unit::lsc_load_ctrl(const register_file::write_effect &fx) {
   const std::uint32_t cmd = fx.w1s_set;
   const std::uint32_t cmds = FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK | FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK |
                              FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK;
   if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK) {           // abort > validate > begin
      lsc_.busy = false;  // the destination stays invalid until reloaded (MAS Table 31, owner Q5)
      lsc_.staging.clear();
   } else if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK) {
      if (!lsc_.busy) {
         lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      } else {
         // The mesh may have been resized after LOAD_BEGIN. The count alone
         // then describes words received, not words stored in the staging bank.
         const auto expected = lsc_expected_count();
         if (lsc_geometry_valid() && lsc_.count == expected && lsc_.staging.size() == expected) {
            lsc_.mesh[lsc_.dest] = lsc_.staging;
            lsc_.valid[lsc_.dest] = true;
         } else {
            lsc_error(FX1_ISP_LSC_ERROR_COEF_COUNT_MASK);        // the profile stays invalid
         }
         lsc_.busy = false;
         lsc_.staging.clear();
      }
   } else if (cmd & FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK) {
      const unsigned dest = fx.new_value & FX1_ISP_LSC_LOAD_CTRL_LOAD_PROFILE_MASK;
      if (dest < lsc_profiles && lsc_resident_owned(dest, current_cycle_)) {
         tune_error(FX1_ISP_TUNE_ERROR_LOCKED_MASK);  // a pinned destination cannot change (§9.9.1)
         return;
      }
      if (lsc_.busy || dest >= lsc_profiles) {
         lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      } else if (dest == lsc_.active) {
         // The active profile is never a destination, valid or not (MAS Table 31,
         // owner Q5): boot by loading profile 1 or 2, validate, then select it.
         lsc_error(FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
      } else if (!lsc_geometry_valid()) {
         lsc_error(FX1_ISP_LSC_ERROR_GEOMETRY_INVALID_MASK);
      } else {
         lsc_.busy = true;
         lsc_.count = 0;
         lsc_.dest = dest;
         lsc_.valid[dest] = false;  // its previous mesh is being overwritten
         lsc_.staging.assign(lsc_expected_count(), 0);
      }
   }
   regs_.hw_ack(FX1_ISP_LSC_LOAD_CTRL_OFFSET, cmds);  // self-feedback clear (MAS §8.1.3)
}

void control_unit::lsc_coef_data(const register_file::write_effect &fx) {
   if (!lsc_.busy) {
      lsc_error(FX1_ISP_LSC_ERROR_LOAD_PROTOCOL_MASK);
      return;
   }
   const std::uint32_t data = fx.data & fx.lane_mask;
   if (data > 0x100000u) {
      lsc_error(FX1_ISP_LSC_ERROR_COEF_RANGE_MASK);              // above 4.0 (ALG-LSC-03)
   }
   if (lsc_.count < lsc_.staging.size()) {
      lsc_.staging[lsc_.count] = data & FX1_ISP_LSC_COEF_DATA_COEF_DATA_MASK;
   }
   lsc_.count = std::min<std::uint32_t>(lsc_.count + 1u, 0x1FFFFu);
}

void control_unit::lsc_profile_sel(const register_file::write_effect &fx) {
   if (profile_mode()) {
      return;  // a bank scalar: validated at SEAL, applied at activation (MAS v1.07 §9.9.4)
   }
   const unsigned sel = fx.new_value & FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK;
   if (sel >= lsc_profiles || !lsc_.valid[sel] || (lsc_.busy && sel == lsc_.dest)) {
      lsc_error(FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
      regs_.hw_write(FX1_ISP_LSC_PROFILE_SEL_OFFSET, FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK, fx.old_value);
   }
}

// ---- GTM, 2DNR, EE state -------------------------------------------------------

void control_unit::gtm_end_of_frame(std::uint32_t yavg) {
   // The builder uses the measured frame's KEY, LWHITE and MANUAL captured at
   // its accepted SOF, not the live CSRs at EOF (MAS v1.07 §6.16.4.1, §6.16.4.3).
   if (gtm_sof_.manual) {
      return;  // curve generator held; the software curve persists (ALG-GTM-07)
   }
   gtm_banks_[gtm_read_ ^ 1u] = pipe::gtm_build_curve(yavg, gtm_sof_.key, gtm_sof_.lwhite);
   gtm_pending_ = true;
}

void control_unit::nr_publish(std::uint32_t var) {
   nr_var_ = var & FX1_ISP_NR_2D_EST_VAR_EST_VAR_MASK;
   nr_have_ = true;
   regs_.hw_write(FX1_ISP_NR_2D_EST_VAR_OFFSET, FX1_ISP_NR_2D_EST_VAR_EST_VAR_MASK, nr_var_);
}

static_assert(FX1_ISP_EE_RADIAL_GAIN_23_OFFSET == FX1_ISP_EE_ALPHA_OFFSET + 4u * (ee_num_scalars - 1),
              "EE profile registers are contiguous, in ee_scalar order");
static_assert(FX1_ISP_EE_CLAMP_OFFSET == FX1_ISP_EE_ALPHA_OFFSET + 4u * ee_clamp &&
                  FX1_ISP_EE_RADIAL_CENTER_OFFSET == FX1_ISP_EE_ALPHA_OFFSET + 4u * ee_radial_center,
              "EE profile register order");

void control_unit::ee_sof() {
   // PROFILE_SEL (code 3 selects profile 0), the enable and the profile's
   // scalars are sampled at the accepted SOF, so a frame never mixes two
   // profiles (MAS §6.18.4.1). Its LUTs stay live.
   const std::uint32_t ctrl = regs_.peek(FX1_ISP_EE_CTRL_OFFSET);
   const unsigned sel = field(ctrl, FX1_ISP_EE_CTRL_PROFILE_SEL_MASK, FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT);
   ee_frame_.profile = sel < ee_profiles ? sel : 0u;
   // An unconfirmed selected resident passes the frame through (MAS v1.07 §9.9.1).
   ee_active_profile_ = ee_frame_.profile;
   ee_active_valid_ = ee_[ee_frame_.profile].valid;
   ee_frame_.en = (ctrl & FX1_ISP_EE_CTRL_EN_MASK) != 0 && ee_active_valid_;
   ee_frame_.scalar = ee_[ee_frame_.profile].scalar;
   const geometry g = active_geometry();
   ee_frame_.width = g.width;
   ee_frame_.height = g.height;
}

void control_unit::ee_end_of_frame() {
   // Radial gain range error: raised once for every frame EE processes (EE
   // enabled, frame at least 5 x 5) while radial gain is enabled and a gain of
   // the frame's profile exceeds 1.5; sticky W1C, so it sets again at the next
   // such frame (MAS §6.18.4.1, §9.2). The gain is clamped on use.
   const ee_frame_set &f = ee_frame_;
   const std::uint32_t g01 = f.scalar[ee_radial_gain_01], g23 = f.scalar[ee_radial_gain_23];
   const bool over = (g01 & 0xFFFFu) > 0xC000u || (g01 >> 16) > 0xC000u || (g23 & 0xFFFFu) > 0xC000u ||
                     (g23 >> 16) > 0xC000u;
   if (f.en && f.width >= 5 && f.height >= 5 &&
       (f.scalar[ee_feature_en] & FX1_ISP_EE_FEATURE_EN_RADIAL_EN_MASK) && over) {
      hw_set(FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_EE_STATUS_ERROR_MASK, current_cycle_);
   }
}

}  // namespace cdc::components::fx1_isp
