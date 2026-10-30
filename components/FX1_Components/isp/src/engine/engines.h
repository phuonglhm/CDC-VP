// SPDX-License-Identifier: Apache-2.0
// Internal SystemC engines of the FX1 ISP model (M2): input DMA, the M2
// pipeline stub and output DMA, connected by line FIFOs.
//
// Every engine runs as an SC_THREAD of fx1_isp_tlm. Reset handling is by
// epoch: each external or soft reset increments `engine_context::epoch` and
// wakes every waiter; an engine that observes a new epoch after any wait
// throws `reset_abort`, unwinds to its idle loop and performs no further
// register or memory side effect for the old frame.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <vector>

#include <systemc>
#include <tlm>

#include "control/control_unit.h"
#include "dma/dma_rules.h"
#include "pipeline/isp_pipeline.h"
#include "fx1_isp/fx1_isp_tlm.h"

namespace cdc::components::fx1_isp {

struct reset_abort {};

class line_fifo;

struct engine_context {
   engine_context(control_unit &c, const fx1_isp_params &p) : ctl(c), params(p) {}

   control_unit &ctl;
   const fx1_isp_params &params;
   std::uint64_t epoch = 0;
   bool in_reset = false;
   sc_core::sc_event kick;     // a register write or a reset release
   sc_core::sc_event reset_ev; // epoch changed
   std::function<void()> state_changed;  // re-evaluate o_irq

   // Rotation pointers (sequencing state, reset by i_rst_n and soft reset).
   unsigned idma_head = 0;
   std::array<unsigned, num_channels> odma_head{};  // per output channel
   unsigned frames_in_flight = 0;  // accepted SOFs not yet retired/aborted
   // A frame retires when every consumer is done with it (MAS v1.07 §9.5.1,
   // §9.7): the pipeline and the ODMA of every channel the frame enabled. With
   // a channel crop an output can complete before the shared IQ has consumed
   // the rows below the crop. `ok`: every output published DONE; then the
   // retirement raises COMMON_STATUS.FRAME_DONE (DEC-17, V107-D57).
   struct frame_consumers {
      unsigned pending = 0;
      bool ok = true;
   };
   std::map<std::uint64_t, frame_consumers> consumers;
   unsigned odma_busy = 0;         // output channels writing a frame (DMA_STAT.ODMA_BUSY)
   unsigned odma_overflowing = 0;  // output channels stalled for a free buffer (DMA_ERR.ODMA_OVERFLOW)
   unsigned axi_outstanding = 0;   // transactions accepted and not yet responded (IDLE §9.9.3, drain §9.9.8)
   // i_rst_n generation: a response to a transaction accepted before a hard
   // reset completes its lifetime but has no effect (review V107-7..10 R9).
   std::uint64_t hard_reset_gen = 0;
   sc_core::sc_event retired;      // a frame retired: admission may resume
   std::vector<line_fifo *> fifos;  // cleared on every reset

   std::uint64_t cycle() const {
      return static_cast<std::uint64_t>(sc_core::sc_time_stamp() / params.core_period);
   }
   // The first core-clock edge at or after the current time.
   std::uint64_t edge_at_or_after_now() const {
      return static_cast<std::uint64_t>(std::ceil(sc_core::sc_time_stamp() / params.core_period));
   }
   void axi_response_error(bool read);
   void check(std::uint64_t my_epoch) const {
      if (epoch != my_epoch) {
         throw reset_abort{};
      }
   }
   // Every wait is also woken by a reset, and the epoch is checked before and
   // after it, so no engine sleeps through a reset (review finding M2-R2).
   void wait_event(const sc_core::sc_event &e, std::uint64_t my_epoch) {
      check(my_epoch);
      sc_core::wait(e | reset_ev);
      check(my_epoch);
   }
   void wait_events(const sc_core::sc_event &a, const sc_core::sc_event &b, std::uint64_t my_epoch) {
      check(my_epoch);
      sc_core::wait(a | b | reset_ev);
      check(my_epoch);
   }
   void wait_time(const sc_core::sc_time &t, std::uint64_t my_epoch) {
      check(my_epoch);
      if (t > sc_core::SC_ZERO_TIME) {
         sc_core::wait(t, reset_ev);
      }
      check(my_epoch);
   }
   // Blocks while i_rst_n is asserted or a soft-reset window is open.
   void wait_out_of_reset(std::uint64_t my_epoch);
   // Until the next core-clock edge.
   void wait_next_edge(std::uint64_t my_epoch) {
      wait_time(params.core_period * static_cast<double>(cycle() + 1) - sc_core::sc_time_stamp(), my_epoch);
   }

