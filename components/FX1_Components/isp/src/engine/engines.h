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

#include <cstdint>
#include <deque>
#include <functional>
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
   unsigned odma_head = 0;
   unsigned frames_in_flight = 0;  // accepted SOFs not yet retired/aborted
   std::vector<line_fifo *> fifos;  // cleared on every reset

   std::uint64_t cycle() const {
      return static_cast<std::uint64_t>(sc_core::sc_time_stamp() / params.core_period);
   }
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

   // Register helpers that also refresh the interrupt output.
   std::uint32_t reg(std::uint32_t offset) const { return ctl.regs().peek(offset); }
   void set(std::uint32_t offset, std::uint32_t mask);
   void clear(std::uint32_t offset, std::uint32_t mask);
   void write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value);
   void hold(std::uint32_t offset, std::uint32_t mask, bool level);

   void accept_frame();
   void retire_frame();
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
   std::uint32_t max_beats_ = 64;
   std::uint64_t frames_ = 0;
};

// Pixel pipeline thread: accepted SOF, configuration snapshot, and either the
// image pipeline (M3) or, for DMA tests only, the M2 geometry stub (DEC-21).
class pipeline_engine {
public:
   pipeline_engine(engine_context &ctx, line_fifo &in, line_fifo &y_out, line_fifo &uv_out)
       : ctx_(ctx), in_(in), y_(y_out), uv_(uv_out) {}
   void run();

private:
   void wait_isp_enabled(std::uint64_t my_epoch);
   void frame(std::uint64_t my_epoch);
   void stub_frame(line &l, std::uint64_t f, std::uint64_t my_epoch);
   void isp_frame(line &l, std::uint64_t f, std::uint64_t my_epoch);
   void abort_downstream(std::uint64_t f, bool output_started, std::uint64_t my_epoch,
                         bool y_complete, bool uv_complete);
   sc_core::sc_time line_time(std::uint32_t width) const;

   pipe::isp_pipeline pipeline_;

   engine_context &ctx_;
   line_fifo &in_;
   line_fifo &y_;
   line_fifo &uv_;
   std::uint64_t frames_ = 0;
};

class odma_engine {
public:
   odma_engine(engine_context &ctx, tlm::tlm_initiator_socket<> &socket, line_fifo &y_in, line_fifo &uv_in)
       : ctx_(ctx), socket_(socket), y_(y_in), uv_(uv_in) {}
   void run();

private:
   bool arm(std::uint64_t my_epoch);
   void sink_frame(bool y_done, bool uv_done, std::uint64_t my_epoch);
   void end_frame();
   bool write_line(std::uint64_t base, std::uint32_t stride, std::uint32_t row, const line &l,
                   std::uint64_t my_epoch);

   engine_context &ctx_;
   tlm::tlm_initiator_socket<> &socket_;
   line_fifo &y_;
   line_fifo &uv_;
   dma::odma_setup setup_{};
   std::uint32_t max_beats_ = 64;
   bool armed_ = false;
   bool overflow_flagged_ = false;
};

}  // namespace cdc::components::fx1_isp
