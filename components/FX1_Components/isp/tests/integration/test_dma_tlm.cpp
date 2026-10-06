// SPDX-License-Identifier: Apache-2.0
// Standalone SystemC/TLM testbench of the FX1 ISP DMA engines and frame
// lifecycle (M2). A TLM test master stands in for the CPU; a test RAM with
// latency, fault injection and an access log serves both DMA masters.
//
// Pixel content comes from the M2 datapath stub (DEC-21): Y = cropped sample
// >> 4 with nearest-neighbour scaling, UV = 128. Expected output below is
// computed from that definition, independently of the model code.

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "test_memory.h"

using namespace sc_core;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
using fx1_test::test_memory;

namespace {

constexpr std::uint64_t mem_base = 0x12'0000'0000ull;  // above 4 GB, inside 40 bits
constexpr std::size_t mem_size = 96u << 20;

std::uint64_t in_buf(unsigned k) { return mem_base + k * 0x90'0000ull; }
std::uint64_t out_y(unsigned k) { return mem_base + 0x280'0000ull + k * 0xA0'0000ull; }
std::uint64_t out_uv(unsigned k) { return out_y(k) + 0x50'0000ull; }

constexpr std::uint8_t guard = 0xA5;

// DMA_IRQ_STAT bits (CSR rows 460-461).
constexpr std::uint32_t irq_idma_start = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_START_BIT;
constexpr std::uint32_t irq_idma_done = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_DONE_BIT;
constexpr std::uint32_t irq_idma_underrun = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t irq_odma_done = FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT;
constexpr std::uint32_t irq_odma_overflow = FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t irq_axi_error = FX1_ISP_DMA_IRQ_STAT_IRQ_AXI_ERROR_BIT;
// DMA_ERR bits (CSR row 457).
constexpr std::uint32_t err_align = FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT;
constexpr std::uint32_t err_underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t err_overflow = FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t err_idma_axi = FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT;
constexpr std::uint32_t err_odma_axi = FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT;

struct frame_cfg {
   std::uint32_t width = 64;
   std::uint32_t height = 32;
   std::uint32_t pattern = FX1_ISP_COMMON_BAYER_PATTERN_RGGB;
   std::uint32_t resizer_ctrl = 0;
   std::uint32_t in_stride = 256;  // > 2W: padding is never read
   std::uint32_t y_stride = 96;    // > W: padding must stay untouched
   std::uint32_t uv_stride = 80;
   std::uint32_t max_burst_m1 = 63;
};

// Output geometry per HAS Table 6-17 and Table 6-67 (RGGB/GRBG/GBRG/BGGR crop,
// then the resizer target if it fits).
struct geom {
   std::uint32_t aw, ah, ow, oh, sh, sv;
};
geom expected_geometry(const frame_cfg &c) {
   geom g{};
   g.sh = c.pattern & 1u;
   g.sv = (c.pattern >> 1) & 1u;
   g.aw = (c.width - g.sh) / 2u * 2u;
   g.ah = (c.height - g.sv) / 2u * 2u;
   g.ow = g.aw;
   g.oh = g.ah;
   static const std::uint32_t modes[16][2] = {{0, 0},       {3840, 2160}, {2560, 1440}, {2880, 1620},
                                              {2304, 1296}, {1920, 1080}, {1280, 720},  {960, 540},
                                              {640, 360},   {2592, 1944}, {2048, 1536}, {1600, 1200},
                                              {1280, 960},  {800, 600},   {640, 480},   {0, 0}};
   const std::uint32_t scale = (c.resizer_ctrl >> 3) & 0xFu;
   if ((c.resizer_ctrl & 1u) && modes[scale][0] != 0 && modes[scale][0] <= g.aw && modes[scale][1] <= g.ah) {
      g.ow = modes[scale][0];
      g.oh = modes[scale][1];
   }
   return g;
}

std::uint16_t sample(unsigned tag, std::uint32_t x, std::uint32_t y) {
   return static_cast<std::uint16_t>((x * 7u + y * 13u + tag * 512u) & 0xFFFu);
}

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   test_memory *mem = nullptr;
   std::vector<std::pair<sc_time, bool>> irq_edges;
   bool finished = false;
   bool slow_pipeline = false;  // DUT built with a very long line latency
   static int &running() {
      static int n = 0;
      return n;
   }

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") {
      ++running();
      SC_THREAD(run);
      SC_METHOD(on_irq);
      sensitive << irq;
      dont_initialize();
   }

   void on_irq() { irq_edges.emplace_back(sc_time_stamp(), irq.read()); }

   // ---- register access -------------------------------------------------------
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
   void set64(std::uint32_t lo_offset, std::uint64_t addr) {
      write32(lo_offset, static_cast<std::uint32_t>(addr));
      write32(lo_offset + 4u, static_cast<std::uint32_t>(addr >> 32));
   }
   bool wait_until(const std::function<bool()> &cond, const sc_time &timeout, const sc_time &step = sc_time(1, SC_US)) {
      const sc_time end = sc_time_stamp() + timeout;
      while (!cond()) {
         if (sc_time_stamp() >= end) {
            return false;
         }
         wait(step);
      }
      return true;
   }
   std::uint32_t peek(std::uint32_t off) { return dut->debug_peek(off); }

   // ---- setup --------------------------------------------------------------------
   void hard_reset() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      mem->log.clear();
      mem->faults.clear();
      mem->latency = SC_ZERO_TIME;
      mem->blocking = false;
   }