   // Register helpers that also refresh the interrupt output.
   std::uint32_t reg(std::uint32_t offset) const { return ctl.regs().peek(offset); }
   // A request register (W1S) as its consumer samples it on this edge: a
   // software set of this edge reaches the consumer on the next one, so the
   // consumer never takes a request on the edge that sets it (MAS Table 8-180).
   std::uint32_t request(std::uint32_t offset) const { return ctl.regs().registered(offset, cycle()); }
   void set(std::uint32_t offset, std::uint32_t mask);
   void clear(std::uint32_t offset, std::uint32_t mask);
   void write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value);
   void hold(std::uint32_t offset, std::uint32_t mask, bool level);

   void accept_frame();
   void retire_frame(bool completed = false);
   // Frame f has `outputs` output consumers besides the pipeline.
   void frame_begin(std::uint64_t f, unsigned outputs) { consumers[f] = frame_consumers{outputs + 1u, outputs > 0}; }
   // One consumer of frame f is done; `ok`: it completed (an output: DONE published).
   void consumer_done(std::uint64_t f, bool ok);
   void reset_engines();  // called on i_rst_n assertion and on soft reset
};

// One line between two engines. A marker line (abort) carries no data.
struct line {
   std::uint64_t frame = 0;  // frame sequence number of the producer
   bool sof = false;
   bool eof = false;
   bool abort = false;       // frame abandoned upstream; ends the frame
   std::uint32_t width = 0;  // frame geometry, valid on every line
   std::uint32_t height = 0;
   std::uint32_t tag = 0;    // input buffer tag and capture timestamp of the frame,
   std::uint32_t ts = 0;     // valid on every line (MAS Table 160)
   std::uint32_t ingress = 0;  // ingress frame ID and profile ID of the frame (MAS §9.9.9)
   std::uint32_t profile = 0;
   std::vector<std::uint16_t> samples;  // input lines
   std::vector<std::uint8_t> bytes;     // output lines
};

class line_fifo {
public:
   line_fifo(engine_context &ctx, std::size_t depth) : ctx_(ctx), depth_(depth) {}

   void push(line l, std::uint64_t my_epoch);
   line pop(std::uint64_t my_epoch);
   const line *front() const { return q_.empty() ? nullptr : &q_.front(); }
   bool empty() const { return q_.empty(); }
   void wait_empty(std::uint64_t my_epoch);
   void clear();
   const sc_core::sc_event &data_event() const { return data_ev_; }

private:
   engine_context &ctx_;
   std::size_t depth_;
   std::deque<line> q_;
   sc_core::sc_event data_ev_;
   sc_core::sc_event space_ev_;
};

// Memory access through one master socket, one TLM transaction per burst.
// Returns false on a non-OK response.
bool memory_access(tlm::tlm_initiator_socket<> &socket, tlm::tlm_command cmd, std::uint64_t addr,
                   std::uint8_t *data, std::uint32_t bytes, engine_context &ctx, std::uint64_t my_epoch);

class idma_engine {
public:
   idma_engine(engine_context &ctx, tlm::tlm_initiator_socket<> &socket, line_fifo &out)
       : ctx_(ctx), socket_(socket), out_(out) {}
   void run();

private:
   void wait_for_buffer(std::uint64_t my_epoch);
   void set_underrun(bool level);
   void frame(std::uint64_t my_epoch);

