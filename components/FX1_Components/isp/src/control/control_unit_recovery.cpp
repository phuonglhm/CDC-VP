// SPDX-License-Identifier: Apache-2.0
// Observable recovery (MAS v1.07 §9.9.8): request, abort set, drain, reset
// pulse, final retirement; COMMON_RECOVERY_STATUS / SEQUENCE and the four
// aborted-frame records (CSR 0x8C00..0x8C2C).
#include <algorithm>
#include <iterator>

#include "control/control_unit.h"

namespace cdc::components::fx1_isp {

namespace {

constexpr std::uint32_t abort_frame[4] = {FX1_ISP_COMMON_ABORT0_FRAME_OFFSET, FX1_ISP_COMMON_ABORT1_FRAME_OFFSET,
                                          FX1_ISP_COMMON_ABORT2_FRAME_OFFSET, FX1_ISP_COMMON_ABORT3_FRAME_OFFSET};
constexpr std::uint32_t abort_outcome[4] = {FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET, FX1_ISP_COMMON_ABORT1_OUTCOME_OFFSET,
                                            FX1_ISP_COMMON_ABORT2_OUTCOME_OFFSET, FX1_ISP_COMMON_ABORT3_OUTCOME_OFFSET};

// Ownership bits cleared at the final retirement of either scope.
constexpr std::uint32_t ownership[] = {FX1_ISP_IDMA_BUF_VALID_OFFSET, FX1_ISP_ODMA_BUF_FREE_OFFSET,
                                       FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET};

}  // namespace

bool control_unit::ownership_word(std::uint32_t offset) {
   return std::find(std::begin(ownership), std::end(ownership), offset) != std::end(ownership);
}

bool control_unit::recovery_busy(std::uint64_t cycle) const { return rec_.busy && cycle < rec_.final_edge; }

void control_unit::output_done(std::uint32_t ingress, unsigned ch) {
   for (live_frame &f : live_frames_) {
      if (f.ingress == ingress) {
         f.done |= 1u << ch;
         return;
      }
   }
}

bool control_unit::recovery_request(reset_scope scope, unsigned cause, std::uint64_t cycle) {
   recovery_sync(cycle);
   const bool software = cause == rc_dma || cause == rc_common;
   if (rec_.busy) {
      // Only hard reset escalates a recovery; a software request is rejected,
      // an error joins (its sticky DMA_ERR bit is already set).
      if (software) {
         tune_error(FX1_ISP_TUNE_ERROR_PENDING_MASK);
      }
      return false;
   }
   current_cycle_ = cycle;
   ++rec_seq_;
   rec_ = recovery_state{};
   rec_.busy = true;
   rec_.scope = scope;
   rec_.request = cycle;
   // The abort set: every admitted, unretired frame, in admission order; the
   // enabled outputs that had not published DONE when it was captured.
   for (const live_frame &f : live_frames_) {
      if (rec_.count == rec_.records.size()) {
         break;
      }
      rec_.records[rec_.count++] = {f.ingress, f.outputs & ~f.done, cause};
   }
   live_frames_.clear();
   hw_clear(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK);
   regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, cycle);
   // The engines stop at once (no later address for aborted work); the
   // drain ends when the transactions already issued have their responses.
   if (soft_reset_hook_) {
      soft_reset_hook_(cycle);
   }
   const std::uint64_t drained = drain_probe_ ? drain_probe_(cycle) : cycle;
   rec_.pulse = drained == never ? never : std::max(cycle, drained);
   rec_.final_edge = rec_.pulse == never ? never : rec_.pulse + soft_reset_cycles_;
   soft_reset_end_ = rec_.final_edge;
   regs_.set_command_block(true);
   if (rec_.pulse == cycle) {
      apply_soft_reset(cycle, scope);
      rec_.pulse_applied = true;
   }
   return true;
}

void control_unit::recovery_drained(std::uint64_t cycle) {
   if (!rec_.busy || rec_.pulse != never) {
      return;
   }
   rec_.pulse = std::max(cycle, rec_.request);
   rec_.final_edge = rec_.pulse + soft_reset_cycles_;
   soft_reset_end_ = rec_.final_edge;
}