   // HAS §9.1 bring-up order: geometry, tuning, ISP_EN, then the DMA.
   void program(const frame_cfg &c, bool isp_en = true) {
      write32(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, c.width);
      write32(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, c.height);
      write32(FX1_ISP_COMMON_BAYER_OFFSET, c.pattern);
      write32(FX1_ISP_RESIZER_CTRL_OFFSET, c.resizer_ctrl);
      write32(FX1_ISP_IDMA_STRIDE_OFFSET, c.in_stride);
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, c.y_stride);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, c.uv_stride);
      for (unsigned k = 0; k < 4; ++k) {
         set64(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, in_buf(k));
         set64(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, out_y(k));
         set64(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, out_uv(k));
      }
      if (isp_en) {
         write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      }
   }
   void enable_dma(const frame_cfg &c, bool idma = true, bool odma = true) {
      write32(FX1_ISP_DMA_CTRL_OFFSET, (c.max_burst_m1 << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) |
                                           (idma ? FX1_ISP_DMA_CTRL_IDMA_EN_MASK : 0u) |
                                           (odma ? FX1_ISP_DMA_CTRL_ODMA_EN_MASK : 0u));
   }

   void fill_input(unsigned buf, unsigned tag, const frame_cfg &c) {
      for (std::uint32_t y = 0; y < c.height; ++y) {
         for (std::uint32_t x = 0; x < c.width; ++x) {
            mem->put16(in_buf(buf) + std::uint64_t{y} * c.in_stride + 2u * x, sample(tag, x, y));
         }
      }
   }
   void guard_output(unsigned buf, const frame_cfg &c) {
      const geom g = expected_geometry(c);
      mem->fill(out_y(buf), std::size_t{c.y_stride} * g.oh + 64u, guard);
      mem->fill(out_uv(buf), std::size_t{c.uv_stride} * (g.oh / 2u) + 64u, guard);
   }

   // Returns the number of wrong bytes (content, padding and trailing guard).
   std::size_t check_output(unsigned buf, unsigned tag, const frame_cfg &c) {
      const geom g = expected_geometry(c);
      std::size_t bad = 0;
      for (std::uint32_t oy = 0; oy < g.oh; ++oy) {
         const std::uint32_t iy = static_cast<std::uint32_t>(std::uint64_t{oy} * g.ah / g.oh);
         const std::uint8_t *row = mem->at(out_y(buf) + std::uint64_t{oy} * c.y_stride);
         for (std::uint32_t ox = 0; ox < c.y_stride; ++ox) {
            std::uint8_t want = guard;
            if (ox < g.ow) {
               const std::uint32_t ix = static_cast<std::uint32_t>(std::uint64_t{ox} * g.aw / g.ow);
               want = static_cast<std::uint8_t>(sample(tag, ix + g.sh, iy + g.sv) >> 4);
            }
            bad += row[ox] != want;
         }
      }
      for (std::uint32_t j = 0; j < g.oh / 2u; ++j) {
         const std::uint8_t *row = mem->at(out_uv(buf) + std::uint64_t{j} * c.uv_stride);
         for (std::uint32_t i = 0; i < c.uv_stride; ++i) {
            bad += row[i] != (i < g.ow ? 128u : guard);
         }
      }
      for (std::uint32_t i = 0; i < 64; ++i) {
         bad += *mem->at(out_y(buf) + std::uint64_t{c.y_stride} * g.oh + i) != guard;
         bad += *mem->at(out_uv(buf) + std::uint64_t{c.uv_stride} * (g.oh / 2u) + i) != guard;
      }
      return bad;
   }

   bool wait_frames(std::uint32_t odma_count, const sc_time &timeout = sc_time(50, SC_MS)) {
      return wait_until([&] { return peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) >= odma_count; }, timeout);
   }

   // ---- tests ---------------------------------------------------------------------
   void run() {
      hard_reset();
      if (slow_pipeline) {
         test_reset_wakes_line_wait();
      } else {
         run_all();
      }
      finished = true;
      if (--running() == 0) {
         sc_stop();
      }
   }

   void run_all() {
      test_overlapping_frames_busy();
      test_single_frame();
      test_real_geometry_bggr();
      test_resizer_scaling();
      test_rotation_and_recycle();
      test_underrun();
      test_overflow_lossless();
      test_alignment_rejection();
      test_isp_en_trap();
      // Every row boundary, including the last UV/Y rows, must recover.
      for (unsigned row = 0; row < 32; ++row) test_axi_read_error(row);
      for (unsigned row = 0; row < 32; ++row) test_axi_write_error(row, false);
      for (unsigned row = 0; row < 16; ++row) test_axi_write_error(row, true);
      test_soft_reset_mid_frame();
      test_external_reset_mid_frame();
      test_address_width();
      test_burst_formation();
      test_idma_disable_at_frame_boundary();
      test_odma_disabled_after_arm();
      // Audit item 1: a reset while the target holds the engine inside b_transport.
      for (const bool idma_side : {true, false}) {
         for (const bool external : {false, true}) {
            test_reset_inside_blocking_target(idma_side, external);
         }
      }
   }

   // The engine thread is suspended inside the target's b_transport when the
   // reset arrives, so the reset event cannot wake it. When the target
   // returns, the transfer belongs to an old epoch: its data and response must
   // be ignored, no further access may start, and no completion may appear.
   void test_reset_inside_blocking_target(bool idma_side, bool external) {
      const std::string ctx = std::string(idma_side ? "IDMA" : "ODMA") + " blocked, " +
                              (external ? "i_rst_n" : "soft reset");
      hard_reset();
      frame_cfg c;
      fill_input(0, 40, c);
      guard_output(0, c);
      program(c);
      mem->latency = sc_time(5, SC_US);
      mem->blocking = true;
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      unsigned &active = idma_side ? mem->active_idma : mem->active_odma;
      FX1_CHECK_EQ_CTX(wait_until([&] { return active > 0; }, sc_time(5, SC_MS), sc_time(100, SC_NS)) ? 1 : 0, 1, ctx + ": transfer inside the target");
      const sc_time tr = sc_time_stamp();
      if (external) {
         rst_n.write(false);
         wait(1, SC_US);
         rst_n.write(true);
      } else {
         write32(FX1_ISP_DMA_CTRL_OFFSET, peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      }
      wait(200, SC_US);  // the blocked transfer returns after 5 us; nothing may follow it
      FX1_CHECK_EQ_CTX(mem->active_idma + mem->active_odma, 0, ctx + ": target released");
      FX1_CHECK_EQ_CTX(no_access_after(tr) ? 1 : 0, 1, ctx + ": no access starts after the reset");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, ctx + ": no completion");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0, ctx + ": no DONE");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0, ctx + ": no input retired");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_ERR_OFFSET) & ~err_underrun, 0, ctx + ": no error from the stale response");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0, ctx + ": idle");
      // Recovery: same frame again, with an annotated (non-blocking) target.
      mem->blocking = false;
      mem->latency = sc_time(20, SC_NS);
      mem->log.clear();
      guard_output(0, c);
      if (external) {
         program(c);
      }
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      if (external) {
         enable_dma(c);
      }
      FX1_CHECK_EQ_CTX(wait_frames(1) ? 1 : 0, 1, ctx + ": retried frame completes");
      FX1_CHECK_EQ_CTX(check_output(0, 40, c), 0, ctx + ": retried frame content");
   }

   // A retiring ODMA frame must not make a younger accepted frame look idle.
   void test_overlapping_frames_busy() {
      hard_reset();
      frame_cfg c;
      mem->latency = sc_time(1, SC_US);
      for (unsigned k = 0; k < 2; ++k) {
         fill_input(k, 30 + k, c);
         guard_output(k, c);
      }
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1); // frame 2 stalls without a buffer
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 3);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      wait(20, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 1);
      FX1_CHECK_EQ(peek(FX1_ISP_DEMOSAIC_STATUS_OFFSET) & 1u, 1);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK,
                   FX1_ISP_COMMON_STATUS_BUSY_MASK);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 2);
      FX1_CHECK(wait_frames(2));
      FX1_CHECK_EQ(check_output(0, 30, c), 0);
      FX1_CHECK_EQ(check_output(1, 31, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
   }

   void test_single_frame() {
      hard_reset();
      frame_cfg c;
      mem->latency = sc_time(30, SC_NS);
      fill_input(0, 1, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_FRAME_START_MASK);
      write32(FX1_ISP_DMA_IRQ_EN_OFFSET, irq_odma_done);  // pin follows ODMA_DONE only
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      const std::size_t e0 = irq_edges.size();
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      settle();
      FX1_CHECK_EQ(check_output(0, 1, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 1);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);  // SPEC-07: cleared when full
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 1);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & (irq_idma_start | irq_idma_done | irq_odma_done),
                   irq_idma_start | irq_idma_done | irq_odma_done);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK,
                   FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_STATUS_OFFSET) & 0x3u, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_FRAME_START_MASK, 0);  // DEC-16
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_STAT_OFFSET), 0);
      FX1_CHECK_EQ(dut->debug_idma_head(), 1);
      FX1_CHECK_EQ(dut->debug_odma_head(), 1);
      // Only active bytes are written: W per luma line, W per chroma line.
      FX1_CHECK_EQ(mem->bytes_written(), 64u * 32u + 64u * 16u);
      // DONE is raised only after the last write response has returned.
      sc_time last_write = SC_ZERO_TIME;
      for (const auto &a : mem->log) {
         if (a.cmd == tlm::TLM_WRITE_COMMAND && a.end > last_write) {
            last_write = a.end;
         }
      }
      FX1_CHECK(irq_edges.size() > e0);
      if (irq_edges.size() > e0) {
         FX1_CHECK(irq_edges[e0].second);
         FX1_CHECK(irq_edges[e0].first >= last_write);
      }
      FX1_CHECK(irq.read());
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, irq_odma_done);
      settle();
      FX1_CHECK(!irq.read());
   }

   void test_real_geometry_bggr() {
      // DEC-04 input format: BGGR 2688x1520, 16-bit container, stride 5376.
      hard_reset();
      frame_cfg c;
      c.width = 2688;
      c.height = 1520;
      c.pattern = FX1_ISP_COMMON_BAYER_PATTERN_BGGR;
      c.in_stride = 5376;
      c.y_stride = 2688;
      c.uv_stride = 2688;
      fill_input(0, 2, c);
      guard_output(0, c);
      program(c);
      FX1_CHECK_EQ(read32(FX1_ISP_RESIZER_OUT_W_OFFSET), 2686);
      FX1_CHECK_EQ(read32(FX1_ISP_RESIZER_OUT_H_OFFSET), 1518);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 2, c), 0);
      FX1_CHECK_EQ(mem->bytes_written(), 2686u * 1518u + 2686u * 759u);
   }

   void test_resizer_scaling() {
      hard_reset();
      frame_cfg c;
      c.width = 1280;
      c.height = 720;
      c.in_stride = 2560;
      c.resizer_ctrl = 1u | FX1_ISP_RESIZER_CTRL_UPDATED_MASK |
                       (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);
      c.y_stride = 640;
      c.uv_stride = 656;
      fill_input(0, 3, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 3, c), 0);
   }

   void test_rotation_and_recycle() {
      hard_reset();
      frame_cfg c;
      for (unsigned k = 0; k < 4; ++k) {
         fill_input(k, 10 + k, c);
         guard_output(k, c);
      }
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0xF);
      enable_dma(c);
      FX1_CHECK(wait_frames(4));
      for (unsigned k = 0; k < 4; ++k) {
         FX1_CHECK_EQ_CTX(check_output(k, 10 + k, c), 0, "buffer " + std::to_string(k));
      }
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0xF);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);
      // HAS §6.25.10.2 recycle: clear DONE, then set FREE; refill and re-arm input.
      write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x3);
      guard_output(0, c);
      guard_output(1, c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3);
      fill_input(0, 20, c);
      fill_input(1, 21, c);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      FX1_CHECK(wait_frames(6));
      FX1_CHECK_EQ(check_output(0, 20, c), 0);
      FX1_CHECK_EQ(check_output(1, 21, c), 0);
      FX1_CHECK_EQ(check_output(2, 12, c), 0);  // untouched
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 6);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0xF);
      FX1_CHECK_EQ(dut->debug_idma_head(), 2);
      FX1_CHECK_EQ(dut->debug_odma_head(), 2);
   }

   void test_underrun() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 4, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      enable_dma(c);  // IDMA enabled, head buffer not valid
      settle();
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_underrun, irq_idma_underrun);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_ERROR_MASK,
                   FX1_ISP_COMMON_STATUS_ERROR_MASK);  // DEC-15
      write32(FX1_ISP_DMA_ERR_OFFSET, err_underrun);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun);  // level held
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, irq_idma_underrun);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 4, c), 0);
      // Idle again with the next head invalid: a new rising edge.
      settle();
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_underrun, irq_idma_underrun);
      // Disabling the IDMA drops the condition; the sticky bit can then clear.
      enable_dma(c, false, true);
      write32(FX1_ISP_DMA_ERR_OFFSET, err_underrun);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, 0);
   }

   void test_overflow_lossless() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 5, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);  // ODMA enabled, no free buffer
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow) != 0; }, sc_time(1, SC_MS)));
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_overflow, irq_odma_overflow);
      wait(200, SC_US);
      // Back-pressure reaches the IDMA: its frame cannot complete.
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK, FX1_ISP_DMA_STAT_IDMA_BUSY_MASK);
      // Reported once per stall episode.
      write32(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, irq_odma_overflow);
      enable_dma(c);  // make the ODMA re-evaluate while still stalled
      write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x1);
      wait(100, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_overflow, 0);
      // Freeing a buffer resumes the frame without loss (DEC-18).
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 5, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 1);
   }

   void test_alignment_rejection() {
      struct bad_case {
         const char *name;
         bool input;  // which unit must refuse
         std::function<void(frame_cfg &)> cfg;
         std::function<void()> regs;
      };
      const std::vector<bad_case> cases = {
         {"idma base %B", true, nullptr, [&] { set64(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET, in_buf(0) + 8); }},
         {"idma stride %B", true, [](frame_cfg &c) { c.in_stride = 248; }, nullptr},
         {"idma stride < 2W", true, [](frame_cfg &c) { c.in_stride = 112; }, nullptr},
         {"odd width", true, [](frame_cfg &c) { c.width = 63; }, nullptr},
         {"odd height", true, [](frame_cfg &c) { c.height = 31; }, nullptr},
         {"width > 3840", true, [](frame_cfg &c) { c.width = 3842; c.in_stride = 7696; c.y_stride = 3856; c.uv_stride = 3856; }, nullptr},
         {"height > 2160", true, [](frame_cfg &c) { c.height = 2162; }, nullptr},
         {"odma y base %B", false, nullptr, [&] { set64(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET, out_y(0) + 8); }},
         {"odma uv base %B", false, nullptr, [&] { set64(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET, out_uv(0) + 4); }},
         {"odma y stride < W", false, [](frame_cfg &c) { c.y_stride = 48; }, nullptr},
         {"odma uv stride %B", false, [](frame_cfg &c) { c.uv_stride = 72; }, nullptr},
      };
      for (const bad_case &bc : cases) {
         hard_reset();
         frame_cfg c;
         if (bc.cfg) {
            bc.cfg(c);
         }
         program(c);
         if (bc.regs) {
            bc.regs();
         }
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         enable_dma(c);
         wait(100, SC_US);
         const std::string ctx = bc.name;
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_DMA_ERR_OFFSET) & err_align, err_align, ctx);
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK,
                          FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, ctx);
         if (bc.input) {
            FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_READ_COMMAND, true), 0, ctx + ": nothing read");
            FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_start, 0, ctx + ": did not start");
         }
         FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_WRITE_COMMAND, false), 0, ctx + ": nothing written");
         FX1_CHECK_EQ_CTX(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, ctx);
      }
   }

   void test_isp_en_trap() {
      // DEC-20: arming the ODMA before ISP_EN captures no geometry.
      hard_reset();
      frame_cfg c;
      fill_input(0, 6, c);
      guard_output(0, c);
      program(c, false);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      enable_dma(c, false, true);
      settle();
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_align, err_align);
      // Recovery in the HAS order: ISP_EN, clear the error, re-arm.
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      write32(FX1_ISP_DMA_ERR_OFFSET, err_align);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 6, c), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_align, 0);
   }

   void test_axi_read_error(unsigned failed_row) {
      hard_reset();
      frame_cfg c;
      fill_input(0, 7, c);
      guard_output(0, c);
      program(c);
      const std::uint64_t addr = in_buf(0) + failed_row * c.in_stride;
      mem->faults.push_back({tlm::TLM_READ_COMMAND, addr, addr + 2u * c.width, 1});
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_idma_axi) != 0; }, sc_time(1, SC_MS)));
      wait(100, SC_US);
      // DEC-19: buffer released, rotation kept, frame discarded end to end.
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_axi_error, irq_axi_error);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & (irq_idma_done | irq_odma_done), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(dut->debug_idma_head(), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 1);  // the output buffer is still usable
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);
      FX1_CHECK_EQ(dut->debug_odma_head(), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_STAT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun);  // waiting for re-arm
      // Software clears the error and re-arms the same buffer.
      write32(FX1_ISP_DMA_ERR_OFFSET, 0x1F);
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7E);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 7, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 1);
   }

   void test_axi_write_error(unsigned failed_row, bool chroma) {
      hard_reset();
      frame_cfg c;
      fill_input(0, 8, c);
      fill_input(1, 9, c);
      guard_output(0, c);
      program(c);
      const std::uint64_t addr = (chroma ? out_uv(0) : out_y(0)) +
                                 failed_row * (chroma ? c.uv_stride : c.y_stride);
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, addr, addr + c.width, 1});
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET) == 1; }, sc_time(1, SC_MS)));
      wait(100, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi, err_odma_axi);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_axi_error, irq_axi_error);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_done, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);  // released
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);  // not marked done
      FX1_CHECK_EQ(dut->debug_odma_head(), 0);              // rotation kept
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      // No write of that frame after the failing burst.
      std::size_t after = 0;
      bool seen_error = false;
      for (const auto &a : mem->log) {
         if (a.cmd == tlm::TLM_WRITE_COMMAND) {
            after += seen_error;
            seen_error = seen_error || a.status != tlm::TLM_OK_RESPONSE;
         }
      }
      FX1_CHECK_EQ(after, 0);
      // The next frame goes to the same output buffer once software frees it.
      write32(FX1_ISP_DMA_ERR_OFFSET, err_odma_axi);
      guard_output(0, c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 9, c), 0);
   }

   void run_to_mid_frame(const frame_cfg &c, unsigned tag) {
      mem->latency = sc_time(20, SC_NS);
      fill_input(0, tag, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return mem->count(tlm::TLM_WRITE_COMMAND, false) >= 100; }, sc_time(5, SC_MS)));
   }

   bool no_access_after(const sc_time &t) {
      for (const auto &a : mem->log) {
         if (a.start > t) {
            return false;
         }
      }
      return true;
   }

   void test_soft_reset_mid_frame() {
      hard_reset();
      frame_cfg c;
      c.width = 640;
      c.height = 480;
      c.in_stride = 1280;
      c.y_stride = 640;
      c.uv_stride = 640;
      run_to_mid_frame(c, 11);
      const std::uint32_t ctrl = peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK;
      const sc_time tr = sc_time_stamp();
      write32(FX1_ISP_DMA_CTRL_OFFSET, ctrl);  // soft reset, enables kept
      wait(2, SC_MS);
      FX1_CHECK(no_access_after(tr));  // only a burst already in flight may complete
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & ~irq_idma_underrun, 0);  // no late DONE
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_CTRL_OFFSET) & 0x3u, 0x3u);  // enables preserved (DEC-14)
      FX1_CHECK_EQ(dut->debug_idma_head(), 0);
      // Re-arm and run: the retried frame is complete and correct.
      guard_output(0, c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 11, c), 0);
   }

   void test_external_reset_mid_frame() {
      hard_reset();
      frame_cfg c;
      c.width = 640;
      c.height = 480;
      c.in_stride = 1280;
      c.y_stride = 640;
      c.uv_stride = 640;
      run_to_mid_frame(c, 12);
      const sc_time tr = sc_time_stamp();
      rst_n.write(false);
      wait(1, SC_MS);
      FX1_CHECK(no_access_after(tr));
      FX1_CHECK(!irq.read());
      rst_n.write(true);
      wait(10, SC_NS);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_CTRL_OFFSET), 0x3F00);
      FX1_CHECK_EQ(read32(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      mem->log.clear();
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 12, c), 0);
   }

   void test_address_width() {
      // Bits [63:40] of the 64-bit address registers are beyond the 40-bit ports.
      hard_reset();
      frame_cfg c;
      fill_input(0, 13, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_IDMA_BUF_ADDR_H0_OFFSET, static_cast<std::uint32_t>(in_buf(0) >> 32) | 0xFF00u);
      write32(FX1_ISP_ODMA_Y_ADDR_H0_OFFSET, static_cast<std::uint32_t>(out_y(0) >> 32) | 0xAB0000u);
      write32(FX1_ISP_ODMA_UV_ADDR_H0_OFFSET, static_cast<std::uint32_t>(out_uv(0) >> 32) | 0x80000000u);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 13, c), 0);
      for (const auto &a : mem->log) {
         FX1_CHECK(a.status == tlm::TLM_OK_RESPONSE);
      }
   }

   void test_burst_formation() {
      hard_reset();
      frame_cfg c;
      c.width = 3840;
      c.height = 2;
      c.in_stride = 7680;
      c.y_stride = 3840;
      c.uv_stride = 3840;
      fill_input(0, 14, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);  // 64 beats x 16 bytes
      FX1_CHECK(wait_frames(1));
      // HAS §6.25.13: 7680-byte input line = 7 x 1024 + 512.
      std::vector<std::uint32_t> reads;
      for (const auto &a : mem->log) {
         if (a.cmd == tlm::TLM_READ_COMMAND && a.addr < in_buf(0) + c.in_stride) {
            reads.push_back(a.len);
         }
      }
      FX1_CHECK(reads == (std::vector<std::uint32_t>{1024, 1024, 1024, 1024, 1024, 1024, 1024, 512}));
      // Y line 0 and the UV line start 4 KB aligned: 3 x 1024 + 768. Y line 1
      // starts at +0xF00: 256 up to the 4 KB boundary, 3 x 1024, then 512.
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 4u + 5u + 4u);

      hard_reset();
      c.max_burst_m1 = 15;  // 16 beats = 256 bytes
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 2u * 30u);
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 3u * 15u);
   }

   void test_idma_disable_at_frame_boundary() {
      hard_reset();
      frame_cfg c;
      c.width = 640;
      c.height = 480;
      c.in_stride = 1280;
      c.y_stride = 640;
      c.uv_stride = 640;
      fill_input(0, 15, c);
      fill_input(1, 16, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return mem->count(tlm::TLM_WRITE_COMMAND, false) >= 50; }, sc_time(5, SC_MS)));
      enable_dma(c, false, true);  // clear IDMA_EN mid-frame
      wait(5, SC_MS);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 1);  // current frame completed
      FX1_CHECK_EQ(check_output(0, 15, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x2);  // next buffer not started
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, 0);  // disabled: no underrun
   }

   // Review finding M2-R1: an ODMA armed and then disabled must not start the
   // next frame (M2-A2), and waits without reporting overflow (M2-A6).
   void test_odma_disabled_after_arm() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 17, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      enable_dma(c, false, true);  // ODMA arms on buffer 0
      settle();
      enable_dma(c, false, false);  // disable before any SOF
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c, true, false);  // input only
      wait(500, SC_US);
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 1);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0);  // back-pressured, not dropped
      // Re-enabling the ODMA lets the waiting frame through intact.
      enable_dma(c, true, true);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 17, c), 0);
   }

   // Review finding M2-R2: a reset must wake an engine sleeping on a long
   // timed wait. With 2 ms per line, a frame re-armed right after the soft-reset
   // window must be accepted at once, not after the old line time.
   void test_reset_wakes_line_wait() {
      frame_cfg c;
      fill_input(0, 18, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0; },
                           sc_time(1, SC_MS)));
      wait(100, SC_US);  // the pipeline is inside the 2 ms of line 0
      write32(FX1_ISP_DMA_CTRL_OFFSET, peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      wait(1, SC_US);  // past the 32-cycle window
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0; },
                           sc_time(100, SC_US)));
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_start, irq_idma_start);
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 0);  // nothing from the old frame
   }

   void settle() { wait(1, SC_US); }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_params params;
   params.test_datapath_stub = true;  // lifecycle tests: simple, reference-free pixels
   fx1_isp_tlm dut("isp", params);
   test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.irq(irq);
   tb.dut = &dut;
   tb.mem = &mem;
   // Second DUT: 1 000 000 extra cycles (2 ms) per line, for the reset wake-up test.
   sc_signal<bool> rst_n_slow("rst_n_slow");
   sc_signal<bool> irq_slow("irq_slow");
   fx1_isp_params slow;
   slow.pipeline_line_overhead_cycles = 1000000;
   slow.test_datapath_stub = true;
   fx1_isp_tlm dut_slow("isp_slow", slow);
   test_memory mem_slow("mem_slow", mem_base, mem_size);
   bench tb_slow("tb_slow");
   tb_slow.slow_pipeline = true;
   tb_slow.socket.bind(dut_slow.csr_socket);
   dut_slow.idma_socket.bind(mem_slow.idma);
   dut_slow.odma_socket.bind(mem_slow.odma);
   dut_slow.rst_n(rst_n_slow);
   dut_slow.irq(irq_slow);
   tb_slow.rst_n(rst_n_slow);
   tb_slow.irq(irq_slow);
   tb_slow.dut = &dut_slow;
   tb_slow.mem = &mem_slow;

   sc_start();
   FX1_CHECK(tb.finished);
   FX1_CHECK(tb_slow.finished);
   return fx1_test::summary("fx1_isp_test_dma_tlm");
}
