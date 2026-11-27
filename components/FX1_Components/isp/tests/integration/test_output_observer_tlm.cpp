// SPDX-License-Identifier: Apache-2.0
// VP-only ISP output observer (bandwidth measurement, plan CSI_ISP_BW1,
// fx1_isp_debug.h): capture -> TAG at ingress admission, and per ODMA frame
// first write issue, last write response and DONE, plus the frame retirement.
//
// Direct frames from a synthetic AXI4-Stream source, datapath stub (as
// test_ingress_tlm). Expected times come from the test memory's own access
// log (start = when the access takes effect, end = start + its latency, the
// response time), for an annotating and for a blocking memory; nothing is
// taken from the unit under test. The observer must change nothing: every
// output byte is checked as in test_ingress_tlm.

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "axis_source.h"
#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_debug.h"
#include "fx1_isp/fx1_isp_ingress.h"
#include "fx1_isp/fx1_isp_ingress_regs.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "test_memory.h"

#define FX1_CHECK_CTX(cond, msg) FX1_CHECK_EQ_CTX((cond) ? 1 : 0, 1, msg)

using namespace sc_core;
namespace isp = cdc::components::fx1_isp;
using isp::control_unit;
using isp::fx1_isp_params;
using isp::fx1_isp_tlm;
using isp::geometry;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 8u << 20;
constexpr std::uint8_t guard = 0xA5;
std::uint64_t out_y(unsigned k) { return mem_base + 0x40'0000ull + 0x4'0000ull * k; }
std::uint64_t out_uv(unsigned k) { return mem_base + 0x60'0000ull + 0x4'0000ull * k; }
constexpr std::uint32_t W = 66, H = 32, YS = 96, UVS = 96;

std::uint16_t sample(unsigned k, std::uint32_t x, std::uint32_t y) {
   return static_cast<std::uint16_t>((x * 7u + y * 13u + k * 29u + 3u) & 0xFFFu);
}

