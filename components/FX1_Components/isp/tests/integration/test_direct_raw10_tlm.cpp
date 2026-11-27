// SPDX-License-Identifier: Apache-2.0
// VP direct CSI->ISP path, RAW10 native (D1a, P22): for every committed RAW12
// vector the ISP runs twice from reset with the same profile:
//   memory: IDMA input (s >> 2) << 2 with COMMON_INPUT_FORMAT.DEPTH = RAW12
//           (the memory path's firmware staging of a RAW10 sensor);
//   direct: the stream ingress with s >> 2 and DEPTH = RAW10 (no adapter shift).
// The pipeline normalizes RAW10 once (<< 2), so every output plane and every
// statistics register of expected_stats.txt must be identical between the
// two runs. A double shift, a missing normalization or a depth-dependent
// difference elsewhere makes them differ.
// Usage: fx1_isp_test_direct_raw10_tlm <vectors dir>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
#include "registers/csr_desc.h"
#include "test_memory.h"

#define FX1_CHECK_CTX(cond, msg) FX1_CHECK_EQ_CTX((cond) ? 1 : 0, 1, msg)
#include "vector_io.h"

using namespace sc_core;
using cdc::components::fx1_isp::control_unit;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
using cdc::components::fx1_isp::geometry;
namespace csr = cdc::components::fx1_isp::csr;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 24u << 20;
constexpr std::uint64_t y_addr = mem_base + 0x60'0000ull;
constexpr std::uint64_t uv_addr = mem_base + 0xA0'0000ull;
constexpr std::uint64_t ch1_y_addr = mem_base + 0x100'0000ull;
constexpr std::uint64_t ch1_uv_addr = mem_base + 0x120'0000ull;
constexpr std::uint64_t ch2_addr = mem_base + 0x140'0000ull;

std::vector<std::uint8_t> read_file(const fs::path &p) {
   std::ifstream f(p, std::ios::binary);
   return {std::istreambuf_iterator<char>(f), {}};
}

std::uint32_t align16(std::uint32_t v) { return (v + 15u) & ~15u; }


constexpr std::uint64_t in_addr = mem_base;

