// SPDX-License-Identifier: Apache-2.0
// VP direct CSI->ISP path, end to end (D1a): every committed vector is
// programmed over the CSR socket as in test_e2e_tlm, but the RAW frame comes
// from a synthetic AXI4-Stream source (RAW16 beats, CSI-2 RX sys_clk grid)
// into the stream ingress instead of the IDMA: ISP_INGRESS.CTRL.SOURCE =
// direct, DMA_CTRL.IDMA_EN = 0. The NV12 planes in RAM and the statistics must
// equal the independent Python reference bit for bit, as for the memory path.
// Also: no IDMA transaction, IDMA counter or interrupt; every frame's context
// entry is OK with TAG = ODMA BUF_TAG and FRAME_ID = BUF_TS (P29); vectors
// whose geometry the ISP refuses are dropped at the SOF (NO_CREDIT).
// Usage: fx1_isp_test_direct_e2e_tlm <vectors dir>

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
#include "vector_io.h"

#define FX1_CHECK_CTX(cond, msg) FX1_CHECK_EQ_CTX((cond) ? 1 : 0, 1, msg)

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

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   tlm_utils::simple_initiator_socket<bench> ing_socket;
   sc_out<bool> rst_n;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fx1_test::axis_source *src = nullptr;
   fs::path root;
   std::size_t vectors = 0, frames_total = 0, refused_below_minimum = 0, channel_outputs = 0, stats_linked = 0;
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
      stats_association(root / "stats_flat_channels_64x48");
      // Never an IDMA transaction or completion in the whole run.
      // (the statistics case above runs direct too)
      FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_READ_COMMAND, true), 0, "IDMA reads");
      sc_stop();
   }

   // D1b: the statistics of a frame carry the ISP frame ID that its context
   // entry reports (CTX_ISP_FRAME): the ISP publishes statistics for a frame
   // whose capture the CSI later reports DROPPED, and firmware uses the link
   // to discard them. Frame 1 DROPPED after its last line, frame 2 OK.
   void stats_association(const fs::path &dir) {
      const std::string name = dir.filename().string() + " statistics link";
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      std::vector<std::uint16_t> px(std::size_t{in_w} * in_h);
      for (std::size_t i = 0; i < px.size(); ++i) {
         px[i] = static_cast<std::uint16_t>(input[2 * i] | (input[2 * i + 1] << 8));
      }
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         write_named(l.reg, l.value);
      }
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F02);
      iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      const std::uint32_t stride = align16(out_w);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, stride);
      for (unsigned k = 0; k < 4; ++k) {
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_addr));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_addr));
      }
      for (std::uint32_t f = 0; f < 2; ++f) {
         const bool dropped = f == 0;
         write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xF);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << f);
         ++seq;
         src->frame(px, in_w, in_h, seq, static_cast<std::uint16_t>(seq & 0x3FFF), 8, true,
                    dropped ? cdc::components::fx1_isp::capture_outcome::dropped
                            : cdc::components::fx1_isp::capture_outcome::ok);
         src->wait_done();
         const sc_time end = sc_time_stamp() + sc_time(50, SC_MS);
         while ((iread(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK) == 0 &&
                sc_time_stamp() < end) {
            wait(10, SC_US);
         }
         const std::string fc = name + (dropped ? ", DROPPED frame" : ", OK frame");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET) & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK,
                          dropped ? FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED : FX1_ISP_INGRESS_OUTCOME_OK, fc + " outcome");
         const std::uint32_t isp_frame = iread(FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET);
         FX1_CHECK_CTX(isp_frame != 0, fc + " ISP_FRAME set");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET + 4u * f), isp_frame, fc + " ISP_FRAME = BUFn_FRAME");
         iwrite(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
         // The frame's AEC result, under a lease: published, with that frame ID.
         const sc_time idle_by = sc_time_stamp() + sc_time(10, SC_MS);
         while ((read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < idle_by) {
            wait(1, SC_US);
         }
         write32(FX1_ISP_TUNE_AEC_CMD_OFFSET, FX1_ISP_TUNE_AEC_CMD_ACQUIRE_MASK);
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_AEC_FRAME_ID_OFFSET), isp_frame, fc + " AEC_FRAME_ID = ISP_FRAME");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_TUNE_AEC_FRAME_OFFSET), isp_frame, fc + " TUNE_AEC_FRAME = ISP_FRAME");
         write32(FX1_ISP_TUNE_AEC_CMD_OFFSET, FX1_ISP_TUNE_AEC_CMD_RELEASE_MASK);
         ++stats_linked;
      }
      iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, 0);
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
      std::vector<std::uint16_t> px(std::size_t{in_w} * in_h);
      for (std::size_t i = 0; i < px.size(); ++i) {
         px[i] = static_cast<std::uint16_t>(input[2 * i] | (input[2 * i + 1] << 8));
      }
      const std::uint32_t y_stride = align16(out_w), uv_stride = align16(out_w);
      mem->fill(y_addr, std::size_t{y_stride} * out_h, 0xA5);
      mem->fill(uv_addr, std::size_t{uv_stride} * out_h / 2u, 0xA5);

      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         write_named(l.reg, l.value);
      }
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      // Direct: ODMA only (IDMA_EN = 0), then the source switch.
      write32(FX1_ISP_DMA_CTRL_OFFSET, 0x3F02);
      iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, FX1_ISP_INGRESS_CTRL_SOURCE_MASK);
      FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_STATUS_OFFSET) &
                           (FX1_ISP_INGRESS_STATUS_DIRECT_MASK | FX1_ISP_INGRESS_STATUS_STICKY_MASK),
                       FX1_ISP_INGRESS_STATUS_DIRECT_MASK, name + " direct selected");

      const std::uint32_t bayer = read32(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;
      const geometry act = control_unit::derive_active(in_w, in_h, bayer);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, y_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, uv_stride);
      for (unsigned k = 0; k < 4; ++k) {
         write32(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(y_addr));
         write32(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, static_cast<std::uint32_t>(uv_addr));
      }
      if (act.width < 32 || act.height < 32) {
         // Refused geometry (GEOMETRY_STATUS.ERROR): the ISP would not admit
         // the frame; the ingress drops it at the SOF, TREADY held 1.
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1);
         const std::uint32_t drops = iread(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET);
         const std::uint64_t refused = src->refused;
         ++seq;
         src->frame(px, in_w, in_h, seq, static_cast<std::uint16_t>(seq & 0x3FFF));
         src->wait_done();
         wait(1, SC_MS);
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET), drops + 1, name + " dropped at the SOF");
         FX1_CHECK_EQ_CTX(src->refused, refused, name + " TREADY held 1 while dropping");
         FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, name + " no output");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_COUNT_OFFSET), 0, name + " no context entry kept");
         iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, 0);
         ++refused_below_minimum;
         return;
      }
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_RESIZER_OUT_W_OFFSET), out_w, name + " RESIZER_OUT_W");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_RESIZER_OUT_H_OFFSET), out_h, name + " RESIZER_OUT_H");
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
         const std::uint16_t fid = static_cast<std::uint16_t>(((seq + 1) * 7u) & 0x3FFFu);
         src->frame(px, in_w, in_h, ++seq, fid);
         src->wait_done();
         const sc_time end = sc_time_stamp() + sc_time(100, SC_MS);
         while ((dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) < f + 1 ||
                 (iread(FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK) == 0) &&
                sc_time_stamp() < end) {
            wait(10, SC_US);
         }
         // The frame's context: OK, its own capture, TAG = BUF_TAG, FRAME_ID = BUF_TS.
         const std::string fc = name + " frame " + std::to_string(f);
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET), FX1_ISP_INGRESS_OUTCOME_OK, fc + " outcome OK");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET), static_cast<std::uint32_t>(seq), fc + " seq");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_FRAME_OFFSET) & FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK, fid,
                          fc + " FRAME_ID");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_TS_LO_OFFSET), static_cast<std::uint32_t>(1000u * seq), fc + " TS");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_LINES_OFFSET), in_h, fc + " lines");
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_SAMPLES_OFFSET), in_w * in_h, fc + " samples");
         const std::uint32_t head = f % 4u;
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * head), iread(FX1_ISP_INGRESS_CTX_TAG_OFFSET),
                          fc + " BUF_TAG = context TAG");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * head), fid, fc + " BUF_TS = FRAME_ID");
         // CTX_ISP_FRAME: the ISP frame ID published with the buffer (and with the statistics).
         FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET),
                          read32(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET + 4u * head), fc + " ISP_FRAME = BUFn_FRAME");
         FX1_CHECK_CTX(iread(FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET) != 0, fc + " ISP_FRAME set");
         iwrite(FX1_ISP_INGRESS_CTX_POP_OFFSET, 1);
         ++frames_total;
      }
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), frames, name + " frames done");
      const sc_time idle_by = sc_time_stamp() + sc_time(10, SC_MS);
      while ((read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < idle_by) {
         wait(1, SC_US);
      }
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_DMA_ERR_OFFSET) & 0x1Du, 0, name + " no DMA error");
      // No IDMA activity is reported or faked (contract §6).
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0, name + " IDMA_FRAME_COUNT");
      FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) &
                           (FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_START_BIT | FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_DONE_BIT |
                            FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT),
                       0, name + " no IDMA interrupt");

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
      compare_plane(y_addr, y_stride, out_w, out_h, "expected_y.bin");
      compare_plane(uv_addr, uv_stride, out_w, out_h / 2u, "expected_uv.bin");
      for (const extra_channel &x : extra) {
         if (x.ch == 1) {
            compare_plane(ch1_y_addr, x.stride, x.w, x.h, "expected_ch1_y.bin");
            compare_plane(ch1_uv_addr, x.stride, x.w, x.h / 2u, "expected_ch1_uv.bin");
         } else {
            compare_plane(ch2_addr, x.stride, 3 * x.w, x.h, "expected_ch2_rgb.bin");
         }
         ++channel_outputs;
      }
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
      FX1_CHECK_EQ_CTX(sc_report_handler::get_count("fx1_isp/pipeline") - warnings_before, 0,
                       name + " pipeline warnings");
      FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET), 0, name + " no invalid frame");
      iwrite(FX1_ISP_INGRESS_CTRL_OFFSET, 0);  // back to memory: idle and drained
      FX1_CHECK_EQ_CTX(iread(FX1_ISP_INGRESS_STATUS_OFFSET) & FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK, 0,
                       name + " switch back accepted");
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
   FX1_CHECK(tb.vectors >= 32);
   FX1_CHECK_EQ(tb.refused_below_minimum, 5);
   FX1_CHECK_EQ(tb.channel_outputs, 6);
   FX1_CHECK_EQ(tb.stats_linked, 2u);
   std::printf("direct e2e: %zu vectors, %zu frames\n", tb.vectors, tb.frames_total);
   return fx1_test::summary("fx1_isp_test_direct_e2e_tlm");
}
