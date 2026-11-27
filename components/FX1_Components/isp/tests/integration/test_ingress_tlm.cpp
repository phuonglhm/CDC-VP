// SPDX-License-Identifier: Apache-2.0
// VP direct CSI->ISP stream ingress (D1a): handshake, geometry, validation,
// admission and source selection (contract DIRECT_CSI_ISP_CONTRACT.md §3-§9).
//
// A synthetic AXI4-Stream source on the CSI-2 RX sys_clk grid drives the
// ingress; the ISP runs with the M2 datapath stub (DEC-21: luma = cropped
// sample >> 4 after the input normalization, chroma = 128), so every expected
// output byte is computed here from that definition, independently of the
// ingress. Each case checks the beats (TREADY, refused beats offered again),
// the ISP_INGRESS counters and context entry, and the output in RAM.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "axis_source.h"
#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_ingress.h"
#include "fx1_isp/fx1_isp_ingress_regs.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "test_memory.h"

#define FX1_CHECK_CTX(cond, msg) FX1_CHECK_EQ_CTX((cond) ? 1 : 0, 1, msg)

using namespace sc_core;
using cdc::components::fx1_isp::control_unit;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
using cdc::components::fx1_isp::geometry;
using cdc::components::fx1_isp::ingress_beat;
using cdc::components::fx1_isp::capture_outcome;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 8u << 20;
constexpr std::uint8_t guard = 0xA5;
std::uint64_t in_buf(unsigned k) { return mem_base + 0x10'0000ull * k; }
std::uint64_t out_y(unsigned k) { return mem_base + 0x40'0000ull + 0x4'0000ull * k; }
std::uint64_t out_uv(unsigned k) { return mem_base + 0x60'0000ull + 0x4'0000ull * k; }

struct cfg {
   std::uint32_t w = 66, h = 32;  // 66 = 16 full beats + a 2-sample tail (TKEEP 0x0F)
   std::uint32_t pattern = FX1_ISP_COMMON_BAYER_PATTERN_RGGB;
   std::uint32_t depth = 0;  // COMMON_INPUT_FORMAT.DEPTH: 0 RAW12, 1 RAW10
   std::uint32_t y_stride = 96, uv_stride = 96;
};

std::uint16_t sample(unsigned k, std::uint32_t x, std::uint32_t y, std::uint32_t depth) {
   return static_cast<std::uint16_t>((x * 7u + y * 13u + k * 29u + 3u) & (depth ? 0x3FFu : 0xFFFu));
}

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   tlm_utils::simple_initiator_socket<bench> ing;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fx1_test::axis_source *src = nullptr;
   fx1_isp_tlm *unbound = nullptr;  // second ISP, no stream bound
   tlm_utils::simple_initiator_socket<bench> ing2;
   tlm_utils::simple_initiator_socket<bench> csr2;
   std::uint64_t seq = 0;
   unsigned cases = 0;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), ing("ing"), rst_n("rst_n"), ing2("ing2"), csr2("csr2") {
      SC_THREAD(run);
   }

   void xfer(tlm_utils::simple_initiator_socket<bench> &s, tlm::tlm_command cmd, std::uint32_t a, std::uint32_t &v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(a);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      s->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::memcpy(&v, b, 4);
   }
   void w32(std::uint32_t a, std::uint32_t v) { xfer(socket, tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t r32(std::uint32_t a) {
      std::uint32_t v = 0;
      xfer(socket, tlm::TLM_READ_COMMAND, a, v);
      return v;
   }
   void iw(std::uint32_t a, std::uint32_t v) { xfer(ing, tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t ir(std::uint32_t a) {
      std::uint32_t v = 0;
      xfer(ing, tlm::TLM_READ_COMMAND, a, v);
      return v;
   }
   bool wait_until(const std::function<bool()> &cond, sc_time timeout = sc_time(20, SC_MS)) {
      const sc_time end = sc_time_stamp() + timeout;
      while (!cond()) {
         if (sc_time_stamp() >= end) {
            return false;
         }
         wait(1, SC_US);
      }
      return true;
   }

   void reset() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      mem->log.clear();
      mem->latency = SC_ZERO_TIME;
      mem->blocking = false;
   }
   void program(const cfg &c) {
      w32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, c.w);
      w32(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, c.h);
      w32(FX1_ISP_COMMON_BAYER_OFFSET, c.pattern);
      w32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, c.depth);
      w32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, c.y_stride);
      w32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, c.uv_stride);
      w32(FX1_ISP_IDMA_STRIDE_OFFSET, 256);
      for (unsigned k = 0; k < 4; ++k) {
         w32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(in_buf(k)));
         w32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(out_y(k)));
         w32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(out_uv(k)));
         mem->fill(out_y(k), 0x4'0000, guard);
         mem->fill(out_uv(k), 0x4'0000, guard);
      }
      w32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);  // Resizer derivation
   }
   void direct(const cfg &c) {
      reset();
      program(c);
      w32(FX1_ISP_DMA_CTRL_OFFSET, (63u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & (FX1_ISP_INGRESS_STATUS_DIRECT_MASK | FX1_ISP_INGRESS_STATUS_STICKY_MASK),
                   FX1_ISP_INGRESS_STATUS_DIRECT_MASK);
   }
   void arm_outputs() {
      w32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
      w32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);
   }
   std::vector<std::uint16_t> pixels(unsigned k, const cfg &c) const {
      std::vector<std::uint16_t> p(std::size_t{c.w} * c.h);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (std::uint32_t x = 0; x < c.w; ++x) {
            p[std::size_t{y} * c.w + x] = sample(k, x, y, c.depth);
         }
      }
      return p;
   }
   // Wrong bytes of buffer `buf` against the stub definition for frame k.
   std::size_t check_output(unsigned buf, unsigned k, const cfg &c) {
      const geometry a = control_unit::derive_active(c.w, c.h, c.pattern);
      const std::uint32_t sh = c.pattern & 1u, sv = (c.pattern >> 1) & 1u;
      std::size_t bad = 0;
      for (std::uint32_t oy = 0; oy < a.height; ++oy) {
         const std::uint8_t *row = mem->at(out_y(buf) + std::uint64_t{oy} * c.y_stride);
         for (std::uint32_t ox = 0; ox < c.y_stride; ++ox) {
            std::uint8_t want = guard;
            if (ox < a.width) {
               const std::uint16_t s = sample(k, ox + sh, oy + sv, c.depth);
               const std::uint32_t n = c.depth ? (s & 0x3FFu) << 2 : (s & 0xFFFu);
               want = static_cast<std::uint8_t>(n >> 4);
            }
            bad += row[ox] != want;
         }
      }
      for (std::uint32_t j = 0; j < a.height / 2u; ++j) {
         const std::uint8_t *row = mem->at(out_uv(buf) + std::uint64_t{j} * c.uv_stride);
         for (std::uint32_t i = 0; i < c.uv_stride; ++i) {
            bad += row[i] != (i < a.width ? 128u : guard);
         }
      }
      return bad;
   }
   std::uint32_t ctx_ready() { return ir(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK; }
   std::uint32_t ctx_used() {
      return (ir(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_USED_MASK) >>
             FX1_ISP_INGRESS_CTX_COUNT_USED_SHIFT;
   }
   // Waits for the head READY entry; returns its OUTCOME (code | cause << 8) and pops it.
   std::uint32_t pop_outcome(const std::string &what, std::uint64_t want_seq) {
      FX1_CHECK_EQ_CTX(wait_until([&] { return ctx_ready() > 0; }) ? 1 : 0, 1, what + ": context READY");
      const std::uint32_t o = ir(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET);
      FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET), static_cast<std::uint32_t>(want_seq), what + ": seq");
      iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
      return o;
   }
   void idle_isp(const std::string &what) {
      FX1_CHECK_CTX(wait_until([&] {
                       return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0 &&
                              (ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_IDLE_MASK);
                    }),
                    what + ": ISP and ingress idle");
   }
   // One good frame k: OK, output exact in buffer `buf`.
   void good_frame(unsigned k, const cfg &c, unsigned buf, const std::string &what, unsigned hblank = 8) {
      arm_outputs();
      const std::uint32_t done = r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
      ++seq;
      src->frame(pixels(k, c), c.w, c.h, seq, static_cast<std::uint16_t>(seq), hblank);
      src->wait_done();
      FX1_CHECK_EQ_CTX(pop_outcome(what, seq), FX1_ISP_INGRESS_OUTCOME_OK, what + ": OK");
      FX1_CHECK_EQ_CTX(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == done + 1; }) ? 1 : 0, 1, what + ": DONE");
      FX1_CHECK_EQ_CTX(check_output(buf, k, c), 0, what + ": output");
   }

   // ---- cases ----------------------------------------------------------------
   void identity() {
      reset();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_ID_OFFSET), FX1_ISP_INGRESS_ID_VALUE);
      const std::uint32_t cap = ir(FX1_ISP_INGRESS_CAPABILITY_OFFSET);
      FX1_CHECK_EQ(cap & FX1_ISP_INGRESS_CAPABILITY_BOUND_MASK, 1);
      FX1_CHECK_EQ((cap & FX1_ISP_INGRESS_CAPABILITY_FIFO_BEATS_MASK) >> FX1_ISP_INGRESS_CAPABILITY_FIFO_BEATS_SHIFT, 16);
      FX1_CHECK_EQ((cap & FX1_ISP_INGRESS_CAPABILITY_CTX_DEPTH_MASK) >> FX1_ISP_INGRESS_CAPABILITY_CTX_DEPTH_SHIFT, 8);
      FX1_CHECK_EQ((cap & FX1_ISP_INGRESS_CAPABILITY_MAX_LINE_MASK) >> FX1_ISP_INGRESS_CAPABILITY_MAX_LINE_SHIFT, 3842);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET), 0);  // memory at reset
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET), FX1_ISP_INGRESS_STATUS_IDLE_MASK);
      std::uint32_t v = 0;  // the unbound second ISP reports BOUND = 0, and NOT_BOUND when direct is selected
      xfer(ing2, tlm::TLM_READ_COMMAND, FX1_ISP_INGRESS_CAPABILITY_OFFSET, v);
      FX1_CHECK_EQ(v & FX1_ISP_INGRESS_CAPABILITY_BOUND_MASK, 0);
      v = FX1_ISP_INGRESS_CTRL_SOURCE_MASK;
      xfer(ing2, tlm::TLM_WRITE_COMMAND, FX1_ISP_INGRESS_CTRL_OFFSET, v);
      xfer(ing2, tlm::TLM_READ_COMMAND, FX1_ISP_INGRESS_STATUS_OFFSET, v);
      FX1_CHECK_EQ(v & FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK, FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK);
      ++cases;
   }

   // P21: memory mode with the stream bound: TREADY 1, every beat discarded;
   // an IDMA frame completes as without the ingress.
   void memory_mode() {
      const cfg c;
      reset();
      program(c);
      w32(FX1_ISP_DMA_CTRL_OFFSET, (63u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_IDMA_EN_MASK |
                                       FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (std::uint32_t x = 0; x < c.w; ++x) {
            mem->put16(in_buf(0) + std::uint64_t{y} * 256u + 2u * x, sample(5, x, y, 0));
         }
      }
      arm_outputs();
      const std::uint64_t refused = src->refused, accepted = src->accepted;
      src->frame(pixels(9, c), c.w, c.h, ++seq, 1);
      w32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      const std::uint64_t beats = src->accepted - accepted;
      FX1_CHECK_EQ(beats, std::uint64_t{17} * c.h);
      FX1_CHECK_EQ(src->refused, refused);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET), beats);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET), 0);
      FX1_CHECK_EQ(ctx_used(), 0);
      FX1_CHECK_EQ(check_output(0, 5, c), 0);  // the IDMA frame, not the stream
      FX1_CHECK(mem->count(tlm::TLM_READ_COMMAND, true) > 0);
      ++cases;
   }

   // Source switch rules (contract §6).
   void switching() {
      const cfg c;
      reset();
      program(c);
      w32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);  // IDMA_EN = 1: refused
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET) & FX1_ISP_INGRESS_CTRL_SOURCE_MASK, 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK,
                   FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK);
      iw(FX1_ISP_INGRESS_STATUS_OFFSET, FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK);  // W1C
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK, 0);
      w32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET) & FX1_ISP_INGRESS_CTRL_SOURCE_MASK, 1);
      // In direct, IDMA_EN = 1 and a VALID buffer: the IDMA arms nothing and reports nothing.
      // (The UNDERRUN raised above, in memory mode with IDMA_EN = 1 and no buffer, is cleared first.)
      w32(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0xFFFFFFFFu);
      FX1_CHECK_EQ(r32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT, 0);
      w32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      w32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1);
      wait(50, SC_US);
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0);
      FX1_CHECK_EQ(r32(FX1_ISP_DMA_IRQ_STAT_OFFSET) &
                       (FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_START_BIT | FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT),
                   0);
      // Mid-frame (in the ingress and in the ISP): switching back is refused.
      w32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      arm_outputs();
      src->frame(pixels(1, c), c.w, c.h, ++seq, 1);
      wait(2, SC_US);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK,
                   FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET) & FX1_ISP_INGRESS_CTRL_SOURCE_MASK, 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK,
                   FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("switch refused mid-frame", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      idle_isp("switch refused mid-frame");
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, 0);  // idle and drained: accepted
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET) & FX1_ISP_INGRESS_CTRL_SOURCE_MASK, 0);
      ++cases;
   }

   // Nominal RAW12 / RAW10 with a 2-sample tail; no stall (long blanking) and
   // with stalls (the pipeline is slower than the stream inside a line).
   void nominal() {
      for (std::uint32_t depth : {0u, 1u}) {
         cfg c;
         c.depth = depth;
         direct(c);
         const std::uint64_t r0 = src->refused, a0 = src->accepted;
         good_frame(3, c, 0, depth ? "RAW10 no stall" : "RAW12 no stall", 200);
         FX1_CHECK_EQ(src->refused, r0);
         FX1_CHECK_EQ(src->accepted - a0, std::uint64_t{17} * c.h);
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET), 17u * c.h);
         const std::uint64_t r1 = src->refused;
         good_frame(4, c, 1, depth ? "RAW10 stalled" : "RAW12 stalled", 0);
         const std::uint64_t stalls = src->refused - r1;
         FX1_CHECK(stalls > 0);
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STALL_CYCLES_OFFSET), stalls);
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET), 2u * 17u * c.h);  // none lost or taken twice
         FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0);
         ++cases;
      }
   }

   // Back-pressure from a slow ODMA: TREADY low for long stretches, payload exact.
   void odma_backpressure() {
      const cfg c;
      direct(c);
      mem->latency = sc_time(2, SC_US);
      mem->blocking = true;
      const std::uint64_t r0 = src->refused;
      good_frame(6, c, 0, "slow ODMA", 8);
      FX1_CHECK(src->refused - r0 > 1000);
      mem->latency = SC_ZERO_TIME;
      mem->blocking = false;
      ++cases;
   }

   // Not admitted at the SOF (contract §7, §8): TREADY stays 1, beats
   // discarded, counters and LAST_DROP only, no readable entry.
   void not_admitted() {
      const cfg c;
      direct(c);  // after reset no output buffer is FREE: NO_CREDIT
      std::uint64_t r0 = src->refused;
      src->frame(pixels(1, c), c.w, c.h, ++seq, 0x123);
      src->wait_done();
      FX1_CHECK_EQ(src->refused, r0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_LAST_DROP_OFFSET),
                   FX1_ISP_INGRESS_DROP_REASON_NO_CREDIT | (0x123u << FX1_ISP_INGRESS_LAST_DROP_FRAME_ID_SHIFT));
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_LAST_DROP_SEQ_LO_OFFSET), static_cast<std::uint32_t>(seq));
      FX1_CHECK_EQ(ctx_used(), 0);  // released once its end arrived
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET), 0);
      // ODMA disabled: also NO_CREDIT.
      w32(FX1_ISP_DMA_CTRL_OFFSET, 0);
      arm_outputs();
      src->frame(pixels(1, c), c.w, c.h, ++seq, 2);
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), 2);
      w32(FX1_ISP_DMA_CTRL_OFFSET, (63u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      // No capture_start for the SOF's capture: NO_CONTEXT.
      for (std::uint32_t y = 0; y < c.h; ++y) {
         const std::vector<std::uint16_t> p = pixels(1, c);
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, 999, 9)) {
            src->beat(b);
         }
      }
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET), 1);
      // Illegal input geometry at the SOF (odd width): CONFIG.
      w32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 65);
      src->frame(pixels(1, c), c.w, c.h, ++seq, 3);
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_CONFIG_OFFSET), 1);
      w32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, c.w);
      wait(dut->params().core_period * 80.0);
      FX1_CHECK_EQ(src->refused, r0);  // never back-pressured while dropping
      FX1_CHECK_EQ(ctx_used(), 0);
      good_frame(2, c, 0, "after the drops");
      ++cases;
   }

   // A capture with its own beat script; `mutate` alters the beats of line y.
   void scripted(unsigned k, const cfg &c, const std::function<void(std::uint32_t, std::vector<ingress_beat> &)> &mutate,
                 bool end_first = false, bool end_last = true) {
      ++seq;
      const std::uint16_t fid = static_cast<std::uint16_t>(seq);
      src->start(seq, fid, seq);
      if (end_first) {
         src->end(seq);
      }
      const std::vector<std::uint16_t> p = pixels(k, c);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         std::vector<ingress_beat> v = fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, seq, fid);
         mutate(y, v);
         for (const ingress_beat &b : v) {
            src->beat(b);
         }
         src->idle(8);
      }
      if (end_last && !end_first) {
         src->end(seq);
      }
      if (end_last || end_first) {
         src->stream_end(seq);  // the boundary after the last beat
      }
   }
   void invalid_case(const std::string &what, unsigned cause,
                     const std::function<void(std::uint32_t, std::vector<ingress_beat> &)> &mutate,
                     const cfg &c = cfg{}) {
      direct(c);
      arm_outputs();
      scripted(7, c, mutate);
      src->wait_done();
      const std::uint32_t o = pop_outcome(what, seq);
      FX1_CHECK_EQ_CTX(o & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK, FX1_ISP_INGRESS_OUTCOME_INVALID, what + ": INVALID");
      FX1_CHECK_EQ_CTX((o & FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_MASK) >> FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT, cause,
                       what + ": cause");
      FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(cause)), 1, what + ": cause counter");
      FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 1, what + ": FRAMES_INVALID");
      idle_isp(what);
      // The next frame is processed normally.
      const std::uint32_t done = r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
      good_frame(8, c, done % 4u, what + ", next frame");
      ++cases;
   }

   void invalid_frames() {
      invalid_case("short line", FX1_ISP_INGRESS_CAUSE_LINE_LENGTH, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 5) {
            v.erase(v.begin() + 3);  // 62 samples
         }
      });
      invalid_case("long line", FX1_ISP_INGRESS_CAUSE_LINE_LENGTH, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 5) {
            ingress_beat extra = v.back();
            v.back().tlast = false;
            v.back().tkeep = 0xFF;
            v.push_back(extra);  // 68 samples
         }
      });
      invalid_case("missing lines", FX1_ISP_INGRESS_CAUSE_MISSING_LINES, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 20) {
            v.front().tuser |= 1u;  // an SOF inside the frame (same capture: not admitted again)
         }
      });
      invalid_case("odd TKEEP", FX1_ISP_INGRESS_CAUSE_KEEP, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 2) {
            v[4].tkeep = 0x7F;
         }
      });
      invalid_case("TKEEP not a prefix", FX1_ISP_INGRESS_CAUSE_KEEP, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 2) {
            v.back().tkeep = 0xF0;
         }
      });
      cfg c10;
      c10.depth = 1;
      invalid_case("sample above RAW10", FX1_ISP_INGRESS_CAUSE_DEPTH, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 9) {
            v[2].tdata |= std::uint64_t{0x400} << 16;
         }
      }, c10);
      invalid_case("TUSER[1] on a data beat", FX1_ISP_INGRESS_CAUSE_TUSER_ERR,
                   [](std::uint32_t y, std::vector<ingress_beat> &v) {
                      if (y == 3) {
                         v.back().tuser |= 2u;
                      }
                   });
      invalid_case("abort beat mid-frame", FX1_ISP_INGRESS_CAUSE_ABORT, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 11) {
            v.resize(5);
            ingress_beat a;
            a.tlast = true;
            a.tuser = 2;
            v.push_back(a);  // closes the open line; the rest of the frame is discarded
         }
      });
      invalid_case("beat of another capture", FX1_ISP_INGRESS_CAUSE_TAG_MISMATCH,
                   [](std::uint32_t y, std::vector<ingress_beat> &v) {
                      if (y == 6) {
                         v[1].capture_seq += 1;
                      }
                   });
      invalid_case("abort on the first beat", FX1_ISP_INGRESS_CAUSE_ABORT, [](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 0) {
            ingress_beat a;
            a.tlast = true;
            a.tuser = 2;
            v.insert(v.begin() + 1, a);  // nothing of the frame reached the ISP
         }
      });
   }

   // After the last line, until the next SOF (contract §7, R3): a late abort
   // or extra data invalidates the frame unless its capture already ended OK.
   void late_events() {
      const cfg c;
      // Late abort, the CSI outcome not yet known: INVALID (ABORT).
      direct(c);
      arm_outputs();
      scripted(2, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, false, false);
      ingress_beat a;
      a.tlast = true;
      a.tuser = 2;
      src->beat(a);
      src->end(seq, capture_outcome::dropped);
      src->stream_end(seq);
      src->wait_done();
      std::uint32_t o = pop_outcome("late abort", seq);
      FX1_CHECK_EQ(o & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK, FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED);  // CSI first
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(FX1_ISP_INGRESS_CAUSE_ABORT)), 1);
      // The abort after capture_end(OK): belongs to a later capture; the frame stays OK.
      idle_isp("late abort");
      arm_outputs();
      scripted(3, c, [](std::uint32_t, std::vector<ingress_beat> &) {});
      src->beat(a);
      src->wait_done();
      o = pop_outcome("abort after OK", seq);
      FX1_CHECK_EQ(o, FX1_ISP_INGRESS_OUTCOME_OK);
      // Extra data after the last line, before the end: INVALID (EXTRA_LINES).
      idle_isp("abort after OK");
      arm_outputs();
      scripted(4, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, false, false);
      const std::vector<std::uint16_t> p = pixels(4, c);
      for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data(), c.w, false, seq, static_cast<std::uint16_t>(seq))) {
         src->beat(b);
      }
      src->end(seq);
      src->stream_end(seq);
      src->wait_done();
      o = pop_outcome("extra line", seq);
      FX1_CHECK_EQ(o, FX1_ISP_INGRESS_OUTCOME_INVALID | (FX1_ISP_INGRESS_CAUSE_EXTRA_LINES << FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT));
      ++cases;
   }

   // Association by capture sequence (contract §9.1, F1): the end before the
   // SOF; a capture that never sent an SOF is retired by the next SOF.
   void association() {
      const cfg c;
      direct(c);
      arm_outputs();
      scripted(5, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, true);  // end(OK) before any beat
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("end before SOF", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(check_output(0, 5, c), 0);
      // Capture A: start and end, no beat (dropped before its first beat).
      // Capture B: a frame. A is retired NO_STREAM at B's SOF, B is OK.
      idle_isp("end before SOF");
      const std::uint64_t a = ++seq;
      src->start(a, 1, 1);
      src->end(a, capture_outcome::dropped);
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 0);
      good_frame(6, c, 1, "after a capture without SOF");
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 1);
      FX1_CHECK_EQ(ctx_used(), 0);
      // With its stream boundary, a capture without SOF is retired at once.
      const std::uint64_t a2 = ++seq;
      src->start(a2, 2, 2);
      src->end(a2, capture_outcome::dropped);
      src->stream_end(a2);
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 2);
      FX1_CHECK_EQ(ctx_used(), 0);
      // Two PENDING captures, the older in the higher slot (a POP freed slot 0
      // in between): the older capture's SOF binds to its own entry, by
      // capture sequence, not to the first PENDING slot.
      idle_isp("after a capture without SOF");
      arm_outputs();
      scripted(7, c, [](std::uint32_t, std::vector<ingress_beat> &) {});  // F: READY, not popped (slot 0)
      src->wait_done();
      FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
      const std::uint64_t c1 = ++seq;
      src->start(c1, 0x11, 11);  // slot 1
      src->call([this] { iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1); });  // frees slot 0
      const std::uint64_t c2 = ++seq;
      src->start(c2, 0x22, 22);  // slot 0
      const std::vector<std::uint16_t> p1 = pixels(1, c), p2 = pixels(2, c);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p1.data() + y * c.w, c.w, y == 0, c1, 0x11)) {
            src->beat(b);
         }
      }
      src->end(c1);
      src->stream_end(c1);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p2.data() + y * c.w, c.w, y == 0, c2, 0x22)) {
            src->beat(b);
         }
      }
      src->end(c2);
      src->stream_end(c2);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("older capture, higher slot", c1), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(pop_outcome("newer capture, lower slot", c2), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 0);
      ++cases;
   }

   // Publish gate (P26): the ISP has finished a frame, its capture has not
   // ended: the entry is not READY. The late end decides the outcome.
   void terminal_gate() {
      const cfg c;
      direct(c);
      arm_outputs();
      scripted(3, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, false, false);
      src->stream_end(seq);  // the stream is over; only the CSI terminal is missing
      src->wait_done();
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; }));
      wait(20, SC_US);
      FX1_CHECK_EQ(ctx_ready(), 0);  // no outcome before the CSI terminal
      src->end(seq, capture_outcome::dropped);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("late CSI end", seq), FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED);
      arm_outputs();
      scripted(4, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, false, false);
      src->stream_end(seq);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; }));
      src->end(seq, capture_outcome::missing_fe);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("missing FE", seq), FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE);
      ++cases;
   }

   // The TID check: beats of another exposure are discarded; EXPECTED_TID selects.
   void tid() {
      const cfg c;
      direct(c);
      arm_outputs();
      scripted(1, c, [](std::uint32_t, std::vector<ingress_beat> &v) {
         for (ingress_beat &b : v) {
            b.tid = 1;
         }
      });
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET), 17u * c.h);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET), 0);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK | (1u << FX1_ISP_INGRESS_CTRL_EXPECTED_TID_SHIFT));
      scripted(2, c, [](std::uint32_t, std::vector<ingress_beat> &v) {
         for (ingress_beat &b : v) {
            b.tid = 1;
         }
      });
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("TID 1 expected", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK_EQ(check_output(0, 2, c), 0);
      ++cases;
   }

   // Output credit look-ahead (contract §8): a frame is admitted only if the
   // buffer it will take is FREE, counting the admitted frames that have not
   // claimed theirs yet. Two captures back to back, one FREE buffer: the
   // second is dropped at its SOF; with two FREE buffers both complete.
   void credit_lookahead() {
      const cfg c;
      for (const std::uint32_t free : {0x1u, 0x3u}) {
         direct(c);
         w32(FX1_ISP_ODMA_BUF_FREE_OFFSET, free);
         const std::uint64_t first = seq + 1;
         ++seq;
         src->frame(pixels(1, c), c.w, c.h, seq, 1, 0);
         ++seq;
         src->frame(pixels(2, c), c.w, c.h, seq, 2, 0);
         src->wait_done();
         FX1_CHECK_EQ(pop_outcome("look-ahead first", first), FX1_ISP_INGRESS_OUTCOME_OK);
         if (free == 0x1u) {
            FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), 1);
            idle_isp("look-ahead one buffer");
            FX1_CHECK_EQ(ctx_ready(), 0);
            FX1_CHECK_EQ(r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 1);
         } else {
            FX1_CHECK_EQ(pop_outcome("look-ahead second", seq), FX1_ISP_INGRESS_OUTCOME_OK);
            FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), 0);
            FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 2; }));
            FX1_CHECK_EQ(check_output(1, 2, c), 0);
         }
         FX1_CHECK_EQ(check_output(0, 1, c), 0);
         ++cases;
      }
   }

   // Review D1a F1: from the accepted SOF beat the frame is an admitted
   // context: IDLE-class writes (depth, geometry, Bayer) are refused
   // (TUNE_ERROR.MODE) and COMMON_STATUS.BUSY is set, before the first line
   // has reached the pipeline. The frame keeps its configuration; OK, exact.
   void config_window() {
      const cfg c;
      direct(c);
      arm_outputs();
      ++seq;
      const std::uint16_t fid = static_cast<std::uint16_t>(seq);
      src->start(seq, fid, seq);
      const std::vector<std::uint16_t> p = pixels(3, c);
      std::uint32_t busy = 0, depth = 9, width = 0, bayer = 9, err = 0;
      for (std::uint32_t y = 0; y < c.h; ++y) {
         const std::vector<ingress_beat> v = fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, seq, fid);
         for (std::size_t i = 0; i < v.size(); ++i) {
            src->beat(v[i]);
            if (y == 0 && i == 0) {
               src->call([&, this] {  // after the SOF beat, the first line not complete
                  busy = r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK;
                  w32(FX1_ISP_TUNE_ERROR_OFFSET, 0xFFFFFFFFu);
                  w32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 1);
                  w32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, 64);
                  w32(FX1_ISP_COMMON_BAYER_OFFSET, FX1_ISP_COMMON_BAYER_PATTERN_BGGR);
                  depth = r32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET);
                  width = r32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET);
                  bayer = r32(FX1_ISP_COMMON_BAYER_OFFSET);
                  err = r32(FX1_ISP_TUNE_ERROR_OFFSET);
               });
            }
         }
      }
      src->end(seq);
      src->stream_end(seq);
      src->wait_done();
      FX1_CHECK_EQ(busy, FX1_ISP_COMMON_STATUS_BUSY_MASK);
      FX1_CHECK_EQ(depth, 0);
      FX1_CHECK_EQ(width, c.w);
      FX1_CHECK_EQ(bayer, c.pattern);
      FX1_CHECK_EQ(err & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK);
      FX1_CHECK_EQ(pop_outcome("config window", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK_EQ(check_output(0, 3, c), 0);
      idle_isp("config window");
      FX1_CHECK_EQ(r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      w32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 1);  // idle again: accepted
      FX1_CHECK_EQ(r32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET), 1);
      ++cases;
   }

   // Review D1a F2: the CSI ended OK (even before the SOF) and the ISP may
   // already have retired the frame: a line beyond HEIGHT of the same capture,
   // before its stream boundary, still makes it INVALID (EXTRA_LINES); the
   // entry is not READY before the boundary.
   void extra_line_after_ok() {
      const cfg c;
      for (int variant = 0; variant < 3; ++variant) {  // 0 fast ODMA, 1 slow ODMA, 2 near READY
         direct(c);
         if (variant == 1) {
            mem->latency = sc_time(1, SC_US);
            mem->blocking = true;
         }
         arm_outputs();
         scripted(5, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, false, false);
         src->end(seq);  // terminal OK before the extra data
         std::uint32_t ready_before = 9;
         if (variant == 2) {
            src->idle(10000);  // 40 us: the ISP retires the frame meanwhile
            src->call([&, this] { ready_before = ctx_ready(); });
         }
         const std::vector<std::uint16_t> p = pixels(5, c);
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data(), c.w, false, seq, static_cast<std::uint16_t>(seq))) {
            src->beat(b);
         }
         src->stream_end(seq);
         src->wait_done();
         const std::string what = std::string("extra line after OK, variant ") + std::to_string(variant);
         if (variant == 2) {
            FX1_CHECK_EQ_CTX(ready_before, 0, what + ": not READY before the boundary");
         }
         FX1_CHECK_EQ_CTX(pop_outcome(what, seq),
                          FX1_ISP_INGRESS_OUTCOME_INVALID | (FX1_ISP_INGRESS_CAUSE_EXTRA_LINES << FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT),
                          what);
         FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(FX1_ISP_INGRESS_CAUSE_EXTRA_LINES)), 1, what + ": counter");
         mem->latency = SC_ZERO_TIME;
         mem->blocking = false;
         ++cases;
      }
      // End before the SOF, no extra data: OK once its boundary comes.
      direct(c);
      arm_outputs();
      scripted(6, c, [](std::uint32_t, std::vector<ingress_beat> &) {}, true);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("end before SOF, exact frame", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      ++cases;
   }

   // The boundary from a beat of a newer capture (no capture_stream_end).
   void boundary_by_newer_capture() {
      const cfg c;
      direct(c);
      arm_outputs();
      const std::uint64_t first = seq + 1;
      src->frame(pixels(1, c), c.w, c.h, ++seq, 1, 8, false);
      src->end(first);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; }));
      wait(20, SC_US);
      FX1_CHECK_EQ(ctx_ready(), 0);  // boundary unknown
      src->frame(pixels(2, c), c.w, c.h, ++seq, 2);  // its first beat closes the first capture
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("closed by a newer capture", first), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(pop_outcome("newer capture", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 2; }));
      FX1_CHECK_EQ(check_output(1, 2, c), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 0);
      ++cases;
   }

   // Review D1a F3: a null abort of another exposure (TID 1) inside, and after,
   // a valid TID 0 frame is discarded before it touches the frame.
   void foreign_tid_abort() {
      const cfg c;
      direct(c);
      arm_outputs();
      ingress_beat a;
      a.tlast = true;
      a.tuser = 2;
      a.tid = 1;
      scripted(4, c, [a](std::uint32_t y, std::vector<ingress_beat> &v) {
         if (y == 10) {
            v.insert(v.begin() + 3, a);  // mid-line
         }
         if (y == 31) {
            v.push_back(a);  // after the payload is complete
         }
      });
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("foreign-TID abort", seq), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(FX1_ISP_INGRESS_CAUSE_ABORT)), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET), 2);
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK_EQ(check_output(0, 4, c), 0);
      ++cases;
   }

   // D1a re-review R1: the capture's stream ends inside its frame and nothing
   // follows (no next SOF): the frame is invalid at its boundary and
   // abandoned at once; the ISP and the ingress go idle, credits and output
   // slots come back; a good frame afterwards is exact.
   void short_capture_at_boundary() {
      const cfg c;
      struct variant {
         const char *what;
         std::uint32_t full_lines;    // complete lines sent
         std::uint32_t tail_samples;  // samples of the next line, no TLAST (0: none)
         bool boundary_first;         // capture_stream_end before capture_end
         unsigned cause;
      };
      const variant vs[] = {
         {"8 of 32 lines, boundary after the end", 8, 0, false, FX1_ISP_INGRESS_CAUSE_MISSING_LINES},
         {"8 of 32 lines, boundary before the end", 8, 0, true, FX1_ISP_INGRESS_CAUSE_MISSING_LINES},
         {"20 of 66 samples of the first line", 0, 20, false, FX1_ISP_INGRESS_CAUSE_LINE_LENGTH},
         {"20 of 66 samples of the last line", 31, 20, false, FX1_ISP_INGRESS_CAUSE_LINE_LENGTH},
      };
      for (const variant &v : vs) {
         direct(c);
         arm_outputs();
         ++seq;
         const std::uint16_t fid = static_cast<std::uint16_t>(seq);
         src->start(seq, fid, seq);
         const std::vector<std::uint16_t> p = pixels(6, c);
         for (std::uint32_t y = 0; y < v.full_lines; ++y) {
            for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, seq, fid)) {
               src->beat(b);
            }
            src->idle(8);
         }
         if (v.tail_samples) {
            const std::vector<ingress_beat> line =
                fx1_test::axis_source::line_beats(p.data() + v.full_lines * c.w, c.w, v.full_lines == 0, seq, fid);
            for (std::uint32_t i = 0; i < v.tail_samples / 4u; ++i) {
               src->beat(line[i]);  // no TLAST
            }
         }
         if (v.boundary_first) {
            src->stream_end(seq);
            src->end(seq);
         } else {
            src->end(seq);
            src->stream_end(seq);
         }
         src->wait_done();  // the source stops here: nothing else comes
         const std::string what = v.what;
         FX1_CHECK_EQ_CTX(pop_outcome(what, seq),
                          FX1_ISP_INGRESS_OUTCOME_INVALID | (v.cause << FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT), what);
         FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(v.cause)), 1, what + ": cause counter");
         idle_isp(what);  // BUSY cleared, ingress idle: no stuck credit
         FX1_CHECK_EQ_CTX(ctx_used(), 0, what + ": no entry left");
         FX1_CHECK_EQ_CTX(r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, what + ": nothing published");
         FX1_CHECK_EQ_CTX(r32(FX1_ISP_DMA_ERR_OFFSET) & 0x1Du, 0, what + ": no DMA error");
         good_frame(7, c, 0, what + ", next frame");  // head buffer 0: no output slot leaked
         FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET), 2, what + ": admitted");
         ++cases;
      }
   }

   void run() {
      identity();
      short_capture_at_boundary();
      config_window();
      extra_line_after_ok();
      boundary_by_newer_capture();
      foreign_tid_abort();
      credit_lookahead();
      memory_mode();
      switching();
      nominal();
      odma_backpressure();
      not_admitted();
      invalid_frames();
      late_events();
      association();
      terminal_gate();
      tid();
      sc_stop();
   }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n"), irq("irq"), rst2("rst2"), irq2("irq2");
   fx1_isp_params p;
   p.test_datapath_stub = true;
   fx1_isp_tlm dut("isp", p);
   fx1_isp_tlm dut2("isp_unbound", p);
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   fx1_test::test_memory mem2("mem2", mem_base, 1u << 20);
   fx1_test::axis_source src("src");
   bench tb("tb");
   tb.socket.bind(dut.csr_socket);
   tb.ing.bind(dut.ingress_csr_socket);
   tb.ing2.bind(dut2.ingress_csr_socket);
   tb.csr2.bind(dut2.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut2.idma_socket.bind(mem2.idma);
   dut2.odma_socket.bind(mem2.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   dut2.rst_n(rst2);
   dut2.irq(irq2);
   rst2.write(true);
   tb.rst_n(rst_n);
   tb.dut = &dut;
   tb.unbound = &dut2;
   tb.mem = &mem;
   tb.src = &src;
   src.sink = &dut.ingress();
   dut.ingress().set_bound(true);
   sc_start();
   FX1_CHECK_EQ(tb.cases, 34u);
   return fx1_test::summary("fx1_isp_test_ingress_tlm");
}
