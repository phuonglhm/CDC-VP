// SPDX-License-Identifier: Apache-2.0
// End-to-end check through the whole IP (DEC-03): the committed vectors are
// programmed over the CSR socket, the RAW frame is placed in RAM, the frame
// runs IDMA -> pipeline -> ODMA, and the NV12 planes in RAM must equal the
// independent Python reference bit for bit.
// Usage: fx1_isp_test_e2e_tlm <vectors dir>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>

#include "control/control_unit.h"
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "test_memory.h"
#include "vector_io.h"

using namespace sc_core;
using cdc::components::fx1_isp::control_unit;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::geo_input;
using cdc::components::fx1_isp::geometry;
using cdc::components::fx1_isp::fx1_isp_tlm;
namespace csr = cdc::components::fx1_isp::csr;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 24u << 20;
constexpr std::uint64_t in_addr = mem_base;
constexpr std::uint64_t y_addr = mem_base + 0x60'0000ull;
constexpr std::uint64_t uv_addr = mem_base + 0xA0'0000ull;
// CH1 NV12 planes and the CH2 RGB888 plane (MAS v1.07 §6.25.3.8).
constexpr std::uint64_t ch1_y_addr = mem_base + 0x100'0000ull;
constexpr std::uint64_t ch1_uv_addr = mem_base + 0x120'0000ull;
constexpr std::uint64_t ch2_addr = mem_base + 0x140'0000ull;

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
   std::size_t refused_below_minimum = 0;
   std::size_t channel_outputs = 0;  // CH1/CH2 planes compared in memory

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

   // Statistics read over the CSR socket while the frame is in flight: a
   // result becomes visible only when its bank is READY after the frame's
   // final sample and then leased (MAS v1.07 §9.8; DEC-30 superseded), so a
   // mid-frame probe sees the FILLING bank, nothing to acquire and zero
   // result words. Vector stats_flat_channels_64x48: 13x10 zones of 5x5.
   static constexpr std::uint32_t block_status[] = {FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET, FX1_ISP_GTM_STATUS_OFFSET,
           FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET, FX1_ISP_CNF_STATUS_OFFSET, FX1_ISP_RESIZER_STATUS_OFFSET};
   void probe_partial_stats(const std::string &name) {
      auto sel = [&](std::uint32_t off, std::uint32_t v, std::uint32_t data_off) {
         write32(off, v);
         return read32(data_off);
      };
      const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
      while (sc_time_stamp() < end && !(read32(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK)) {
         wait(50, SC_NS);
      }
      wait(dut->params().core_period * (64.0 * 12.0));  // about a quarter of the frame's 48 rows
      const std::uint32_t st = read32(FX1_ISP_TUNE_AEC_STATUS_OFFSET);
      FX1_CHECK_EQ_CTX(st & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK, FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK,
                       name + " probe inside the frame (FILLING)");
      FX1_CHECK_EQ_CTX(st & (FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_MASK | FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK), 0,
                       name + " nothing READY or leased mid-frame");
      const std::uint32_t err_before = read32(FX1_ISP_TUNE_ERROR_OFFSET);
      FX1_CHECK_EQ_CTX(sel(FX1_ISP_AEC_HIST_ADDR_OFFSET, 1000 >> 6, FX1_ISP_AEC_HIST_DATA_OFFSET), 0,
                       name + " no partial histogram");
      write32(FX1_ISP_AEC_CHANNEL_SEL_OFFSET, 0);
      FX1_CHECK_EQ_CTX(sel(FX1_ISP_AEC_ZONE_ADDR_OFFSET, 0, FX1_ISP_AEC_ZONE_SUM_OFFSET), 0, name + " no partial zone");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_GLOBAL_COUNT_OFFSET), 0, name + " globals not yet published");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_FRAME_ID_OFFSET), 0, name + " a result word reads zero without a lease");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_NO_LEASE_MASK,
                       FX1_ISP_TUNE_ERROR_NO_LEASE_MASK, name + " result read without a lease");
      if (!(err_before & FX1_ISP_TUNE_ERROR_NO_LEASE_MASK)) {
         write32(FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_NO_LEASE_MASK);  // leave the golden's state
      }
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
      // Wait for the Resizer to publish the derived set (MAS §6.20.4.3: at most
      // two derivations, 2 x 36 clocks, after the last change; §11.6 step 5).
      wait(dut->params().core_period * 80.0);
      // An active image below 32 x 32 is not a legal channel geometry (MAS v1.07
      // §6.20.1.1, GEOMETRY_STATUS.ERROR 1): the frame is never admitted.
      const std::uint32_t bayer = read32(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;
      const geometry act = control_unit::derive_active(in_w, in_h, bayer);
      if (act.width < 32 || act.height < 32) {
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET),
                          unsigned{geo_input} << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT, name + " ERROR 1");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_INVALID_MASK,
                          FX1_ISP_TUNE_ERROR_INVALID_MASK, name + " sticky INVALID");
         write32(FX1_ISP_IDMA_STRIDE_OFFSET, in_stride);
         write32(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET, static_cast<std::uint32_t>(in_addr));
         write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, y_stride);
         write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, uv_stride);
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET, static_cast<std::uint32_t>(y_addr));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET, static_cast<std::uint32_t>(uv_addr));
         write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1);
         wait(1, SC_MS);
         FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0, name + " not admitted");
         FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, name + " no output");
         ++refused_below_minimum;
         return;
      }
      // The geometry software reads is the one the frame will have.
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET), FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK,
                       name + " GEOMETRY_STATUS");
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
      // CH1 / CH2 (vectors with channels.txt): their own buffers and strides.
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
         for (const extra_channel &x : extra) {
            write32(x.ch == 1 ? FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET : FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET, 0xF);
            write32(x.ch == 1 ? FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET : FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, bit);
         }
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
      // With a channel crop the output can complete before the shared IQ has
      // consumed the rows below the crop: the statistics publish and the
      // blocks go idle when the frame retires (DEC-38, MAS §9.8).
      const sc_time idle_by = sc_time_stamp() + sc_time(10, SC_MS);
      while ((read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < idle_by) {
         wait(1, SC_US);
      }
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
      // CH1 / CH2 in memory (MAS v1.07 §6.25.3.8): DONE, frame count, the
      // published ingress frame ID, and the bytes (CH2: R, G, B, 3W per row).
      auto compare_plane = [&](std::uint64_t base, std::uint32_t stride, std::uint32_t row_bytes, std::uint32_t rows,
                               const std::string &file) {
         const std::vector<std::uint8_t> want = read_file(dir / file);
         std::size_t bad = 0;
         for (std::uint32_t y = 0; y < rows; ++y) {
            const std::uint8_t *got = mem->at(base + std::uint64_t{y} * stride);
            for (std::uint32_t x = 0; x < stride; ++x) {
               bad += got[x] != (x < row_bytes ? want[std::size_t{y} * row_bytes + x] : 0xA5);
            }
         }
         FX1_CHECK_EQ_CTX(bad, 0, name + " " + file);
      };
      for (const extra_channel &x : extra) {
         const std::string c = " CH" + std::to_string(x.ch);
         const std::uint32_t count = x.ch == 1 ? FX1_ISP_CH1_ODMA_FRAME_COUNT_OFFSET : FX1_ISP_CH2_ODMA_FRAME_COUNT_OFFSET;
         const std::uint32_t frame0 = x.ch == 1 ? FX1_ISP_TUNE_CH1_BUF0_FRAME_OFFSET : FX1_ISP_TUNE_CH2_BUF0_FRAME_OFFSET;
         FX1_CHECK_EQ_CTX(read32(count), frames, name + c + " frame count");
         FX1_CHECK_EQ_CTX(read32(frame0 + 4u * ((frames - 1) % 4u)), read32(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET + 4u * ((frames - 1) % 4u)),
                          name + c + " ingress frame ID equals CH0's");
         if (x.ch == 1) {
            compare_plane(ch1_y_addr, x.stride, x.w, x.h, "expected_ch1_y.bin");
            compare_plane(ch1_uv_addr, x.stride, x.w, x.h / 2u, "expected_ch1_uv.bin");
         } else {
            compare_plane(ch2_addr, x.stride, 3 * x.w, x.h, "expected_ch2_rgb.bin");
         }
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET) & (1u << x.ch), 1u << x.ch, name + c + " OUTPUT DONE");
         ++channel_outputs;
      }
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
      // Nothing in the vectors warns (review M3-R4); BPC dynamic detection
      // has no effect and no longer warns (MAS §6.9, DEC-28 superseded).
      FX1_CHECK_EQ_CTX(sc_report_handler::get_count("fx1_isp/pipeline") - warnings_before, 0,
                       name + " pipeline warnings");
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
   FX1_CHECK_EQ(tb.refused_below_minimum, 5);  // four edge vectors and blc_profile2_bggr_64x32
   FX1_CHECK_EQ(tb.channel_outputs, 6);         // channels_ai (CH1, CH2) and the four CH2 RGB vectors
   return fx1_test::summary("fx1_isp_test_e2e_tlm");
}
