// SPDX-License-Identifier: Apache-2.0
// VP-only stream ingress of the direct CSI-2 -> ISP path (fx1_isp_ingress.h,
// registers fx1_isp_ingress_regs.h). Contract:
// components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md.
//
// Two halves:
//  - front end, called from the stream master's process (axis_cycle and the
//    capture observer): handshake, frame parsing and validation, admission
//    at the accepted SOF, the context table. It never waits.
//  - drain, an SC_THREAD of the ISP (run()): moves the buffered beats into
//    the line assembly and each complete line into the ISP's in_fifo (the
//    IDMA -> Input Formatter boundary); it blocks when in_fifo is full, which
//    fills the 16-beat FIFO and lowers TREADY.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include "engine/engines.h"
#include "fx1_isp/fx1_isp_ingress.h"

namespace cdc::components::fx1_isp {

class ingress_engine : public stream_ingress {
public:
   ingress_engine(engine_context &ctx, line_fifo &out);

   // stream_ingress
   bool axis_cycle(bool tvalid, const ingress_beat &beat) override;
   void capture_start(std::uint64_t seq, std::uint16_t frame_id, std::uint64_t timestamp_ns,
                      std::uint32_t epoch) override;
   void capture_end(std::uint64_t seq, capture_outcome outcome) override;
   void capture_stream_end(std::uint64_t seq) override;
   void source_reset(std::uint32_t epoch) override;
   void set_bound(bool bound) override { bound_ = bound; }

   void run();  // drain thread

   // ISP_INGRESS registers (32-bit word offsets).
   std::uint32_t csr_read(std::uint32_t offset);
   std::uint32_t csr_peek(std::uint32_t offset) const;
   void csr_write(std::uint32_t offset, std::uint32_t value);

   // From the ISP engines.
   // i_rst_n, soft reset or recovery (engine_context::reset_engines); `failure`:
   // a recovery for an error (outcome ISP_FAILED), else RESET.
   void isp_reset(bool failure = false);
   void isp_accepted(std::uint32_t tag, std::uint32_t isp_frame);
   void isp_retired(std::uint32_t tag, bool ok);
   void odma_end(unsigned ch, std::uint32_t tag);
   void hard_reset();  // i_rst_n asserted: registers, counters and table to reset values
   void debug_set_tag(std::uint32_t last) { tag_seq_ = last; }  // test fixtures only

private:
   enum class slot_state { free, pending, active, cancelling, ready };
   enum class csi_end { none, ok, dropped, missing_fe };
   enum class stream_end { none, complete, invalid, not_admitted };
   enum class isp_end { none, ok, failed, not_started };

   struct entry {
      slot_state state = slot_state::free;
      std::uint64_t seq = 0;
      std::uint16_t frame_id = 0;
      std::uint64_t ts = 0;
      std::uint32_t epoch = 0;
      csi_end csi = csi_end::none;
      stream_end stream = stream_end::none;
      unsigned cause = 0;  // invalid cause
      isp_end isp = isp_end::none;
      std::uint32_t forced = 0;  // outcome imposed by a reset / recovery (RESET, ISP_FAILED)
      std::uint32_t isp_frame = 0;  // ISP frame ID (statistics, BUFn_FRAME); 0: never reached the ISP
      bool boundary = false;  // no further beat of this capture (capture_stream_end, newer beat, reset)
      std::uint32_t tag = 0;
      std::uint32_t outputs = 0;  // OUTPUT_ENABLE at admission
      std::uint32_t odma_done = 0;  // channels whose ODMA has finished with the frame
      std::uint32_t lines = 0;
      std::uint32_t samples = 0;
      std::uint32_t code = 0;  // final OUTCOME code
   };

   // One accepted beat of an admitted frame, decoded (RAW16, 1..4 samples).
   struct fifo_item {
      std::uint64_t token = 0;
      std::array<std::uint16_t, 4> px{};
      unsigned n = 0;
      bool last = false;
   };
   // A frame on its way through the drain, in admission order.
   struct drain_frame {
      std::uint64_t token = 0;
      int slot = -1;
      std::uint32_t tag = 0, frame_id = 0, width = 0, height = 0;
      std::uint32_t lines_pushed = 0;
      bool invalid = false;  // no more beats come; abandon at its end
   };

   enum class fe_state { idle, in_frame, dropping };

   bool direct() const { return direct_; }
   bool idle() const;
   std::uint64_t cycle() const { return ctx_.cycle(); }
   int find_seq(std::uint64_t seq) const;
   int free_slot() const;
   void release(int slot);
   void not_admitted(int slot, unsigned reason, std::uint64_t seq, std::uint16_t frame_id);
   void retire_older_pending(std::uint64_t seq);
   void boundary_before(std::uint64_t seq);
   void retire_pending_all();
   void sof(const ingress_beat &b);
   void invalidate(unsigned cause);
   void tail_event(unsigned cause);
   void try_complete(int slot);
   void complete(int slot);
   bool admit_now(std::uint32_t outputs);
   unsigned occupying(unsigned ch) const;
   drain_frame *drain_of(std::uint64_t token);
   void notify_drain();
   void check_cancelling();
   void wait_work(std::uint64_t my_epoch);

   engine_context &ctx_;
   line_fifo &out_;
   bool bound_ = false;
   bool direct_ = false;
   std::uint32_t expected_tid_ = 0;
   std::uint32_t sticky_ = 0;
   // Counters.
   std::uint32_t beats_accepted_ = 0, frames_admitted_ = 0, drop_no_credit_ = 0, drop_no_context_ = 0,
                 drop_no_stream_ = 0, drop_config_ = 0, frames_invalid_ = 0, untracked_ = 0, beats_discarded_ = 0,
                 stall_cycles_ = 0;
   std::array<std::uint32_t, 8> cause_count_{};
   std::uint32_t last_drop_ = 0;
   std::uint64_t last_drop_seq_ = 0;

   std::array<entry, 8> table_{};
   std::deque<int> admitted_;  // slots of ACTIVE / CANCELLING / READY entries, admission order
   std::uint32_t tag_seq_ = 0;

   // Front end.
   fe_state fe_ = fe_state::idle;
   int cur_ = -1;               // slot of the open frame
   std::uint64_t cur_seq_ = 0;  // its capture sequence
   std::uint64_t cur_token_ = 0;
   std::uint32_t width_ = 0, height_ = 0, depth_mask_ = 0, x_ = 0, y_ = 0;
   int tail_ = -1;  // complete frame still open for a late abort / extra data (until the next SOF)
   std::uint64_t token_seq_ = 0;

   // Buffering.
   std::deque<fifo_item> fifo_;
   std::deque<drain_frame> frames_;
   std::vector<std::uint16_t> assembly_;
   sc_core::sc_event work_ev_;
};

}  // namespace cdc::components::fx1_isp