   engine_context &ctx_;
   tlm::tlm_initiator_socket<> &socket_;
   line_fifo &out_;
   dma::idma_setup setup_{};
   std::uint32_t tag_ = 0;  // IDMA_BUF_TAG/TS of the armed buffer, captured with
   std::uint32_t ts_ = 0;   // the working set (MAS §6.25.4.1.0.3)
   std::uint32_t max_beats_ = 64;
   std::uint64_t frames_ = 0;
};

// Pixel pipeline thread: accepted SOF, configuration snapshot, and either the
// image pipeline (M3) or, for DMA tests only, the M2 geometry stub (DEC-21).
// The output FIFOs of one channel: CH0/CH1 NV12 (luma, chroma), CH2 RGB888
// (packed rows in `y`, no `uv`).
struct channel_fifos {
   line_fifo *y = nullptr;
   line_fifo *uv = nullptr;
};

class pipeline_engine {
public:
   pipeline_engine(engine_context &ctx, line_fifo &in, const std::array<channel_fifos, num_channels> &out)
       : ctx_(ctx), in_(in), out_(out) {}
   void run();

private:
   // Rows a channel has emitted for the frame in progress (abort markers).
   struct channel_progress {
      bool on = false;
      std::uint32_t width = 0, height = 0, y_rows = 0, uv_rows = 0;
   };
   void wait_isp_enabled(std::uint64_t my_epoch);
   void frame(std::uint64_t my_epoch);
   void stub_frame(line &l, std::uint64_t f, std::uint64_t my_epoch);
   void isp_frame(line &l, std::uint64_t f, std::uint64_t my_epoch);
   // The frame is abandoned: each enabled channel that started receives an
   // abort marker on every incomplete plane; the others are done with it.
   void abort_downstream(std::uint64_t f, std::uint64_t my_epoch);
   sc_core::sc_time line_time(std::uint32_t width) const;

   pipe::isp_pipeline pipeline_;

   engine_context &ctx_;
   line_fifo &in_;
   std::array<channel_fifos, num_channels> out_;
   std::array<channel_progress, num_channels> prog_{};
   std::uint64_t frames_ = 0;
};

// Registers of one output channel's DMA (CSR v1.07): CH0 uses the ODMA page,
// CH1 0x3280.., CH2 0x3480.. (one RGB888 plane: `uv_addr0` = `uv_stride` = 0).
struct odma_channel_regs {
   std::uint32_t free, done, count, y_addr0, uv_addr0, y_stride, uv_stride;
   std::uint32_t tag0, ts0, sof_ts0, eof_ts0, frame0, profile0;
};
extern const odma_channel_regs odma_channels[num_channels];

// The output DMA of one channel (MAS v1.07 §6.25.3.8). The three engines share
// the ODMA AXI master; transactions interleave at burst boundaries.
class odma_engine {
public:
   odma_engine(engine_context &ctx, tlm::tlm_initiator_socket<> &socket, unsigned ch, const channel_fifos &in)
       : ctx_(ctx), socket_(socket), ch_(ch), r_(odma_channels[ch]), y_(*in.y), uv_(in.uv) {}
   void run();

private:
   // At the channel's output SOF: claim the head buffer if it is free and its
   // working set is legal (§6.25.3.4, §6.25.3.8); false: the frame waits.
   bool claim(const line &first, std::uint64_t my_epoch);
   void set_overflow(bool level);
   void sink_frame(bool y_done, bool uv_done, std::uint64_t my_epoch);
   void end_frame(bool ok);
   bool write_line(std::uint64_t base, std::uint32_t stride, std::uint32_t row, const line &l,
                   std::uint64_t my_epoch);

   engine_context &ctx_;
   tlm::tlm_initiator_socket<> &socket_;
   const unsigned ch_;
   const odma_channel_regs &r_;
   line_fifo &y_;
   line_fifo *uv_;  // nullptr: CH2
   dma::odma_setup setup_{};
   std::uint32_t max_beats_ = 64;
   bool busy_ = false;
   bool overflow_ = false;
   std::uint64_t frame_ = 0;  // pipeline frame number of the frame being written
   bool retry_next_edge_ = false;  // the head buffer was freed on this edge
};

}  // namespace cdc::components::fx1_isp
