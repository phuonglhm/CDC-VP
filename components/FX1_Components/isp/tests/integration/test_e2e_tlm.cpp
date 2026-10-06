// SPDX-License-Identifier: Apache-2.0
// End-to-end check through the whole IP (DEC-03): the committed vectors are
// programmed over the CSR socket, the RAW frame is placed in RAM, the frame
// runs IDMA -> pipeline -> ODMA, and the NV12 planes in RAM must equal the
// independent Python reference bit for bit.
// Usage: fx1_isp_test_e2e_tlm <vectors dir>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "test_memory.h"
#include "vector_io.h"

using namespace sc_core;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
namespace csr = cdc::components::fx1_isp::csr;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 16u << 20;
constexpr std::uint64_t in_addr = mem_base;
constexpr std::uint64_t y_addr = mem_base + 0x60'0000ull;
constexpr std::uint64_t uv_addr = mem_base + 0xA0'0000ull;

std::vector<std::uint8_t> read_file(const fs::path &p) {
   std::ifstream f(p, std::ios::binary);
   return {std::istreambuf_iterator<char>(f), {}};
}

std::uint32_t align16(std::uint32_t v) { return (v + 15u) & ~15u; }

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fs::path root;
   std::size_t vectors = 0;
   std::size_t partial_probes = 0;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n") { SC_THREAD(run); }

   void write32(std::uint32_t addr, std::uint32_t v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(tlm::TLM_WRITE_COMMAND);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      socket->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
   }
   std::uint32_t read32(std::uint32_t addr) {
      unsigned char b[4] = {};
      tlm::tlm_generic_payload t;
      t.set_command(tlm::TLM_READ_COMMAND);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      socket->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::uint32_t v;
      std::memcpy(&v, b, 4);
      return v;
   }
   void write_named(const std::string &name, std::uint32_t v) {
      for (std::size_t i = 0; i < csr::num_registers; ++i) {
         if (name == csr::registers[i].name) {
            write32(csr::registers[i].offset, v);
            return;
         }
      }
      FX1_CHECK_EQ_CTX(1, 0, "unknown register " + name);
   }

   void run() {
      for (const auto &e : fs::directory_iterator(root)) {
         if (e.is_directory()) {
            run_vector(e.path());
            ++vectors;
         }
      }
      sc_stop();
   }

   // Statistics read over the CSR socket while the frame is in flight: the
   // single zone/histogram memory shows the rows processed so far (DEC-30).
   // Vector stats_flat_channels_64x48: 16x12 zones of 3x3, flat R/Gr/Gb/B.
   static constexpr std::uint32_t block_status[] = {FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET, FX1_ISP_GTM_STATUS_OFFSET,
           FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_CNF_STATUS_OFFSET, FX1_ISP_RESIZER_STATUS_OFFSET};
   void probe_partial_stats(const std::string &name) {
      auto sel = [&](std::uint32_t off, std::uint32_t v, std::uint32_t data_off) {
         write32(off, v);
         return read32(data_off);
      };
      const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
      std::uint32_t gr = 0;
      while (sc_time_stamp() < end) {
         gr = sel(FX1_ISP_AEC_HIST_ADDR_OFFSET, 1000 >> 6, FX1_ISP_AEC_HIST_DATA_OFFSET);
         if (gr >= 2 * 32) {  // rows 0 and 2 are in: zone 0 is complete
            break;
         }
         wait(50, SC_NS);
      }
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK,
                       FX1_ISP_AEC_STATUS_BUSY_MASK, name + " probe inside the frame");
      FX1_CHECK_EQ_CTX(gr >= 64 && gr < 768 ? 1 : 0, 1, name + " partial histogram " + std::to_string(gr));
      write32(FX1_ISP_AEC_CHANNEL_SEL_OFFSET, 0);
      FX1_CHECK_EQ_CTX(sel(FX1_ISP_AEC_ZONE_ADDR_OFFSET, 0, FX1_ISP_AEC_ZONE_SUM_OFFSET), 400, name + " zone 0 partial");
      FX1_CHECK_EQ_CTX(sel(FX1_ISP_AEC_ZONE_ADDR_OFFSET, 191, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET), 4095,
                       name + " last zone not written yet");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_GLOBAL_COUNT_OFFSET), 0, name + " globals not yet published");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_FRAME_ID_OFFSET), 0, name + " FRAME_ID not yet published");
      for (std::uint32_t off : block_status) {  // DEC-38: every pixel block holds pixels
         FX1_CHECK_EQ_CTX(read32(off) & 1u, 1, name + " block busy mid-frame");
      }
      ++partial_probes;
   }

   void run_vector(const fs::path &dir) {
      const std::string name = dir.filename().string();
      const int warnings_before = sc_report_handler::get_count("fx1_isp/pipeline");
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      mem->log.clear();

      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      const std::uint32_t in_stride = align16(2 * in_w), y_stride = align16(out_w), uv_stride = align16(out_w);
      for (std::uint32_t y = 0; y < in_h; ++y) {
         std::memcpy(mem->at(in_addr + std::uint64_t{y} * in_stride), input.data() + 2u * y * in_w, 2u * in_w);
      }
      mem->fill(y_addr, std::size_t{y_stride} * out_h, 0xA5);
      mem->fill(uv_addr, std::size_t{uv_stride} * out_h / 2u, 0xA5);

      // HAS §9.1 order: profile (geometry, tuning, LUTs), ISP_EN, then the DMA.
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         write_named(l.reg, l.value);
      }
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      // The geometry software reads is the one the frame will have.
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_RESIZER_OUT_W_OFFSET), out_w, name + " RESIZER_OUT_W");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_RESIZER_OUT_H_OFFSET), out_h, name + " RESIZER_OUT_H");
      write32(FX1_ISP_IDMA_STRIDE_OFFSET, in_stride);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, y_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, uv_stride);
      write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET, static_cast<std::uint32_t>(in_addr));
      write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET, static_cast<std::uint32_t>(y_addr));
      write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET, static_cast<std::uint32_t>(uv_addr));
      // Rotation disabled for simplicity: every frame reuses buffer 0 of each engine,
      // recycled by software after each ODMA_DONE (HAS §6.25.10.2 order).
      for (unsigned k = 1; k < 4; ++k) {
         write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(in_addr));
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_addr));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_addr));
      }
      write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
      const std::vector<fx1_test::csr_line> between = fx1_test::read_frame_writes(dir / "frames.csrw");
      for (std::uint32_t f = 0; f < frames; ++f) {
         for (const fx1_test::csr_line &w : between) {  // software between frames, before the next SOF
            if (w.frame == f) {
               write_named(w.reg, w.value);
            }
         }
         const std::uint32_t bit = 1u << (f % 4u);
         write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, bit);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, bit);
         if (f == 0 && name == "stats_flat_channels_64x48") {
            probe_partial_stats(name);
         }
         const sc_time end = sc_time_stamp() + sc_time(100, SC_MS);
         while (dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) < f + 1 && sc_time_stamp() < end) {
            wait(10, SC_US);
         }
      }
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), frames, name + " frames done");
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_DMA_ERR_OFFSET) & 0x1Du, 0, name + " no DMA error");

      auto compare = [&](std::uint64_t base, std::uint32_t stride, std::uint32_t rows, const char *file) {
         const std::vector<std::uint8_t> want = read_file(dir / file);
         std::size_t bad = 0;
         for (std::uint32_t y = 0; y < rows; ++y) {
            const std::uint8_t *got = mem->at(base + std::uint64_t{y} * stride);
            for (std::uint32_t x = 0; x < stride; ++x) {
               const std::uint8_t exp = x < out_w ? want[std::size_t{y} * out_w + x] : 0xA5;  // padding untouched
               bad += got[x] != exp;
            }
         }
         FX1_CHECK_EQ_CTX(bad, 0, name + " " + file);
      };
      compare(y_addr, y_stride, out_h, "expected_y.bin");
      compare(uv_addr, uv_stride, out_h / 2u, "expected_uv.bin");
      // Published block counters and statistics, read by software over the
      // CSR port (SET lines program the readout selectors).
      std::string sel;
      for (const fx1_test::csr_line &l : fx1_test::read_expected(dir / "expected_stats.txt")) {
         if (l.set) {
            write_named(l.reg, l.value);
            sel = " @" + l.reg + "=" + std::to_string(l.value);
            continue;
         }
         bool found = false;
         for (std::size_t i = 0; i < csr::num_registers; ++i) {
            if (l.reg == csr::registers[i].name) {
               found = true;
               FX1_CHECK_EQ_CTX(read32(csr::registers[i].offset), l.value, name + " " + l.reg + sel);
            }
         }
         FX1_CHECK_EQ_CTX(found ? 1 : 0, 1, name + " unknown register " + l.reg);
      }
      for (std::uint32_t off : block_status) {  // DEC-38: idle after the last frame
         FX1_CHECK_EQ_CTX(read32(off) & 1u, 0, name + " block busy after the frames");
      }
      // Only DEC-28 (BPC dynamic requested) warns, once per frame (review M3-R4).
      const bool dyn = name.find("bpc_dynamic_requested") != std::string::npos;
      FX1_CHECK_EQ_CTX(sc_report_handler::get_count("fx1_isp/pipeline") - warnings_before,
                       dyn ? static_cast<int>(frames) : 0, name + " pipeline warnings");
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   if (argc < 2) {
      std::cerr << "usage: " << argv[0] << " <vectors dir>\n";
      return 2;
   }
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.root = argv[1];
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.dut = &dut;
   tb.mem = &mem;
   sc_start();
   FX1_CHECK(tb.vectors >= 32);
   FX1_CHECK_EQ(tb.partial_probes, 1);
   return fx1_test::summary("fx1_isp_test_e2e_tlm");
}