struct result {
   std::vector<std::uint8_t> planes;
   std::vector<std::uint32_t> stats;
   std::uint32_t frames_done = 0;
};

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   tlm_utils::simple_initiator_socket<bench> ing_socket;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fx1_test::axis_source *src = nullptr;
   fs::path root;
   std::size_t compared = 0, skipped = 0;
   std::uint64_t seq = 0;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), ing_socket("ing_socket"), rst_n("rst_n") {
      SC_THREAD(run);
   }
   void access(tlm_utils::simple_initiator_socket<bench> &s, tlm::tlm_command cmd, std::uint32_t addr,
               std::uint32_t &v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      s->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::memcpy(&v, b, 4);
   }
   void write32(std::uint32_t a, std::uint32_t v) { access(socket, tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t read32(std::uint32_t a) {
      std::uint32_t v = 0;
      access(socket, tlm::TLM_READ_COMMAND, a, v);
      return v;
   }
   void iwrite(std::uint32_t a, std::uint32_t v) { access(ing_socket, tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t iread(std::uint32_t a) {
      std::uint32_t v = 0;
      access(ing_socket, tlm::TLM_READ_COMMAND, a, v);
      return v;
   }
   bool write_named(const std::string &name, std::uint32_t v) {
      for (std::size_t i = 0; i < csr::num_registers; ++i) {
         if (name == csr::registers[i].name) {
            write32(csr::registers[i].offset, v);
            return true;
         }
      }
      return false;
   }

   void run() {
      for (const auto &e : fs::directory_iterator(root)) {
         if (e.is_directory()) {
            compare_vector(e.path());
         }
      }
      sc_stop();
   }

   void compare_vector(const fs::path &dir) {
      const std::string name = dir.filename().string();
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         if (l.reg == "COMMON_INPUT_FORMAT" && l.value != 0) {
            ++skipped;  // already a RAW10 vector (covered by direct_e2e against the reference)
            return;
         }
      }
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const result m = capture(dir, false, in_w, in_h, out_w, out_h, frames);
      if (m.frames_done == 0) {
         ++skipped;  // geometry refused by the ISP: no output on either path
         return;
      }
      const result d = capture(dir, true, in_w, in_h, out_w, out_h, frames);
      FX1_CHECK_EQ_CTX(d.frames_done, m.frames_done, name + " frames");
      std::size_t diff = 0;
      for (std::size_t i = 0; i < m.planes.size() && i < d.planes.size(); ++i) {
         diff += m.planes[i] != d.planes[i];
      }
      FX1_CHECK_EQ_CTX(d.planes.size(), m.planes.size(), name + " plane bytes");
      FX1_CHECK_EQ_CTX(diff, 0, name + " output planes: memory RAW12(<<2) vs direct RAW10");
      FX1_CHECK_EQ_CTX(d.stats.size(), m.stats.size(), name + " stats count");
      for (std::size_t i = 0; i < m.stats.size() && i < d.stats.size(); ++i) {
         FX1_CHECK_EQ_CTX(d.stats[i], m.stats[i], name + " stats read " + std::to_string(i));
      }
      ++compared;
   }

   result capture(const fs::path &dir, bool direct, std::uint32_t in_w, std::uint32_t in_h, std::uint32_t out_w,
                  std::uint32_t out_h, std::uint32_t frames) {
      result r;
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      mem->log.clear();
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      std::vector<std::uint16_t> px10(std::size_t{in_w} * in_h);
      for (std::size_t i = 0; i < px10.size(); ++i) {
         px10[i] = static_cast<std::uint16_t>(((input[2 * i] | (input[2 * i + 1] << 8)) & 0x0FFF) >> 2);
      }
      const std::uint32_t in_stride = align16(2 * in_w), y_stride = align16(out_w), uv_stride = align16(out_w);
      for (std::uint32_t y = 0; y < in_h; ++y) {
         for (std::uint32_t x = 0; x < in_w; ++x) {
            mem->put16(in_addr + std::uint64_t{y} * in_stride + 2u * x,
                       static_cast<std::uint16_t>(px10[std::size_t{y} * in_w + x] << 2));
         }
      }
      mem->fill(y_addr, std::size_t{y_stride} * out_h, 0xA5);
      mem->fill(uv_addr, std::size_t{uv_stride} * out_h / 2u, 0xA5);
      // DEPTH first: COMMON_INPUT_FORMAT is an input-domain word, refused (TUNE_ERROR.MODE)
      // once a profile has put the ISP in profile mode (MAS v1.07 §9.9.6).
      write32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, direct ? 1u : 0u);
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         if (l.reg == "COMMON_INPUT_FORMAT") {
            continue;
         }
         FX1_CHECK_CTX(write_named(l.reg, l.value), "unknown register " + l.reg);
      }
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK, direct ? 1u : 0u);
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      write32(FX1_ISP_IDMA_STRIDE_OFFSET, in_stride);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, y_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, uv_stride);
      for (unsigned k = 0; k < 4; ++k) {
         write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(in_addr));
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_addr));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_addr));
      }
      struct extra_channel {
         unsigned ch;
         std::uint32_t w, h, stride;
      };
      std::vector<extra_channel> extra;
      {
         std::ifstream chf(dir / "channels.txt");
         unsigned ch = 0;
         std::uint32_t w = 0, h = 0;
         while (chf >> ch >> w >> h) {
            const std::uint32_t stride = align16(ch == 2 ? 3 * w : w);
            extra.push_back({ch, w, h, stride});
            if (ch == 1) {
               mem->fill(ch1_y_addr, std::size_t{stride} * h, 0xA5);
               mem->fill(ch1_uv_addr, std::size_t{stride} * h / 2u, 0xA5);
               write32(FX1_ISP_CH1_ODMA_Y_STRIDE_OFFSET, stride);
               write32(FX1_ISP_CH1_ODMA_UV_STRIDE_OFFSET, stride);
               for (unsigned k = 0; k < 4; ++k) {
                  write32(FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(ch1_y_addr));
                  write32(FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(ch1_uv_addr));
               }
            } else {
               mem->fill(ch2_addr, std::size_t{stride} * h, 0xA5);
               write32(FX1_ISP_CH2_ODMA_Y_STRIDE_OFFSET, stride);
               for (unsigned k = 0; k < 4; ++k) {
                  write32(FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(ch2_addr));
               }
            }
         }
      }
      write32(FX1_ISP_DMA_CTRL_OFFSET, direct ? 0x3F02u : 0x3F03u);
      if (direct) {
         iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      }
      const std::vector<fx1_test::csr_line> between = fx1_test::read_frame_writes(dir / "frames.csrw");
      for (std::uint32_t f = 0; f < frames; ++f) {
         for (const fx1_test::csr_line &w : between) {
            if (w.frame == f) {
               write_named(w.reg, w.value);
            }
         }
         const std::uint32_t bit = 1u << (f % 4u);
         write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, bit);
         for (const extra_channel &x : extra) {
            write32(x.ch == 1 ? FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET : FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET, 0xF);
            write32(x.ch == 1 ? FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET : FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, bit);
         }
         if (direct) {
            ++seq;
            src->frame(px10, in_w, in_h, seq, static_cast<std::uint16_t>(seq & 0x3FFF));
            src->wait_done();
         } else {
            write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, bit);
         }
         const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
         while (dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) < f + 1 && sc_time_stamp() < end) {
            wait(10, SC_US);
         }
         if (direct && (iread(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK)) {
            FX1_CHECK_EQ(iread(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET), FX1_ISP_INGRESS_OUTCOME_OK);
            iwrite(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
         }
      }
      r.frames_done = dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
      const sc_time idle_by = sc_time_stamp() + sc_time(10, SC_MS);
      while ((read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < idle_by) {
         wait(1, SC_US);
      }
      auto take = [&](std::uint64_t base, std::uint32_t stride, std::uint32_t rows) {
         const std::uint8_t *p = mem->at(base);
         r.planes.insert(r.planes.end(), p, p + std::size_t{stride} * rows);
      };
      take(y_addr, y_stride, out_h);
      take(uv_addr, uv_stride, out_h / 2u);
      for (const extra_channel &x : extra) {
         if (x.ch == 1) {
            take(ch1_y_addr, x.stride, x.h);
            take(ch1_uv_addr, x.stride, x.h / 2u);
         } else {
            take(ch2_addr, x.stride, x.h);
         }
      }
      for (const fx1_test::csr_line &l : fx1_test::read_expected(dir / "expected_stats.txt")) {
         if (l.set) {
            write_named(l.reg, l.value);
            continue;
         }
         for (std::size_t i = 0; i < csr::num_registers; ++i) {
            if (l.reg == csr::registers[i].name) {
               std::uint32_t v = read32(csr::registers[i].offset);
               if (csr::registers[i].offset == FX1_ISP_TUNE_AEC_RESULT_META_OFFSET) {
                  // The AEC result reports the native input depth (MAS: RESULT_META.NATIVE_DEPTH):
                  // it differs by design; compared on its own, the rest with the other run.
                  const std::uint32_t nd = (v & FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_MASK) >>
                                           FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_SHIFT;
                  if (v & FX1_ISP_TUNE_AEC_RESULT_META_LEASE_VALID_MASK) {
                     FX1_CHECK_EQ_CTX(nd, direct ? 1u : 0u, dir.filename().string() + " AEC NATIVE_DEPTH");
                  }
                  v &= ~FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_MASK;
               }
               r.stats.push_back(v);
            }
         }
      }
      if (direct) {
         FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0);
      }
      return r;
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   if (argc < 2) {
      std::fprintf(stderr, "usage: %s <vectors dir>\n", argv[0]);
      return 2;
   }
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   fx1_test::axis_source src("src");
   bench tb("tb");
   tb.root = argv[1];
   tb.socket.bind(dut.csr_socket);
   tb.ing_socket.bind(dut.ingress_csr_socket);
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
   FX1_CHECK(tb.compared >= 40);
   std::printf("direct RAW10: %zu vectors compared, %zu skipped\n", tb.compared, tb.skipped);
   return fx1_test::summary("fx1_isp_test_direct_raw10_tlm");
}
