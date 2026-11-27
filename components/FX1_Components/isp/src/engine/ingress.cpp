// SPDX-License-Identifier: Apache-2.0
#include "engine/ingress.h"

#include "dma/dma_rules.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_ingress_regs.h"

namespace cdc::components::fx1_isp {

namespace {

constexpr std::size_t fifo_depth = FX1_ISP_INGRESS_FIFO_BEATS;
constexpr std::uint32_t version = 1;

unsigned popcount8(std::uint8_t v) {
   unsigned n = 0;
   for (; v; v &= static_cast<std::uint8_t>(v - 1u)) {
      ++n;
   }
   return n;
}

}  // namespace

ingress_engine::ingress_engine(engine_context &ctx, line_fifo &out) : ctx_(ctx), out_(out) {}

// ---- helpers -------------------------------------------------------------------

bool ingress_engine::idle() const {
   if (fe_ == fe_state::in_frame || !fifo_.empty() || !frames_.empty()) {
      return false;
   }
   for (const entry &e : table_) {
      if (e.state == slot_state::active || e.state == slot_state::cancelling) {
         return false;
      }
   }
   return true;
}

int ingress_engine::find_seq(std::uint64_t seq) const {
   if (seq == 0) {
      return -1;
   }
   for (std::size_t i = 0; i < table_.size(); ++i) {
      const entry &e = table_[i];
      if (e.state != slot_state::free && e.state != slot_state::ready && e.seq == seq) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

int ingress_engine::free_slot() const {
   for (std::size_t i = 0; i < table_.size(); ++i) {
      if (table_[i].state == slot_state::free) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

void ingress_engine::release(int slot) { table_[static_cast<std::size_t>(slot)] = entry{}; }

void ingress_engine::notify_drain() { work_ev_.notify(sc_core::SC_ZERO_TIME); }

ingress_engine::drain_frame *ingress_engine::drain_of(std::uint64_t token) {
   for (drain_frame &d : frames_) {
      if (d.token == token) {
         return &d;
      }
   }
   return nullptr;
}

// A frame that is not admitted is recorded by counters only (contract §9.1
// rule 4): no readable record. Its entry, if any, is released once the
// capture's CSI outcome is known as well.
void ingress_engine::not_admitted(int slot, unsigned reason, std::uint64_t seq, std::uint16_t frame_id) {
   switch (reason) {
   case FX1_ISP_INGRESS_DROP_REASON_NO_CREDIT: ++drop_no_credit_; break;
   case FX1_ISP_INGRESS_DROP_REASON_NO_CONTEXT: ++drop_no_context_; break;
   case FX1_ISP_INGRESS_DROP_REASON_NO_STREAM: ++drop_no_stream_; break;
   default: ++drop_config_; break;
   }
   last_drop_ = (reason & FX1_ISP_INGRESS_LAST_DROP_REASON_MASK) |
                ((std::uint32_t{frame_id} << FX1_ISP_INGRESS_LAST_DROP_FRAME_ID_SHIFT) &
                 FX1_ISP_INGRESS_LAST_DROP_FRAME_ID_MASK);
   last_drop_seq_ = seq;
   if (slot < 0) {
      return;
   }
   entry &e = table_[static_cast<std::size_t>(slot)];
   e.stream = stream_end::not_admitted;
   if (e.csi != csi_end::none) {
      release(slot);
   }
}

// Rule 5: a PENDING capture older than the capture of an accepted SOF never
// sent its own SOF (one in-order linear stream). Plain integer order of the
// 64-bit capture sequence; newer entries are left alone.
void ingress_engine::retire_older_pending(std::uint64_t seq) {
   for (std::size_t i = 0; i < table_.size(); ++i) {
      entry &e = table_[i];
      if (e.state == slot_state::pending && e.stream == stream_end::none && e.seq < seq) {
         not_admitted(static_cast<int>(i), FX1_ISP_INGRESS_DROP_REASON_NO_STREAM, e.seq, e.frame_id);
      }
   }
}

void ingress_engine::retire_pending_all() {
   for (std::size_t i = 0; i < table_.size(); ++i) {
      entry &e = table_[i];
      if (e.state == slot_state::pending) {
         if (e.stream == stream_end::none) {
            not_admitted(static_cast<int>(i), FX1_ISP_INGRESS_DROP_REASON_NO_STREAM, e.seq, e.frame_id);
         }
         release(static_cast<int>(i));
      }
   }
}

// Admitted frames that hold or will take an output buffer of channel `ch`
// ahead of a new frame: the ODMA advances its rotation only at DONE, so a
// frame that has claimed the head buffer still holds it until its ODMA ends.
unsigned ingress_engine::occupying(unsigned ch) const {
   unsigned n = 0;
   for (const int s : admitted_) {
      const entry &e = table_[static_cast<std::size_t>(s)];
      const std::uint32_t bit = 1u << ch;
      n += e.state == slot_state::active && (e.outputs & bit) && !(e.odma_done & bit) && e.isp == isp_end::none;
   }
   return n;
}

// The ISP would accept the frame now (contract §8): the common admission
// rules with the credits the ingress already holds, and the buffer each
// enabled channel will take free.
bool ingress_engine::admit_now(std::uint32_t outputs) {
   if (!(ctx_.reg(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK)) {
      return false;
   }
   if (ctx_.in_reset || ctx_.ctl.soft_reset_active(cycle())) {
      return false;
   }
   if (ctx_.ctl.admission_state(cycle(), ctx_.frames_in_flight + ctx_.ingress_reserved) !=
       control_unit::admission::go) {
      return false;
   }
   if (outputs != 0 && !(ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_ODMA_EN_MASK)) {
      return false;
   }
   for (unsigned c = 0; c < num_channels; ++c) {
      if (!(outputs & (1u << c))) {
         continue;
      }
      const unsigned idx = (ctx_.odma_head[c] + occupying(c)) % dma::num_buffers;
      if (!((ctx_.reg(odma_channels[c].free) >> idx) & 1u)) {
         return false;
      }
   }
   return true;
}

// ---- outcome ---------------------------------------------------------------------

void ingress_engine::try_complete(int slot) {
   entry &e = table_[static_cast<std::size_t>(slot)];
   if (e.state == slot_state::active && e.csi != csi_end::none && e.boundary &&
       (e.stream == stream_end::complete || e.stream == stream_end::invalid) && e.isp != isp_end::none) {
      complete(slot);
   }
}

void ingress_engine::complete(int slot) {
   entry &e = table_[static_cast<std::size_t>(slot)];
   if (e.forced) {
      e.code = e.forced;
   } else if (e.csi == csi_end::dropped) {
      e.code = FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED;
   } else if (e.csi == csi_end::missing_fe) {
      e.code = FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE;
   } else if (e.stream != stream_end::complete) {
      e.code = FX1_ISP_INGRESS_OUTCOME_INVALID;
   } else if (e.isp != isp_end::ok) {
      e.code = FX1_ISP_INGRESS_OUTCOME_ISP_FAILED;
   } else {
      e.code = FX1_ISP_INGRESS_OUTCOME_OK;
   }
   if (e.code != FX1_ISP_INGRESS_OUTCOME_INVALID) {
      e.cause = 0;  // a cause qualifies INVALID only
   }
   e.state = slot_state::ready;
   if (tail_ == slot) {
      tail_ = -1;
   }
   ctx_.state_changed();
}

// The frame in progress is invalid (contract §7): its remaining beats are
// discarded until the next SOF; the drain abandons it (abort marker, or no
// ISP frame at all if its first line has not been handed over).
void ingress_engine::invalidate(unsigned cause) {
   const int slot = cur_;
   entry &e = table_[static_cast<std::size_t>(slot)];
   e.stream = stream_end::invalid;
   e.cause = cause;
   ++frames_invalid_;
   ++cause_count_[cause - 1u];
   if (drain_frame *d = drain_of(cur_token_)) {
      d->invalid = true;
   }
   fe_ = fe_state::dropping;
   cur_ = -1;
   notify_drain();
   try_complete(slot);
}

// After its last line, until the next SOF, a frame can still receive a late
// abort beat (CRC error of the last line) or extra data. It counts unless the
// capture has already ended OK (then the abort belongs to a later capture or
// to a stream disable) or the entry is READY.
void ingress_engine::tail_event(unsigned cause) {
   entry &e = table_[static_cast<std::size_t>(tail_)];
   // A null abort after a capture that ended OK belongs to a later capture or
   // to a stream disable. Extra data of the same capture is always an error:
   // a CSI OK does not prove the stream matched HEIGHT (D1a review F2).
   const bool abort_of_ok = cause == FX1_ISP_INGRESS_CAUSE_ABORT && e.csi == csi_end::ok;
   if (e.state != slot_state::active || abort_of_ok || e.stream != stream_end::complete) {
      ++beats_discarded_;
      return;
   }
   e.stream = stream_end::invalid;
   e.cause = cause;
   ++frames_invalid_;
   ++cause_count_[cause - 1u];
   const int slot = tail_;
   tail_ = -1;
   try_complete(slot);
}

// ---- front end ---------------------------------------------------------------------

void ingress_engine::sof(const ingress_beat &b) {
   tail_ = -1;
   if (b.capture_seq != 0) {
      retire_older_pending(b.capture_seq);
   }
   int slot = -1;
   for (std::size_t i = 0; i < table_.size(); ++i) {
      const entry &e = table_[i];
      if (b.capture_seq != 0 && e.state == slot_state::pending && e.stream == stream_end::none &&
          e.seq == b.capture_seq) {
         slot = static_cast<int>(i);
      }
   }
   fe_ = fe_state::dropping;
   if (slot < 0) {
      not_admitted(-1, FX1_ISP_INGRESS_DROP_REASON_NO_CONTEXT, b.capture_seq, b.frame_id);
      return;
   }
   const std::uint32_t w = ctx_.reg(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & FX1_ISP_COMMON_FRAME_WIDTH_H_ACTIVE_MASK;
   const std::uint32_t h = ctx_.reg(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET) & FX1_ISP_COMMON_FRAME_HEIGHT_V_ACTIVE_MASK;
   const std::uint32_t depth = ctx_.reg(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK;
   if (!dma::frame_geometry_ok(w, h) || depth > 1u || w > FX1_ISP_INGRESS_MAX_LINE_SAMPLES) {
      not_admitted(slot, FX1_ISP_INGRESS_DROP_REASON_CONFIG, b.capture_seq, b.frame_id);
      return;
   }
   const std::uint32_t outputs = ctx_.reg(FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
   if (!admit_now(outputs)) {
      not_admitted(slot, FX1_ISP_INGRESS_DROP_REASON_NO_CREDIT, b.capture_seq, b.frame_id);
      return;
   }
   entry &e = table_[static_cast<std::size_t>(slot)];
   e.state = slot_state::active;
   if (++tag_seq_ == 0) {
      tag_seq_ = 1;  // TAG 0 is never assigned
   }
   e.tag = tag_seq_;
   e.outputs = outputs;
   admitted_.push_back(slot);
   ++ctx_.ingress_reserved;
   // An admitted context from the accepted SOF beat (§9.9.3 IDLE, review F1).
   ctx_.set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
   ++frames_admitted_;
   cur_ = slot;
   cur_seq_ = b.capture_seq;
   cur_token_ = ++token_seq_;
   width_ = w;
   height_ = h;
   depth_mask_ = depth == 1u ? 0x3FFu : 0xFFFu;
   x_ = y_ = 0;
   fe_ = fe_state::in_frame;
   frames_.push_back(drain_frame{cur_token_, slot, e.tag, e.frame_id, w, h, 0, false});
}

bool ingress_engine::axis_cycle(bool tvalid, const ingress_beat &b) {
   const bool room = fifo_.size() < fifo_depth;
   if (!direct_) {  // memory mode (P21): always ready, every beat discarded
      beats_discarded_ += tvalid ? 1u : 0u;
      return true;
   }
   if (!tvalid) {
      return room;
   }
   // Another exposure (TID): discarded before it can touch any frame state,
   // its null abort beats included (D1a review F3).
   if ((b.tid & 3u) != expected_tid_) {
      ++beats_discarded_;
      return true;
   }
   if (b.capture_seq != 0) {
      boundary_before(b.capture_seq);
   }
   // The null abort beat: always accepted, needs no buffer and no credit.
   if (b.tkeep == 0 && b.tlast && (b.tuser & 2u)) {
      if (fe_ == fe_state::in_frame) {
         invalidate(FX1_ISP_INGRESS_CAUSE_ABORT);
      } else if (tail_ >= 0) {
         tail_event(FX1_ISP_INGRESS_CAUSE_ABORT);
      } else {
         ++beats_discarded_;
      }
      return true;
   }
   if (b.tuser & 1u) {
      if (!room) {  // the SOF waits; nothing is decided before it is accepted
         ++stall_cycles_;
         return false;
      }
      if (fe_ == fe_state::in_frame) {
         invalidate(FX1_ISP_INGRESS_CAUSE_MISSING_LINES);
      }
      sof(b);
      if (fe_ != fe_state::in_frame) {
         ++beats_discarded_;
         return true;
      }
   } else {
      if (fe_ != fe_state::in_frame) {
         if (fe_ == fe_state::idle && tail_ >= 0 && b.capture_seq == table_[static_cast<std::size_t>(tail_)].seq) {
            tail_event(FX1_ISP_INGRESS_CAUSE_EXTRA_LINES);
         } else {
            ++beats_discarded_;
         }
         return true;
      }
      if (!room) {
         ++stall_cycles_;
         return false;
      }
   }
   // A data beat of the open frame (the SOF beat included).
   const unsigned bytes = popcount8(b.tkeep);
   const bool prefix = (static_cast<unsigned>(b.tkeep) & (static_cast<unsigned>(b.tkeep) + 1u)) == 0;
   unsigned cause = 0;
   fifo_item it;
   it.token = cur_token_;
   it.n = bytes / 2u;
   it.last = b.tlast;
   if (!prefix || (bytes & 1u) || bytes == 0) {
      cause = FX1_ISP_INGRESS_CAUSE_KEEP;
   } else if (b.capture_seq != cur_seq_) {
      cause = FX1_ISP_INGRESS_CAUSE_TAG_MISMATCH;
   } else if (b.tuser & 2u) {
      cause = FX1_ISP_INGRESS_CAUSE_TUSER_ERR;
   } else {
      for (unsigned i = 0; i < it.n; ++i) {
         it.px[i] = static_cast<std::uint16_t>(b.tdata >> (16u * i));
         if (it.px[i] & ~depth_mask_) {
            cause = FX1_ISP_INGRESS_CAUSE_DEPTH;
         }
      }
      if (!cause && (x_ + it.n > width_ || (b.tlast && x_ + it.n != width_))) {
         cause = FX1_ISP_INGRESS_CAUSE_LINE_LENGTH;
      }
   }
   if (cause) {
      ++beats_discarded_;
      invalidate(cause);
      return true;
   }
   entry &e = table_[static_cast<std::size_t>(cur_)];
   fifo_.push_back(it);
   ++beats_accepted_;
   e.samples += it.n;
   x_ += it.n;
   if (b.tlast) {
      x_ = 0;
      e.lines = ++y_;
      if (y_ == height_) {  // payload complete, not yet validated (late abort: tail_event)
         e.stream = stream_end::complete;
         tail_ = cur_;
         fe_ = fe_state::idle;
         const int slot = cur_;
         cur_ = -1;
         try_complete(slot);
      }
   }
   notify_drain();
   return true;
}

// ---- capture observer --------------------------------------------------------------

void ingress_engine::capture_start(std::uint64_t seq, std::uint16_t frame_id, std::uint64_t timestamp_ns,
                                   std::uint32_t epoch) {
   if (!direct_ || seq == 0) {
      return;
   }
   const int slot = free_slot();
   if (slot < 0) {  // rule 1: untracked, nothing evicted
      ++untracked_;
      sticky_ |= FX1_ISP_INGRESS_STATUS_CTX_FULL_MASK;
      return;
   }
   entry &e = table_[static_cast<std::size_t>(slot)];
   e = entry{};
   e.state = slot_state::pending;
   e.seq = seq;
   e.frame_id = static_cast<std::uint16_t>(frame_id & 0x3FFFu);
   e.ts = timestamp_ns;
   e.epoch = epoch;
}

void ingress_engine::capture_end(std::uint64_t seq, capture_outcome outcome) {
   const int slot = find_seq(seq);
   if (slot < 0) {
      return;  // untracked capture (or not in direct mode)
   }
   entry &e = table_[static_cast<std::size_t>(slot)];
   if (e.csi != csi_end::none) {
      return;
   }
   e.csi = outcome == capture_outcome::ok ? csi_end::ok
           : outcome == capture_outcome::dropped ? csi_end::dropped
                                                  : csi_end::missing_fe;
   if (e.state == slot_state::pending && e.stream == stream_end::not_admitted) {
      release(slot);
   } else if (e.state == slot_state::active) {
      try_complete(slot);
   }
}

// The stream boundary of a capture (contract §9.1): an admitted entry may
// become READY; a capture that never sent an SOF is retired at once (exact).
void ingress_engine::capture_stream_end(std::uint64_t seq) {
   const int slot = find_seq(seq);
   if (slot < 0) {
      return;
   }
   entry &e = table_[static_cast<std::size_t>(slot)];
   e.boundary = true;
   if (e.state == slot_state::pending && e.stream == stream_end::none) {
      not_admitted(slot, FX1_ISP_INGRESS_DROP_REASON_NO_STREAM, e.seq, e.frame_id);
   } else if (e.state == slot_state::active && fe_ == fe_state::in_frame && cur_ == slot) {
      // The capture's stream ended inside its frame (D1a re-review R1): a
      // partial line (LINE_LENGTH) or fewer than HEIGHT lines (MISSING_LINES).
      // The frame is abandoned now (abort marker, or no ISP frame and its
      // credit returned), not when a next SOF happens to come.
      invalidate(x_ > 0 ? FX1_ISP_INGRESS_CAUSE_LINE_LENGTH : FX1_ISP_INGRESS_CAUSE_MISSING_LINES);
   } else if (e.state == slot_state::active) {
      if (tail_ == slot) {
         tail_ = -1;
      }
      try_complete(slot);
   }
}

// A beat of capture `seq` was offered: every older capture has finished on
// this in-order stream.
void ingress_engine::boundary_before(std::uint64_t seq) {
   for (std::size_t i = 0; i < table_.size(); ++i) {
      entry &e = table_[i];
      if (e.state == slot_state::active && !e.boundary && e.seq < seq) {
         e.boundary = true;
         if (tail_ == static_cast<int>(i)) {
            tail_ = -1;
         }
         try_complete(static_cast<int>(i));
      }
   }
}

// CSI reset (contract §10, §9.1 rule 8): TVALID dropped without TLAST. The
// open frame is abandoned, the buffers cleared; PENDING entries are released,
// admitted ones keep their reservation (CANCELLING) until the ISP retires them.
void ingress_engine::source_reset(std::uint32_t epoch) {
   (void)epoch;
   if (fe_ == fe_state::in_frame) {
      entry &e = table_[static_cast<std::size_t>(cur_)];
      e.stream = stream_end::invalid;
      e.cause = FX1_ISP_INGRESS_CAUSE_MISSING_LINES;
   }
   beats_discarded_ += static_cast<std::uint32_t>(fifo_.size());
   fifo_.clear();
   for (drain_frame &d : frames_) {
      d.invalid = true;
   }
   fe_ = fe_state::idle;
   cur_ = -1;
   tail_ = -1;
   for (std::size_t i = 0; i < table_.size(); ++i) {
      entry &e = table_[i];
      if (e.state == slot_state::pending) {
         release(static_cast<int>(i));
      } else if (e.state == slot_state::active) {
         e.state = slot_state::cancelling;
         e.forced = FX1_ISP_INGRESS_OUTCOME_RESET;
         e.boundary = true;
         if (e.isp != isp_end::none) {
            complete(static_cast<int>(i));
         }
      }
   }
   notify_drain();
}

// ---- from the ISP --------------------------------------------------------------------

void ingress_engine::odma_end(unsigned ch, std::uint32_t tag) {
   for (const int s : admitted_) {
      entry &e = table_[static_cast<std::size_t>(s)];
      if ((e.state == slot_state::active || e.state == slot_state::cancelling) && e.tag == tag) {
         e.odma_done |= 1u << ch;
         return;
      }
   }
}

void ingress_engine::isp_accepted(std::uint32_t tag, std::uint32_t isp_frame) {
   for (const int s : admitted_) {
      entry &e = table_[static_cast<std::size_t>(s)];
      if ((e.state == slot_state::active || e.state == slot_state::cancelling) && e.tag == tag) {
         e.isp_frame = isp_frame;
         return;
      }
   }
}

void ingress_engine::isp_retired(std::uint32_t tag, bool ok) {
   for (const int s : admitted_) {
      entry &e = table_[static_cast<std::size_t>(s)];
      if ((e.state == slot_state::active || e.state == slot_state::cancelling) && e.tag == tag &&
          e.isp == isp_end::none) {
         e.isp = ok ? isp_end::ok : isp_end::failed;
         if (e.state == slot_state::cancelling) {
            complete(s);
         } else {
            try_complete(s);
         }
         return;
      }
   }
}

// ISP soft / common reset, recovery (and the start of i_rst_n): every engine
// abandons its frame. Admitted entries the ISP has not retired latch RESET
// (ISP_FAILED for an error recovery) and stay CANCELLING until the ISP is
// drained (check_cancelling): reset released, no admitted frame, no AXI
// transaction outstanding (ISP MAS v1.07 §9.9.3). ODMA_BUSY is not used: the
// reset clears it while an accepted write may still await its response.
// Frames the ISP had already retired (outputs DONE) are not affected.
void ingress_engine::isp_reset(bool failure) {
   beats_discarded_ += static_cast<std::uint32_t>(fifo_.size());
   fifo_.clear();
   frames_.clear();
   assembly_.clear();
   if (fe_ == fe_state::in_frame) {
      fe_ = fe_state::dropping;
   }
   cur_ = -1;
   for (std::size_t i = 0; i < table_.size(); ++i) {
      entry &e = table_[i];
      if (e.state == slot_state::pending) {
         // Not admitted: no ISP work (§9.1 rule 8). Released; a later SOF of
         // that capture finds no entry (NO_CONTEXT), its terminal is ignored
         // (D1b review F2).
         release(static_cast<int>(i));
      } else if ((e.state == slot_state::active || e.state == slot_state::cancelling) && e.isp == isp_end::none) {
         e.state = slot_state::cancelling;
         if (!e.forced) {
            e.forced = failure ? FX1_ISP_INGRESS_OUTCOME_ISP_FAILED : FX1_ISP_INGRESS_OUTCOME_RESET;
         }
         e.boundary = true;
         e.isp = isp_end::failed;  // abandoned; READY waits for the drain
         e.odma_done = 0;
      }
   }
   // A frame the ISP had retired keeps its stream checks until its boundary
   // (extra data, late abort): the tail stays unless that entry was cancelled
   // (D1b review F1).
   if (tail_ >= 0 && table_[static_cast<std::size_t>(tail_)].state != slot_state::active) {
      tail_ = -1;
   }
   notify_drain();
}

void ingress_engine::check_cancelling() {
   const bool drained = !ctx_.in_reset && !ctx_.ctl.soft_reset_active(cycle()) && ctx_.frames_in_flight == 0 &&
                        ctx_.axi_outstanding == 0;
   for (const int s : admitted_) {
      entry &e = table_[static_cast<std::size_t>(s)];
      if (e.state == slot_state::cancelling && e.isp != isp_end::none && drained) {
         complete(s);
      }
   }
}

void ingress_engine::hard_reset() {
   isp_reset();
   table_ = {};
   admitted_.clear();
   direct_ = false;
   ctx_.direct_source = false;
   expected_tid_ = 0;
   sticky_ = 0;
   beats_accepted_ = frames_admitted_ = drop_no_credit_ = drop_no_context_ = drop_no_stream_ = drop_config_ =
       frames_invalid_ = untracked_ = beats_discarded_ = stall_cycles_ = 0;
   cause_count_ = {};
   last_drop_ = 0;
   last_drop_seq_ = 0;
   tag_seq_ = 0;
   fe_ = fe_state::idle;
}

// ---- drain thread ------------------------------------------------------------------

void ingress_engine::run() {
   for (;;) {
      const std::uint64_t e = ctx_.epoch;
      try {
         for (;;) {
            check_cancelling();
            if (frames_.empty()) {
               wait_work(e);
               continue;
            }
            drain_frame &d = frames_.front();
            if (!fifo_.empty() && fifo_.front().token == d.token) {
               const fifo_item it = fifo_.front();
               fifo_.pop_front();
               if (d.invalid) {
                  ++beats_discarded_;
                  continue;
               }
               assembly_.insert(assembly_.end(), it.px.begin(), it.px.begin() + it.n);
               if (!it.last) {
                  continue;
               }
               line l;
               l.frame = d.token;
               l.sof = d.lines_pushed == 0;
               l.eof = d.lines_pushed + 1u == d.height;
               l.width = d.width;
               l.height = d.height;
               l.tag = d.tag;
               l.ts = d.frame_id;  // BUF_TS = CSI FRAME_ID, as the memory path (P29)
               l.direct = true;
               l.samples = std::move(assembly_);
               assembly_.clear();
               const std::uint64_t token = d.token;
               out_.push(std::move(l), e);  // blocks while in_fifo is full
               drain_frame *dd = drain_of(token);
               if (dd && ++dd->lines_pushed == dd->height) {
                  frames_.pop_front();
               }
               continue;
            }
            if (d.invalid) {  // no beat of it is left: abandon it
               assembly_.clear();
               const int slot = d.slot;
               const bool started = d.lines_pushed > 0;
               const std::uint64_t token = d.token;
               frames_.pop_front();
               if (!started) {
                  sc_assert(ctx_.ingress_reserved > 0);
                  --ctx_.ingress_reserved;  // never reached the pipeline: its credit returns
                  if (ctx_.frames_in_flight == 0 && ctx_.ingress_reserved == 0) {
                     ctx_.clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
                  }
                  entry &en = table_[static_cast<std::size_t>(slot)];
                  en.isp = isp_end::not_started;
                  if (en.state == slot_state::cancelling) {
                     complete(slot);
                  } else {
                     try_complete(slot);
                  }
                  ctx_.retired.notify(sc_core::SC_ZERO_TIME);
               } else {
                  line marker;
                  marker.frame = token;
                  marker.abort = true;
                  marker.direct = true;
                  out_.push(std::move(marker), e);
               }
               continue;
            }
            wait_work(e);
         }
      } catch (const reset_abort &) {
      }
   }
}

// Waits for work. While a CANCELLING entry waits for the ISP to drain, the
// end of a soft-reset / recovery window is a time, not an event: wake there.
void ingress_engine::wait_work(std::uint64_t e) {
   bool draining = false;
   for (const int s : admitted_) {
      const entry &en = table_[static_cast<std::size_t>(s)];
      draining = draining || (en.state == slot_state::cancelling && en.isp != isp_end::none);
   }
   const std::uint64_t now = cycle();
   const std::uint64_t end = ctx_.ctl.soft_reset_end_cycle();
   ctx_.check(e);
   if (draining && !ctx_.in_reset && ctx_.ctl.soft_reset_active(now) && end != control_unit::never && end > now) {
      const sc_core::sc_time until = ctx_.params.core_period * static_cast<double>(end) - sc_core::sc_time_stamp();
      sc_core::wait(until, work_ev_ | ctx_.kick | ctx_.retired | ctx_.reset_ev);
   } else {
      sc_core::wait(work_ev_ | ctx_.kick | ctx_.retired | ctx_.reset_ev);
   }
   ctx_.check(e);
}

// ---- registers --------------------------------------------------------------------

std::uint32_t ingress_engine::csr_peek(std::uint32_t offset) const {
   const int head = admitted_.empty() ? -1 : admitted_.front();
   const entry *h = head >= 0 && table_[static_cast<std::size_t>(head)].state == slot_state::ready
                        ? &table_[static_cast<std::size_t>(head)]
                        : nullptr;
   switch (offset) {
   case FX1_ISP_INGRESS_ID_OFFSET: return FX1_ISP_INGRESS_ID_VALUE;
   case FX1_ISP_INGRESS_CAPABILITY_OFFSET:
      return (bound_ ? FX1_ISP_INGRESS_CAPABILITY_BOUND_MASK : 0u) |
             (version << FX1_ISP_INGRESS_CAPABILITY_VERSION_SHIFT) |
             (FX1_ISP_INGRESS_FIFO_BEATS << FX1_ISP_INGRESS_CAPABILITY_FIFO_BEATS_SHIFT) |
             (FX1_ISP_INGRESS_CTX_DEPTH << FX1_ISP_INGRESS_CAPABILITY_CTX_DEPTH_SHIFT) |
             (FX1_ISP_INGRESS_MAX_LINE_SAMPLES << FX1_ISP_INGRESS_CAPABILITY_MAX_LINE_SHIFT);
   case FX1_ISP_INGRESS_CTRL_OFFSET:
      return (direct_ ? FX1_ISP_INGRESS_CTRL_SOURCE_MASK : 0u) | (expected_tid_ << FX1_ISP_INGRESS_CTRL_EXPECTED_TID_SHIFT);
   case FX1_ISP_INGRESS_STATUS_OFFSET:
      return (idle() ? FX1_ISP_INGRESS_STATUS_IDLE_MASK : 0u) |
             (fe_ == fe_state::in_frame ? FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK : 0u) |
             (direct_ ? FX1_ISP_INGRESS_STATUS_DIRECT_MASK : 0u) | sticky_;
   case FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET: return beats_accepted_;
   case FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET: return frames_admitted_;
   case FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET: return drop_no_credit_;
   case FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET: return drop_no_context_;
   case FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET: return drop_no_stream_;
   case FX1_ISP_INGRESS_DROP_CONFIG_OFFSET: return drop_config_;
   case FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET: return frames_invalid_;
   case FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET: return untracked_;
   case FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET: return beats_discarded_;
   case FX1_ISP_INGRESS_STALL_CYCLES_OFFSET: return stall_cycles_;
   case FX1_ISP_INGRESS_LAST_DROP_OFFSET: return last_drop_;
   case FX1_ISP_INGRESS_LAST_DROP_SEQ_LO_OFFSET: return static_cast<std::uint32_t>(last_drop_seq_);
   case FX1_ISP_INGRESS_LAST_DROP_SEQ_HI_OFFSET: return static_cast<std::uint32_t>(last_drop_seq_ >> 32);
   case FX1_ISP_INGRESS_CTX_TAG_OFFSET: return h ? h->tag : 0u;
   case FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET: return h ? static_cast<std::uint32_t>(h->seq) : 0u;
   case FX1_ISP_INGRESS_CTX_SEQ_HI_OFFSET: return h ? static_cast<std::uint32_t>(h->seq >> 32) : 0u;
   case FX1_ISP_INGRESS_CTX_FRAME_OFFSET:
      return h ? (h->frame_id & FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK) |
                     ((h->epoch << FX1_ISP_INGRESS_CTX_FRAME_EPOCH_SHIFT) & FX1_ISP_INGRESS_CTX_FRAME_EPOCH_MASK)
               : 0u;
   case FX1_ISP_INGRESS_CTX_TS_LO_OFFSET: return h ? static_cast<std::uint32_t>(h->ts) : 0u;
   case FX1_ISP_INGRESS_CTX_TS_HI_OFFSET: return h ? static_cast<std::uint32_t>(h->ts >> 32) : 0u;
   case FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET:
      return h ? (h->code & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK) | (h->cause << FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT)
               : 0u;
   case FX1_ISP_INGRESS_CTX_LINES_OFFSET: return h ? h->lines : 0u;
   case FX1_ISP_INGRESS_CTX_SAMPLES_OFFSET: return h ? h->samples : 0u;
   case FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET: return h ? h->isp_frame : 0u;
   case FX1_ISP_INGRESS_CTX_COUNT_OFFSET: {
      std::uint32_t ready = 0, used = 0;
      for (const int s : admitted_) {
         if (table_[static_cast<std::size_t>(s)].state != slot_state::ready) {
            break;
         }
         ++ready;
      }
      for (const entry &e : table_) {
         used += e.state != slot_state::free;
      }
      return ready | (used << FX1_ISP_INGRESS_CTX_COUNT_USED_SHIFT);
   }
   default: break;
   }
   if (offset >= FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(1u) &&
       offset <= FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(FX1_ISP_INGRESS_NUM_CAUSES)) {
      return cause_count_[(offset - FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(1u)) / 4u];
   }
   return 0;
}

std::uint32_t ingress_engine::csr_read(std::uint32_t offset) { return csr_peek(offset); }

void ingress_engine::csr_write(std::uint32_t offset, std::uint32_t value) {
   switch (offset) {
   case FX1_ISP_INGRESS_CTRL_OFFSET: {
      expected_tid_ = (value & FX1_ISP_INGRESS_CTRL_EXPECTED_TID_MASK) >> FX1_ISP_INGRESS_CTRL_EXPECTED_TID_SHIFT;
      const bool want = (value & FX1_ISP_INGRESS_CTRL_SOURCE_MASK) != 0;
      if (want == direct_) {
         break;
      }
      // Only idle and drained (contract §6): nothing in the ingress, no frame
      // in the ISP, no AXI transaction; direct also needs the IDMA off.
      bool ok = idle() && ctx_.frames_in_flight == 0 && ctx_.ingress_reserved == 0 && ctx_.axi_outstanding == 0 &&
                !(ctx_.reg(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK);
      if (want && (ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_IDMA_EN_MASK)) {
         ok = false;
      }
      if (!ok) {
         sticky_ |= FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK;
         break;
      }
      retire_pending_all();
      fe_ = fe_state::idle;
      tail_ = -1;
      direct_ = want;
      ctx_.direct_source = want;
      if (want && !bound_) {
         sticky_ |= FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK;
      }
      ctx_.kick.notify(sc_core::SC_ZERO_TIME);
      break;
   }
   case FX1_ISP_INGRESS_STATUS_OFFSET: sticky_ &= ~(value & FX1_ISP_INGRESS_STATUS_STICKY_MASK); break;
   case FX1_ISP_INGRESS_CTX_POP_OFFSET:
      if ((value & 1u) && !admitted_.empty() &&
          table_[static_cast<std::size_t>(admitted_.front())].state == slot_state::ready) {
         release(admitted_.front());
         admitted_.pop_front();
      }
      break;
   default: break;
   }
}

}  // namespace cdc::components::fx1_isp
