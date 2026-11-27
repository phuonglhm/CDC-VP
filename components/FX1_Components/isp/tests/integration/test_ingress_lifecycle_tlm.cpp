// SPDX-License-Identifier: Apache-2.0
// VP direct CSI->ISP stream ingress, lifecycle (D1b; contract
// DIRECT_CSI_ISP_CONTRACT.md §9.1 rules 1-8, §9.3, §10): context table full
// and firmware not popping, untracked captures, capture sequence vs FRAME_ID
// (duplicates, wrap, sequence above 32 bits), TAG wrap, CSI reset and ISP
// soft reset / error recovery with outputs still being written (CANCELLING
// until the ISP has drained), i_rst_n, and the C driver (fx1_isp_ingress_drv).
// Same synthetic source and datapath stub as test_ingress_tlm.

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
#include "fx1_isp/fx1_isp_ingress_drv.h"

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
   std::uint64_t seq = 0;
   unsigned cases = 0;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), ing("ing"), rst_n("rst_n") {
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

   // ---- helpers for this file ----------------------------------------------------
   void send_frame(unsigned k, const cfg &c, std::uint64_t s, std::uint16_t fid, bool with_end = true,
                   capture_outcome out = capture_outcome::ok, std::uint32_t lines = 0) {
      src->start(s, fid, 1000u * s);
      const std::vector<std::uint16_t> p = pixels(k, c);
      const std::uint32_t h = lines ? lines : c.h;
      for (std::uint32_t y = 0; y < h; ++y) {
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, s, fid)) {
            src->beat(b);
         }
         src->idle(8);
      }
      if (with_end) {
         src->end(s, out);
         src->stream_end(s);
      }
   }
   void csi_reset() {
      fx1_test::axis_op o;
      o.k = fx1_test::axis_op::csi_reset;
      src->push(o);
   }
   std::uint32_t ctx_seq_lo() { return ir(FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET); }
   std::uint32_t outcome_code(std::uint32_t o) { return o & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK; }

   // ---- cases ------------------------------------------------------------------------

   // Rules 1, 4, 7: firmware never POPs. Four admitted frames still being
   // written (slow ODMA), a frame with no output credit, four started
   // captures waiting for their SOF: 8 entries used. The next capture is
   // untracked (CTX_FULL), its frame dropped NO_CONTEXT with TREADY 1;
   // nothing evicted; the admitted frames complete; after the POPs the waiting
   // captures and a capture that starts afterwards are processed normally.
   void context_full_no_pop() {
      const cfg c;
      direct(c);
      arm_outputs();
      mem->latency = sc_time(1, SC_US);
      mem->blocking = true;
      const std::uint64_t f1 = seq + 1;
      for (unsigned i = 0; i < 4; ++i) {
         ++seq;
         send_frame(i, c, seq, static_cast<std::uint16_t>(0x100 + i));
      }
      const std::uint64_t f5 = ++seq;
      send_frame(4, c, f5, 0x105);  // the four output buffers are taken: NO_CREDIT
      std::vector<std::uint64_t> waiting;
      for (unsigned i = 0; i < 4; ++i) {
         waiting.push_back(++seq);
         src->start(seq, static_cast<std::uint16_t>(0x200 + i), 1000u * seq);
      }
      std::uint32_t used_full = 0, ready_full = 9;
      src->call([&, this] { used_full = ctx_used(); ready_full = ctx_ready(); });
      const std::uint64_t s10 = ++seq;
      src->start(s10, 0x210, 1000u * s10);  // no free entry: untracked
      src->wait_done();
      FX1_CHECK_EQ(used_full, 8);
      FX1_CHECK(ready_full < 4);  // the admitted frames were still in flight when the table filled
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_CTX_FULL_MASK,
                   FX1_ISP_INGRESS_STATUS_CTX_FULL_MASK);
      FX1_CHECK(wait_until([&] { return ctx_ready() == 4; }));  // no deadlock: the reserved entries complete
      FX1_CHECK_EQ(ctx_used(), 8);
      mem->latency = SC_ZERO_TIME;
      mem->blocking = false;
      std::uint32_t last_tag = 0;
      for (unsigned i = 0; i < 4; ++i) {  // terminals kept, in admission order
         FX1_CHECK_EQ(ctx_seq_lo(), static_cast<std::uint32_t>(f1 + i));
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_FRAME_OFFSET) & FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK, 0x100u + i);
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET), FX1_ISP_INGRESS_OUTCOME_OK);
         const std::uint32_t tag = ir(FX1_ISP_INGRESS_CTX_TAG_OFFSET);
         FX1_CHECK(tag > last_tag);
         FX1_CHECK_EQ(r32(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * i), tag);
         last_tag = tag;
         iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
         FX1_CHECK_EQ(check_output(i, i, c), 0);
      }
      FX1_CHECK_EQ(ctx_used(), 4);  // the four waiting captures, untouched
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 0);
      iw(FX1_ISP_INGRESS_STATUS_OFFSET, FX1_ISP_INGRESS_STATUS_CTX_FULL_MASK);
      // The waiting captures send their frames (in capture order), then the
      // untracked one (dropped although entries are free now), then a
      // capture started after the POPs.
      arm_outputs();
      for (unsigned i = 0; i < 4; ++i) {
         const std::uint64_t s = waiting[i];
         const std::uint16_t fid = static_cast<std::uint16_t>(0x200 + i);
         const std::vector<std::uint16_t> p = pixels(10 + i, c);
         for (std::uint32_t y = 0; y < c.h; ++y) {
            for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, s, fid)) {
               src->beat(b);
            }
         }
         src->end(s);
         src->stream_end(s);
         src->wait_done();
         FX1_CHECK_EQ(pop_outcome("waiting capture " + std::to_string(i), s), FX1_ISP_INGRESS_OUTCOME_OK);
      }
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 8; }));
      FX1_CHECK_EQ(check_output(3, 13, c), 0);
      std::uint64_t r_before = 0, r_after = 0;
      src->call([&] { r_before = src->refused; });
      {
         const std::vector<std::uint16_t> p = pixels(5, c);
         for (std::uint32_t y = 0; y < c.h; ++y) {
            for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, s10, 0x210)) {
               src->beat(b);
            }
         }
         src->end(s10);
         src->stream_end(s10);
      }
      src->call([&] { r_after = src->refused; });
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_LAST_DROP_SEQ_LO_OFFSET), static_cast<std::uint32_t>(s10));
      FX1_CHECK_EQ(r_after, r_before);  // the untracked capture's frame: TREADY 1 throughout
      FX1_CHECK_EQ(ctx_used(), 0);
      good_frame(20, c, 0, "capture started after the POPs");
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 0);
      ++cases;
   }

   // Rule 1: a capture that found the table full stays untracked even if an
   // entry frees before its SOF.
   void untracked_stays_untracked() {
      const cfg c;
      direct(c);
      arm_outputs();
      const std::uint64_t a = ++seq;
      send_frame(1, c, a, 1);  // READY, not popped
      src->wait_done();
      FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
      std::vector<std::uint64_t> held;
      for (unsigned i = 0; i < 7; ++i) {
         held.push_back(++seq);
         src->start(seq, static_cast<std::uint16_t>(seq), 1000u * seq);
      }
      const std::uint64_t u = ++seq;
      src->start(u, 0x77, 1000u * u);  // untracked
      src->wait_done();
      FX1_CHECK_EQ(ctx_used(), 8);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET), 1);
      iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);  // an entry frees before u's SOF
      FX1_CHECK_EQ(ctx_used(), 7);
      const std::vector<std::uint16_t> p = pixels(2, c);
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data() + y * c.w, c.w, y == 0, u, 0x77)) {
            src->beat(b);
         }
      }
      src->end(u);
      src->stream_end(u);
      for (const std::uint64_t h : held) {  // their CSI terminals: dropped before their SOF
         src->end(h, capture_outcome::dropped);
      }
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET), 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 7);  // retired at u's SOF (older)
      FX1_CHECK_EQ(ctx_used(), 0);
      ++cases;
   }

   // The 64-bit capture sequence is the key (§9, R1): duplicated and wrapping
   // FRAME_IDs, a sequence crossing 2^32; an old READY entry is not touched by
   // a newer capture with the same FRAME_ID.
   void sequence_and_frame_id() {
      const cfg c;
      direct(c);
      arm_outputs();
      const std::uint64_t a = ++seq;
      send_frame(1, c, a, 5);  // FRAME_ID 5, READY, not popped
      src->wait_done();
      FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
      const std::uint64_t b = ++seq;  // the same FRAME_ID, no SOF, dropped
      src->start(b, 5, 1000u * b);
      src->end(b, capture_outcome::dropped);
      src->stream_end(b);
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 1);
      FX1_CHECK_EQ(pop_outcome("FRAME_ID 5, older capture", a), FX1_ISP_INGRESS_OUTCOME_OK);
      // FRAME_ID wrap: 16383 then 1 (14-bit field, values passed through).
      for (const std::uint16_t fid : {std::uint16_t{0x3FFF}, std::uint16_t{1}}) {
         ++seq;
         send_frame(2, c, seq, fid);
         src->wait_done();
         FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
         FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_FRAME_OFFSET) & FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK, fid);
         FX1_CHECK_EQ(pop_outcome("FRAME_ID wrap", seq), FX1_ISP_INGRESS_OUTCOME_OK);
         arm_outputs();
      }
      // A sequence crossing 2^32: a capture without SOF at 0xFFFFFFFF is
      // older than 0x1'0000'0000 (64-bit order), retired at its SOF.
      const std::uint64_t e = 0xFFFF'FFFFull, f = 0x1'0000'0000ull;
      src->start(e, 9, 1);
      src->end(e, capture_outcome::dropped);
      send_frame(3, c, f, 10);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET), 0u);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_SEQ_HI_OFFSET), 1u);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET), FX1_ISP_INGRESS_OUTCOME_OK);
      iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET), 2);
      FX1_CHECK_EQ(ctx_used(), 0);
      seq = f;
      ++cases;
   }

   // TAG: 32-bit, from 1, never 0 (§9.1).
   void tag_wrap() {
      const cfg c;
      direct(c);
      dut->debug_set_ingress_tag(0xFFFF'FFFEu);
      std::uint32_t tags[2] = {};
      for (unsigned i = 0; i < 2; ++i) {
         arm_outputs();
         ++seq;
         send_frame(i, c, seq, 1);
         src->wait_done();
         FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
         tags[i] = ir(FX1_ISP_INGRESS_CTX_TAG_OFFSET);
         FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == i + 1; }));
         FX1_CHECK_EQ(r32(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * i), tags[i]);
         iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
      }
      FX1_CHECK_EQ(tags[0], 0xFFFF'FFFFu);
      FX1_CHECK_EQ(tags[1], 1u);  // 0 skipped
      ++cases;
   }

   // Rule 8, CSI reset (§10, R2): admitted frames stay CANCELLING, not readable
   // and their output slot not reusable, until the ISP has retired them with
   // every write response; PENDING entries are released; no write of the old
   // frame after its entry is READY; the next frame is exact.
   void csi_reset_cancelling() {
      const cfg c;
      for (int variant = 0; variant < 2; ++variant) {  // 0 mid-frame, 1 all lines sent (ISP still writing)
         direct(c);
         arm_outputs();
         mem->latency = sc_time(3, SC_US);
         mem->blocking = true;
         const std::uint64_t g = ++seq;
         send_frame(1, c, g, 1, false, capture_outcome::ok, variant == 0 ? 20u : c.h);
         const std::uint64_t pend = ++seq;
         src->start(pend, 2, 1);  // a capture waiting for its SOF
         csi_reset();
         src->wait_done();
         FX1_CHECK_EQ(ctx_ready(), 0);
         iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);  // a POP while CANCELLING has no effect
         FX1_CHECK_EQ(ctx_used(), 1);            // the pending capture released, the admitted one kept
         sc_time ready_at = SC_ZERO_TIME;
         bool early = false;
         const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
         while (sc_time_stamp() < end) {
            const bool writing = mem->active_odma > 0;
            if (ctx_ready() > 0) {
               ready_at = sc_time_stamp();
               early = writing;
               break;
            }
            wait(200, SC_NS);
         }
         const std::string what = std::string("CSI reset, variant ") + std::to_string(variant);
         FX1_CHECK_CTX(ready_at > SC_ZERO_TIME, what + ": READY after the drain");
         FX1_CHECK_CTX(!early, what + ": not READY while a write is outstanding");
         std::size_t after = 0;
         for (const auto &acc : mem->log) {
            after += acc.cmd == tlm::TLM_WRITE_COMMAND && acc.end > ready_at;
         }
         FX1_CHECK_EQ_CTX(after, 0, what + ": no write of the old frame completes after READY");
         FX1_CHECK_EQ_CTX(pop_outcome(what, g), FX1_ISP_INGRESS_OUTCOME_RESET, what + ": RESET");
         wait(100, SC_US);
         std::size_t stale = 0;
         for (const auto &acc : mem->log) {
            stale += acc.cmd == tlm::TLM_WRITE_COMMAND && acc.start > ready_at;
         }
         FX1_CHECK_EQ_CTX(stale, 0, what + ": no stale write");
         mem->latency = SC_ZERO_TIME;
         mem->blocking = false;
         idle_isp(what);
         const std::uint32_t done = r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
         good_frame(5, c, done % 4u, what + ", next frame");
         ++cases;
      }
   }

   // Rule 8, ISP soft reset (R2): the reset clears DMA_STAT.ODMA_BUSY while
   // an accepted write is still outstanding: the entry stays CANCELLING until
   // the reset is released and no AXI transaction is outstanding. A frame the
   // ISP had already retired is not affected.
   void isp_soft_reset_drain() {
      const cfg c;
      direct(c);
      arm_outputs();
      // J: retired by the ISP, waiting for its CSI terminal.
      const std::uint64_t j = ++seq;
      send_frame(7, c, j, 7, false);
      src->stream_end(j);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; }));
      // H: being written, slow blocking memory.
      mem->latency = sc_time(5, SC_US);
      mem->blocking = true;
      const std::uint64_t h = ++seq;
      send_frame(8, c, h, 8);
      FX1_CHECK(wait_until([&] { return mem->active_odma > 0; }));
      w32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
      const bool write_outstanding = mem->active_odma > 0;
      const std::uint32_t odma_busy = r32(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_ODMA_BUSY_MASK;
      std::printf("soft reset with a write outstanding: DMA_STAT.ODMA_BUSY=%u\n", odma_busy ? 1u : 0u);
      FX1_CHECK_CTX(write_outstanding, "soft reset while a write is outstanding");
      FX1_CHECK_EQ(ctx_ready(), 0);
      src->wait_done();
      src->end(j);  // J's terminal: J completes OK, untouched by the reset
      src->wait_done();
      sc_time ready_at = SC_ZERO_TIME;
      bool early = false;
      unsigned busy0_outstanding = 0, samples = 0;  // DMA_STAT.ODMA_BUSY = 0 while a write is outstanding
      const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
      while (sc_time_stamp() < end) {
         ++samples;
         busy0_outstanding += mem->active_odma > 0 && !(r32(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_ODMA_BUSY_MASK);
         if (ctx_ready() == 2) {
            ready_at = sc_time_stamp();
            early = mem->active_odma > 0;
            break;
         }
         wait(200, SC_NS);
      }
      std::printf("soft reset drain: %u of %u samples with ODMA_BUSY = 0 and a write outstanding\n", busy0_outstanding, samples);
      FX1_CHECK_CTX(ready_at > SC_ZERO_TIME, "soft reset: READY after the drain");
      FX1_CHECK_CTX(!early, "soft reset: not READY while a write is outstanding");
      FX1_CHECK_EQ(pop_outcome("retired before the reset", j), FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(pop_outcome("soft reset", h), FX1_ISP_INGRESS_OUTCOME_RESET);
      mem->latency = SC_ZERO_TIME;
      mem->blocking = false;
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) == 0; }));
      idle_isp("soft reset");
      good_frame(9, c, 0, "after the soft reset");  // rotations restarted at 0
      ++cases;
   }

   // A late ISP error (R2, D1.3): an ODMA write error aborts the frame through
   // the automatic recovery: outcome ISP_FAILED (not RESET), READY after the
   // drain, credit returned; the next frame is exact.
   void late_isp_error() {
      const cfg c;
      direct(c);
      arm_outputs();
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, out_y(0) + 5u * c.y_stride, out_y(0) + 6u * c.y_stride, 1});
      const std::uint64_t k = ++seq;
      send_frame(2, c, k, 2);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("ODMA write error", k), FX1_ISP_INGRESS_OUTCOME_ISP_FAILED);
      FX1_CHECK_EQ(r32(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT, FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT);
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK) != 0; }));
      w32(FX1_ISP_DMA_ERR_OFFSET, 0xFFFF'FFFFu);  // software acknowledges the error
      idle_isp("ODMA write error");
      FX1_CHECK_EQ(ctx_used(), 0);
      mem->faults.clear();
      good_frame(3, c, 0, "after the recovery");
      ++cases;
   }

   // i_rst_n: the block returns to its reset values (SOURCE memory, empty table).
   void hard_reset_block() {
      const cfg c;
      direct(c);
      arm_outputs();
      ++seq;
      send_frame(1, c, seq, 1);
      src->start(++seq, 2, 2);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return ctx_ready() == 1; }));
      reset();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_ID_OFFSET), FX1_ISP_INGRESS_ID_VALUE);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTRL_OFFSET), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_STATUS_OFFSET), FX1_ISP_INGRESS_STATUS_IDLE_MASK);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CTX_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET), 0);
      ++cases;
   }

   // The C driver on the model (fx1_isp_ingress_drv.h).
   static std::uint32_t hal_read(void *ctx, std::uint32_t off) { return static_cast<bench *>(ctx)->ir(off); }
   static void hal_write(void *ctx, std::uint32_t off, std::uint32_t v) { static_cast<bench *>(ctx)->iw(off, v); }
   static std::uint32_t hal_isp_read(void *ctx, std::uint32_t off) { return static_cast<bench *>(ctx)->r32(off); }
   static void hal_delay(void *, std::uint32_t cycles) { sc_core::wait(sc_time(2.0 * cycles, SC_NS)); }
   void driver() {
      const cfg c;
      reset();
      program(c);
      fx1_isp_ingress dev{};
      fx1_isp_hal hal{hal_read, hal_write, hal_delay, this};
      FX1_CHECK_EQ(fx1_isp_ingress_init(nullptr, &hal), FX1_ISP_EINVAL);
      fx1_isp_hal wrong{hal_isp_read, hal_write, hal_delay, this};  // the ISP's own CSR page: not 'ISPI'
      FX1_CHECK_EQ(fx1_isp_ingress_init(&dev, &wrong), FX1_ISP_ENOTSUP);
      FX1_CHECK_EQ(fx1_isp_ingress_init(&dev, &hal), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_ingress_bound(&dev), 1);
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, 7), FX1_ISP_EINVAL);
      w32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, FX1_ISP_INGRESS_SOURCE_DIRECT), FX1_ISP_EBUSY);
      FX1_CHECK_EQ(fx1_isp_ingress_source(&dev), FX1_ISP_INGRESS_SOURCE_MEMORY);
      FX1_CHECK_EQ(fx1_isp_ingress_status(&dev) & FX1_ISP_INGRESS_STATUS_STICKY_MASK, 0);  // cleared by the call
      w32(FX1_ISP_DMA_CTRL_OFFSET, (63u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, FX1_ISP_INGRESS_SOURCE_DIRECT), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_ingress_source(&dev), FX1_ISP_INGRESS_SOURCE_DIRECT);
      fx1_isp_ingress_ctx cx{};
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_peek(&dev, &cx), FX1_ISP_ENODATA);
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_peek(&dev, nullptr), FX1_ISP_EINVAL);
      arm_outputs();
      const std::uint64_t s = 0x1'2345'6789ull;
      send_frame(4, c, s, 0x1ABC);
      src->wait_done();
      FX1_CHECK(wait_until([&] { return fx1_isp_ingress_ctx_ready(&dev) == 1; }));
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_used(&dev), 1);
      const std::uint32_t tag = ir(FX1_ISP_INGRESS_CTX_TAG_OFFSET), isp_frame = ir(FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET);
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_pop(&dev, &cx), FX1_ISP_OK);
      FX1_CHECK_EQ(cx.tag, tag);
      FX1_CHECK_EQ(cx.seq, s);
      FX1_CHECK_EQ(cx.frame_id, 0x1ABC);
      FX1_CHECK_EQ(cx.ts_ns, 1000u * s);
      FX1_CHECK_EQ(cx.outcome, FX1_ISP_INGRESS_OUTCOME_OK);
      FX1_CHECK_EQ(cx.cause, 0);
      FX1_CHECK_EQ(cx.lines, c.h);
      FX1_CHECK_EQ(cx.samples, c.w * c.h);
      FX1_CHECK_EQ(cx.isp_frame, isp_frame);
      FX1_CHECK(cx.isp_frame != 0);
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_used(&dev), 0);
      FX1_CHECK_EQ(fx1_isp_ingress_ctx_pop(&dev, &cx), FX1_ISP_ENODATA);
      fx1_isp_ingress_counters k{};
      fx1_isp_ingress_read_counters(&dev, &k);
      FX1_CHECK_EQ(k.frames_admitted, 1);
      FX1_CHECK_EQ(k.beats_accepted, 17u * c.h);
      FX1_CHECK_EQ(k.frames_invalid, 0);
      for (std::uint32_t v : k.cause) {
         FX1_CHECK_EQ(v, 0);
      }
      seq = s;
      // Not bound: direct is selected, admits nothing, ENOTSUP.
      idle_isp("driver");
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, FX1_ISP_INGRESS_SOURCE_MEMORY), FX1_ISP_OK);
      dut->ingress().set_bound(false);
      FX1_CHECK_EQ(fx1_isp_ingress_bound(&dev), 0);
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, FX1_ISP_INGRESS_SOURCE_DIRECT), FX1_ISP_ENOTSUP);
      FX1_CHECK_EQ(fx1_isp_ingress_source(&dev), FX1_ISP_INGRESS_SOURCE_DIRECT);
      FX1_CHECK_EQ(fx1_isp_ingress_set_source(&dev, FX1_ISP_INGRESS_SOURCE_MEMORY), FX1_ISP_OK);
      dut->ingress().set_bound(true);
      fx1_isp_ingress_clear_status(&dev, 0xFFFF'FFFFu);
      FX1_CHECK_EQ(fx1_isp_ingress_status(&dev), FX1_ISP_INGRESS_STATUS_IDLE_MASK);
      ++cases;
   }

   void soft_reset_and_wait() {
      w32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
      FX1_CHECK(wait_until([&] {
         return (r32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) == 0;
      }));
      wait(1, SC_US);
   }

   // D1b review F1: an ISP reset does not cancel a frame the ISP had already
   // retired, and does not drop that capture's stream checks either: a line
   // beyond HEIGHT of the same capture after the reset still makes it
   // INVALID (EXTRA_LINES); without extra data it is OK.
   void retired_frame_checked_after_isp_reset() {
      const cfg c;
      for (int extra = 0; extra < 2; ++extra) {
         direct(c);
         arm_outputs();
         const std::uint64_t r = ++seq;
         const std::uint16_t fid = static_cast<std::uint16_t>(r);
         send_frame(3, c, r, fid, false);  // the whole payload, no terminal, no boundary
         src->wait_done();
         FX1_CHECK(wait_until([&] { return r32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == 1; }));
         FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; }));
         FX1_CHECK_EQ(ctx_ready(), 0);
         soft_reset_and_wait();
         FX1_CHECK_EQ(ctx_used(), 1);  // kept: the ISP had retired it
         if (extra) {
            const std::vector<std::uint16_t> p = pixels(3, c);
            for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data(), c.w, false, r, fid)) {
               src->beat(b);
            }
         }
         src->end(r);
         src->stream_end(r);
         src->wait_done();
         const std::string what = extra ? "extra line after an ISP reset" : "retired frame across an ISP reset";
         FX1_CHECK_EQ_CTX(pop_outcome(what, r),
                          extra ? FX1_ISP_INGRESS_OUTCOME_INVALID |
                                      (FX1_ISP_INGRESS_CAUSE_EXTRA_LINES << FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT)
                                : FX1_ISP_INGRESS_OUTCOME_OK,
                          what);
         FX1_CHECK_EQ_CTX(ir(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), extra ? 1u : 0u, what + ": FRAMES_INVALID");
         ++cases;
      }
   }

   // D1b review F2: an ISP soft reset or error recovery releases the PENDING
   // entries (§9.1 rule 8): a full table of captures waiting for their SOF
   // does not make the first capture after the reset untracked. A late SOF /
   // terminal of a released capture touches nothing.
   void isp_reset_releases_pending() {
      const cfg c;
      // Soft reset with 8 PENDING entries (terminal DROPPED, no SOF, no boundary yet).
      direct(c);
      std::vector<std::uint64_t> old;
      for (unsigned i = 0; i < 8; ++i) {
         old.push_back(++seq);
         src->start(seq, static_cast<std::uint16_t>(seq), 1000u * seq);
         if (i < 7) {
            src->end(seq, capture_outcome::dropped);
         }
      }
      src->wait_done();
      FX1_CHECK_EQ(ctx_used(), 8);
      soft_reset_and_wait();
      FX1_CHECK_EQ(ctx_used(), 0);
      src->end(old.back(), capture_outcome::ok);  // a released capture's terminal: ignored
      src->wait_done();
      FX1_CHECK_EQ(ctx_used(), 0);
      good_frame(4, c, 0, "first capture after the soft reset");
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET), 0);
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET), 0);
      // A beat of a released capture after a newer one: dropped, no effect.
      const std::vector<std::uint16_t> p = pixels(1, c);
      for (const ingress_beat &b : fx1_test::axis_source::line_beats(p.data(), c.w, true, old[0], 1)) {
         src->beat(b);
      }
      src->wait_done();
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET), 1);
      FX1_CHECK_EQ(ctx_used(), 0);
      ++cases;
      // Error recovery with PENDING entries: the faulty frame CANCELLING
      // (ISP_FAILED), the PENDING ones released at once.
      direct(c);
      arm_outputs();
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, out_y(0) + 5u * c.y_stride, out_y(0) + 6u * c.y_stride, 1});
      const std::uint64_t k = ++seq;
      src->start(k, 1, 1);
      for (unsigned i = 0; i < 5; ++i) {
         ++seq;
         src->start(seq, static_cast<std::uint16_t>(seq), 1);
      }
      const std::uint16_t kf = 1;
      for (std::uint32_t y = 0; y < c.h; ++y) {
         for (const ingress_beat &b : fx1_test::axis_source::line_beats(pixels(2, c).data() + y * c.w, c.w, y == 0, k, kf)) {
            src->beat(b);
         }
      }
      src->end(k);
      src->stream_end(k);
      src->wait_done();
      FX1_CHECK_EQ(pop_outcome("error recovery with PENDING entries", k), FX1_ISP_INGRESS_OUTCOME_ISP_FAILED);
      FX1_CHECK_EQ(ctx_used(), 0);  // the 5 PENDING entries released by the recovery
      FX1_CHECK(wait_until([&] { return (r32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK) != 0; }));
      w32(FX1_ISP_DMA_ERR_OFFSET, 0xFFFF'FFFFu);
      mem->faults.clear();
      idle_isp("error recovery with PENDING entries");
      good_frame(5, c, 0, "first capture after the recovery");
      FX1_CHECK_EQ(ir(FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET), 0);
      ++cases;
   }

   void run() {
      retired_frame_checked_after_isp_reset();
      isp_reset_releases_pending();
      context_full_no_pop();
      untracked_stays_untracked();
      sequence_and_frame_id();
      tag_wrap();
      csi_reset_cancelling();
      isp_soft_reset_drain();
      late_isp_error();
      hard_reset_block();
      driver();
      sc_stop();
   }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n"), irq("irq");
   fx1_isp_params p;
   p.test_datapath_stub = true;
   fx1_isp_tlm dut("isp", p);
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   fx1_test::axis_source src("src");
   bench tb("tb");
   tb.socket.bind(dut.csr_socket);
   tb.ing.bind(dut.ingress_csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.dut = &dut;
   tb.mem = &mem;
   tb.src = &src;
   src.sink = &dut.ingress();
   dut.ingress().set_bound(true);
   sc_start();
   FX1_CHECK_EQ(tb.cases, 14u);
   return fx1_test::summary("fx1_isp_test_ingress_lifecycle_tlm");
}