struct recorder : isp::output_observer {
   struct adm {
      std::uint64_t seq;
      std::uint32_t epoch;
      std::uint16_t frame_id;
      std::uint32_t tag;
   };
   std::vector<adm> admitted;
   std::vector<std::pair<std::uint64_t, unsigned>> dropped;
   std::vector<std::pair<isp::output_end, isp::output_record>> outputs;
   struct ret {
      bool direct;
      std::uint32_t tag;
      bool ok;
   };
   std::vector<ret> retired;
   void ingress_admitted(std::uint64_t seq, std::uint32_t epoch, std::uint16_t fid, std::uint32_t tag) override {
      admitted.push_back({seq, epoch, fid, tag});
   }
   void ingress_not_admitted(std::uint64_t seq, unsigned reason) override { dropped.emplace_back(seq, reason); }
   void output_ended(isp::output_end how, const isp::output_record &r) override { outputs.emplace_back(how, r); }
   void frame_retired(bool direct, std::uint32_t tag, bool ok) override { retired.push_back({direct, tag, ok}); }
   void clear() {
      admitted.clear();
      dropped.clear();
      outputs.clear();
      retired.clear();
   }
};

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   tlm_utils::simple_initiator_socket<bench> ing;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fx1_test::axis_source *src = nullptr;
   recorder *obs = nullptr;
   std::uint64_t seq = 0;
   unsigned cases = 0;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), ing("ing"), rst_n("rst_n") { SC_THREAD(run); }

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
   void setup(sc_time latency, bool blocking) {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      mem->log.clear();
      mem->faults.clear();
      mem->latency = latency;
      mem->blocking = blocking;
      w32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W);
      w32(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H);
      w32(FX1_ISP_COMMON_BAYER_OFFSET, FX1_ISP_COMMON_BAYER_PATTERN_RGGB);
      w32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 0);
      w32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, YS);
      w32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, UVS);
      for (unsigned k = 0; k < 4; ++k) {
         w32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(out_y(k)));
         w32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(out_uv(k)));
         mem->fill(out_y(k), 0x4'0000, guard);
         mem->fill(out_uv(k), 0x4'0000, guard);
      }
      w32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      w32(FX1_ISP_DMA_CTRL_OFFSET, (63u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
      iw(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      obs->clear();
   }
   std::vector<std::uint16_t> pixels(unsigned k) const {
      std::vector<std::uint16_t> p(std::size_t{W} * H);
      for (std::uint32_t y = 0; y < H; ++y) {
         for (std::uint32_t x = 0; x < W; ++x) {
            p[std::size_t{y} * W + x] = sample(k, x, y);
         }
      }
      return p;
   }
   std::size_t check_output(unsigned buf, unsigned k) {
      const geometry a = control_unit::derive_active(W, H, FX1_ISP_COMMON_BAYER_PATTERN_RGGB);
      std::size_t bad = 0;
      for (std::uint32_t oy = 0; oy < a.height; ++oy) {
         const std::uint8_t *row = mem->at(out_y(buf) + std::uint64_t{oy} * YS);
         for (std::uint32_t ox = 0; ox < YS; ++ox) {
            const std::uint8_t want = ox < a.width ? static_cast<std::uint8_t>(sample(k, ox, oy) >> 4) : guard;
            bad += row[ox] != want;
         }
      }
      for (std::uint32_t j = 0; j < a.height / 2u; ++j) {
         const std::uint8_t *row = mem->at(out_uv(buf) + std::uint64_t{j} * UVS);
         for (std::uint32_t i = 0; i < UVS; ++i) {
            bad += row[i] != (i < a.width ? 128u : guard);
         }
      }
      return bad;
   }
   void pop_ready() {
      FX1_CHECK(wait_until([&] { return (ir(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK) > 0; }));
      iw(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
   }
   // One capture k; returns the number of ODMA write accesses it caused.
   void frame(unsigned k, bool arm = true) {
      if (arm) {
         w32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
         w32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);
      }
      ++seq;
      src->frame(pixels(k), W, H, seq, static_cast<std::uint16_t>(seq), 8);
      src->wait_done();
      pop_ready();
   }
   // Times of the ODMA writes in mem->log[from, to): first start, last end, count, bytes.
   struct span {
      sc_time first, last;
      std::uint64_t n = 0, bytes = 0;
   };
   span odma_writes(std::size_t from, std::size_t to) const {
      span s;
      for (std::size_t i = from; i < to; ++i) {
         const auto &a = mem->log[i];
         if (a.from_idma || a.cmd != tlm::TLM_WRITE_COMMAND) {
            continue;
         }
         if (s.n == 0 || a.start < s.first) s.first = a.start;
         if (s.n == 0 || a.end > s.last) s.last = a.end;
         ++s.n;
         s.bytes += a.len;
      }
      return s;
   }

   // One good frame: admission, record = memory log, DONE after the last response.
   void timing(const std::string &what, sc_time latency, bool blocking) {
      setup(latency, blocking);
      frame(1);
      FX1_CHECK_CTX(wait_until([&] { return obs->retired.size() == 1; }), what + ": retired");
      FX1_CHECK_EQ_CTX(check_output(0, 1), 0, what + ": output unchanged by the observer");
      FX1_CHECK_EQ_CTX(obs->admitted.size(), 1, what + ": one admission");
      FX1_CHECK_EQ_CTX(obs->outputs.size(), 1, what + ": one output");
      if (obs->admitted.size() == 1 && obs->outputs.size() == 1) {
         const auto &ad = obs->admitted[0];
         const auto &[how, r] = obs->outputs[0];
         const span s = odma_writes(0, mem->log.size());
         FX1_CHECK_EQ_CTX(ad.seq, seq, what + ": admitted capture seq");
         FX1_CHECK_EQ_CTX(ad.frame_id, static_cast<std::uint16_t>(seq), what + ": admitted FRAME_ID");
         FX1_CHECK_CTX(how == isp::output_end::done && r.direct && r.tag == ad.tag && r.tag != 0, what + ": DONE, TAG");
         FX1_CHECK_EQ_CTX(r.slot, 0, what + ": slot 0");
         FX1_CHECK_CTX(r.y_base == out_y(0) && r.uv_base == out_uv(0) && r.y_stride == YS && r.uv_stride == UVS,
                       what + ": plane bases and strides of the claim");
         FX1_CHECK_EQ_CTX(r.writes, s.n, what + ": write count = memory log");
         FX1_CHECK_EQ_CTX(r.write_bytes, s.bytes, what + ": write bytes = memory log");
         FX1_CHECK_CTX(r.wrote && r.first_write_issue == s.first, what + ": first write issue = first access start");
         FX1_CHECK_CTX(r.last_write_response == s.last, what + ": last write response = last access end");
         FX1_CHECK_CTX(r.first_write_issue < r.last_write_response && r.last_write_response <= r.done,
                       what + ": first issue < last response <= DONE");
         const auto &rt = obs->retired[0];
         FX1_CHECK_CTX(rt.direct && rt.tag == ad.tag && rt.ok, what + ": retired OK with its TAG");
      }
      ++cases;
   }

   // Five frames: slots rotate 0..3 then 0; TAGs distinct; each output keyed by
   // the TAG its capture was admitted with, not by its slot.
   void reuse() {
      setup(sc_time(20, SC_NS), false);
      w32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
      w32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);
      for (unsigned k = 0; k < 5; ++k) {
         if (k == 4) {  // slot 0 again: give it back
            w32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x1);
            w32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
         }
         frame(k + 1, false);
         FX1_CHECK(wait_until([&] { return obs->retired.size() == k + 1; }));
      }
      FX1_CHECK_EQ(obs->admitted.size(), 5);
      FX1_CHECK_EQ(obs->outputs.size(), 5);
      std::map<std::uint32_t, std::uint64_t> seq_of;
      for (const auto &a : obs->admitted) seq_of[a.tag] = a.seq;
      FX1_CHECK_EQ_CTX(seq_of.size(), 5, "reuse: TAGs distinct");
      for (std::size_t i = 0; i < obs->outputs.size() && i < 5; ++i) {
         const auto &r = obs->outputs[i].second;
         FX1_CHECK_EQ_CTX(r.slot, static_cast<unsigned>(i % 4), "reuse: slot rotation");
         FX1_CHECK_EQ_CTX(seq_of.count(r.tag), 1, "reuse: output TAG was admitted");
         FX1_CHECK_EQ_CTX(seq_of[r.tag], seq - 4 + i, "reuse: output keyed to its own capture");
      }
      FX1_CHECK_EQ_CTX(check_output(0, 5), 0, "reuse: slot 0 holds frame 5");
      ++cases;
   }

   // A write error: the output ends failed (reported before the recovery
   // unwinds the frame); the abandoned frame does not retire.
   void failed() {
      setup(sc_time(20, SC_NS), false);
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, out_y(0), out_y(0) + 0x4'0000, -1});
      frame(1);
      FX1_CHECK(wait_until([&] { return !obs->outputs.empty(); }));
      wait(100, SC_US);
      FX1_CHECK_EQ(obs->outputs.size(), 1);
      if (!obs->outputs.empty()) {
         const auto &[how, r] = obs->outputs[0];
         FX1_CHECK_CTX(how == isp::output_end::failed && r.wrote && r.writes == 1 && r.tag == obs->admitted.at(0).tag,
                       "failed: output ended failed after its first write, with its TAG");
      }
      for (const auto &rt : obs->retired) FX1_CHECK_CTX(!rt.ok, "failed: no OK retirement");
      ++cases;
   }

   void run() {
      timing("annotated memory", sc_time(100, SC_NS), false);
      timing("blocking memory", sc_time(100, SC_NS), true);
      reuse();
      failed();
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
   recorder obs;
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
   tb.obs = &obs;
   src.sink = &dut.ingress();
   dut.ingress().set_bound(true);
   dut.debug_set_output_observer(&obs);
   sc_start();
   FX1_CHECK_EQ(tb.cases, 4u);
   return fx1_test::summary("fx1_isp_test_output_observer_tlm");
}
