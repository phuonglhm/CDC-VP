// SPDX-License-Identifier: Apache-2.0
// Synthetic AXI4-Stream source for the VP direct CSI->ISP ingress tests.
//
// Plays a script of operations on a 4 ns grid (the CSI-2 RX sys_clk, 250 MHz):
// beats are offered with TVALID = 1 and held, unchanged, until TREADY; capture
// events and CSI resets are delivered in order with the beats, as the CSI
// observer would. Every executed cycle is recorded (time, valid, ready) so a
// test can check stalls and that no refused beat was consumed.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <vector>

#include <systemc>

#include "fx1_isp/fx1_isp_ingress.h"

namespace fx1_test {

namespace isp = cdc::components::fx1_isp;

struct axis_op {
   enum kind { beat, idle, start, end, stream_end, csi_reset, call } k = beat;
   isp::ingress_beat b{};
   unsigned cycles = 0;  // idle
   std::uint64_t seq = 0;
   std::uint16_t frame_id = 0;
   std::uint64_t ts = 0;
   isp::capture_outcome outcome = isp::capture_outcome::ok;
   std::function<void()> fn;  // call: runs in the source's process at its turn
};

struct axis_source : sc_core::sc_module {
   isp::stream_ingress *sink = nullptr;
   sc_core::sc_time period{4, sc_core::SC_NS};
   std::deque<axis_op> ops;
   sc_core::sc_event done_ev;  // the script ran empty
   // Statistics.
   std::uint64_t offered = 0, accepted = 0, refused = 0;
   std::vector<isp::ingress_beat> accepted_beats;

   SC_HAS_PROCESS(axis_source);
   explicit axis_source(sc_core::sc_module_name n) : sc_core::sc_module(n) { SC_THREAD(run); }

   void push(const axis_op &op) {
      ops.push_back(op);
      kick_.notify(sc_core::SC_ZERO_TIME);
   }
   void beat(const isp::ingress_beat &b) {
      axis_op o;
      o.b = b;
      push(o);
   }
   void idle(unsigned n) {
      axis_op o;
      o.k = axis_op::idle;
      o.cycles = n;
      push(o);
   }
   void start(std::uint64_t seq, std::uint16_t fid, std::uint64_t ts) {
      axis_op o;
      o.k = axis_op::start;
      o.seq = seq;
      o.frame_id = fid;
      o.ts = ts;
      push(o);
   }
   void end(std::uint64_t seq, isp::capture_outcome out = isp::capture_outcome::ok) {
      axis_op o;
      o.k = axis_op::end;
      o.seq = seq;
      o.outcome = out;
      push(o);
   }
   void stream_end(std::uint64_t seq) {
      axis_op o;
      o.k = axis_op::stream_end;
      o.seq = seq;
      push(o);
   }
   void call(std::function<void()> fn) {
      axis_op o;
      o.k = axis_op::call;
      o.fn = std::move(fn);
      push(o);
   }
   bool empty() const { return ops.empty() && !busy_; }
   // Bounded: a script that cannot finish (TREADY held low for good, the
   // ingress stuck) ends the test with a failure instead of a hang.
   void wait_done(const sc_core::sc_time &limit = sc_core::sc_time(50, sc_core::SC_MS)) {
      const sc_core::sc_time end = sc_core::sc_time_stamp() + limit;
      while (!empty()) {
         if (sc_core::sc_time_stamp() >= end) {
            std::fprintf(stderr, "axis_source: CHECK FAILED: script not finished after %s (%zu ops left, %llu refused)\n",
                         limit.to_string().c_str(), ops.size(), static_cast<unsigned long long>(refused));
            std::fflush(stderr);
            std::_Exit(1);
         }
         sc_core::wait(sc_core::sc_time(10, sc_core::SC_US), done_ev);
      }
   }

   // RAW16 beats of one line: 4 samples per beat, the last one TKEEP = 2 bytes
   // per sample; SOF on the first beat of the frame.
   static std::vector<isp::ingress_beat> line_beats(const std::uint16_t *px, std::uint32_t w, bool sof,
                                                    std::uint64_t seq, std::uint16_t fid) {
      std::vector<isp::ingress_beat> v;
      for (std::uint32_t x = 0; x < w; x += 4) {
         isp::ingress_beat b;
         const std::uint32_t n = w - x < 4 ? w - x : 4;
         for (std::uint32_t i = 0; i < n; ++i) {
            b.tdata |= std::uint64_t{px[x + i]} << (16u * i);
         }
         b.tkeep = static_cast<std::uint8_t>((1u << (2u * n)) - 1u);
         b.tlast = x + 4 >= w;
         b.tuser = (sof && x == 0) ? 1u : 0u;
         b.capture_seq = seq;
         b.frame_id = fid;
         v.push_back(b);
      }
      return v;
   }
   // A whole capture: start, the lines with `hblank` idle cycles after each,
   // then the end (outcome) and the stream boundary, unless `with_end` is
   // false (the test sends them itself).
   void frame(const std::vector<std::uint16_t> &px, std::uint32_t w, std::uint32_t h, std::uint64_t seq,
              std::uint16_t fid, unsigned hblank = 8, bool with_end = true,
              isp::capture_outcome out = isp::capture_outcome::ok) {
      start(seq, fid, 1000u * seq);
      for (std::uint32_t y = 0; y < h; ++y) {
         for (const isp::ingress_beat &b : line_beats(px.data() + std::size_t{y} * w, w, y == 0, seq, fid)) {
            beat(b);
         }
         idle(hblank);
      }
      if (with_end) {
         end(seq, out);
         stream_end(seq);
      }
   }

private:
   sc_core::sc_event kick_;
   bool busy_ = false;

   void run() {
      for (;;) {
         while (ops.empty()) {
            busy_ = false;
            done_ev.notify(sc_core::SC_ZERO_TIME);
            sc_core::wait(kick_);
         }
         busy_ = true;
         axis_op op = ops.front();
         ops.pop_front();
         switch (op.k) {
         case axis_op::start: sink->capture_start(op.seq, op.frame_id, op.ts, 0); break;
         case axis_op::end: sink->capture_end(op.seq, op.outcome); break;
         case axis_op::stream_end: sink->capture_stream_end(op.seq); break;
         case axis_op::csi_reset: sink->source_reset(0); break;
         case axis_op::call: op.fn(); break;
         case axis_op::idle:
            for (unsigned i = 0; i < op.cycles; ++i) {
               sink->axis_cycle(false, isp::ingress_beat{});
               sc_core::wait(period);
            }
            break;
         case axis_op::beat: {
            for (;;) {
               ++offered;
               const bool ready = sink->axis_cycle(true, op.b);
               sc_core::wait(period);
               if (ready) {
                  ++accepted;
                  accepted_beats.push_back(op.b);
                  break;
               }
               ++refused;  // offered again, unchanged, on the next cycle
            }
            break;
         }
         }
      }
   }
};

}  // namespace fx1_test