void control_unit::recovery_sync(std::uint64_t t) {
   if (!rec_.busy) {
      return;
   }
   if (!rec_.pulse_applied && rec_.pulse <= t) {
      rec_.pulse_applied = true;
      apply_soft_reset(rec_.pulse, rec_.scope);
   }
   if (rec_.final_edge <= t) {
      recovery_final(rec_.final_edge);
   }
}

void control_unit::recovery_final(std::uint64_t cycle) {
   // Final retirement: every input / output ownership bit is cleared on this
   // edge (kept through drain and pulse; the command block drops W1S writes
   // until here; review V107-7..10 R8). The engines' rotations restarted at
   // the request; the DMA scope kept DONE, published metadata, DMA_ERR and
   // DMA_IRQ_STAT. The records and DONE are published atomically and BUSY
   // clears.
   for (const std::uint32_t off : ownership) {
      regs_.hw_write(off, 0xFFFFFFFFu, 0);
   }
   std::uint32_t failed = 0;
   for (unsigned i = 0; i < rec_.count; ++i) {
      failed |= rec_.records[i].failed;
   }
   rec_records_ = rec_.records;
   rec_count_ = rec_.count;
   if (failed) {  // ERROR[c]: an enabled channel-c output retired without DONE (Table 9-188)
      regs_.hw_set(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET, failed << FX1_ISP_OUTPUT_IRQ_STATUS_ERROR_SHIFT, cycle);
   }
   if (rec_.count) {  // the latest frame retired at every output, by recovery
      regs_.hw_set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, cycle);
      regs_.hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK, cycle);
   }
   regs_.hw_set(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK, cycle);
   rec_.busy = false;
}

void control_unit::recovery_install_hooks() {
   // Status and records at the edge of the read: BUSY / PHASE / SCOPE of the
   // running recovery; the records of the last completed one (read them with
   // BUSY = 0 and a stable SEQUENCE).
   regs_.on_read(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET, [this](std::uint32_t stored) -> std::uint32_t {
      const std::uint64_t t = read_cycle_ == settled ? current_cycle_ : read_cycle_;
      const bool done_now = rec_.busy && rec_.final_edge <= t;
      std::uint32_t v = stored & FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK;
      unsigned count = rec_count_;
      if (rec_.busy && !done_now) {
         const unsigned phase = t < rec_.pulse ? 1u : 2u;  // drain, reset pulse (final retirement: one edge)
         const unsigned scope = rec_.scope == reset_scope::common ? 2u : 1u;
         v |= FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK | (phase << FX1_ISP_COMMON_RECOVERY_STATUS_PHASE_SHIFT) |
              (scope << FX1_ISP_COMMON_RECOVERY_STATUS_SCOPE_SHIFT);
      } else if (done_now) {
         v |= FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK;
         count = rec_.count;
      }
      return v | (count << FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_SHIFT);
   });
   regs_.on_read(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET, [this](std::uint32_t) { return rec_seq_; });
   for (unsigned i = 0; i < 4; ++i) {
      auto record = [this, i]() -> abort_record {
         const std::uint64_t t = read_cycle_ == settled ? current_cycle_ : read_cycle_;
         const bool done_now = rec_.busy && rec_.final_edge <= t;
         const unsigned count = done_now ? rec_.count : rec_count_;
         return i < count ? (done_now ? rec_.records[i] : rec_records_[i]) : abort_record{};
      };
      regs_.on_read(abort_frame[i], [record](std::uint32_t) { return record().frame; });
      regs_.on_read(abort_outcome[i], [record](std::uint32_t) {
         const abort_record r = record();
         return (r.failed << FX1_ISP_COMMON_ABORT0_OUTCOME_FAILED_OUTPUTS_SHIFT) |
                (r.cause << FX1_ISP_COMMON_ABORT0_OUTCOME_CAUSE_SHIFT);
      });
   }
}

}  // namespace cdc::components::fx1_isp
