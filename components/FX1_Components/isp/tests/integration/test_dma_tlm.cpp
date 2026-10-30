// SPDX-License-Identifier: Apache-2.0
// Standalone SystemC/TLM testbench of the FX1 ISP DMA engines and frame
// lifecycle (M2). A TLM test master stands in for the CPU; a test RAM with
// latency, fault injection and an access log serves both DMA masters.
//
// Pixel content comes from the M2 datapath stub (DEC-21): Y = cropped sample
// >> 4 with nearest-neighbour scaling, UV = 128. Expected output below is
// computed from that definition, independently of the model code.

#include <cstdint>
#include <cstdio>
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
constexpr std::uint32_t err_align = FX1_ISP_DMA_ERR_ERR_ALIGN_BIT;
constexpr std::uint32_t irq_cfg_error = FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT;
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
   bool slow_pipeline = false;
   std::uint64_t release_cycle = 0;  // core clock of the last i_rst_n release  // DUT built with a very long line latency
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
   std::uint64_t cycle(const sc_time &t) const {
      return static_cast<std::uint64_t>(t / dut->params().core_period);
   }
   // FRAME_COUNTER expected at time t: core clocks since the release, mod 2^32.
   std::uint32_t counter_at(const sc_time &t) const {
      return static_cast<std::uint32_t>(cycle(t) - release_cycle);
   }
   struct meta {
      std::uint32_t tag, ts, sof, eof;
   };
   meta out_meta(unsigned k) {
      return {read32(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * k), read32(FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * k),
              read32(FX1_ISP_ODMA_BUF_SOF_TS0_OFFSET + 4u * k), read32(FX1_ISP_ODMA_BUF_EOF_TS0_OFFSET + 4u * k)};
   }
   void set_in_meta(unsigned k, std::uint32_t tag, std::uint32_t ts) {
      write32(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * k, tag);
      write32(FX1_ISP_IDMA_BUF_TS0_OFFSET + 4u * k, ts);
   }
   bool no_out_meta(unsigned k) {
      const meta m = out_meta(k);
      return m.tag == 0 && m.ts == 0 && m.sof == 0 && m.eof == 0;
   }
   // First and last ODMA write of the log (start times).
   std::pair<sc_time, sc_time> odma_write_span() {
      sc_time first = SC_ZERO_TIME, last = SC_ZERO_TIME;
      bool any = false;
      for (const auto &a : mem->log) {
         if (a.cmd == tlm::TLM_WRITE_COMMAND && !a.from_idma) {
            first = any ? first : a.start;
            last = a.start;
            any = true;
         }
      }
      FX1_CHECK(any);
      return {first, last};
   }

   // ---- setup --------------------------------------------------------------------
   void hard_reset() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(SC_ZERO_TIME);  // the release reaches the DUT in the next delta cycle
      release_cycle = cycle(sc_time_stamp());
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
      resizer_settle();
   }
   // The Resizer publishes a derived set within two derivations of the last
   // key change (MAS §6.20.4.3); software completes the configuration first.
   void resizer_settle() { wait(dut->params().core_period * 80.0); }
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

   // Stop/drain (MAS v1.07 §9.9.3): no frame in flight before an IDLE-class write.
   bool wait_idle(const sc_time &timeout = sc_time(10, SC_MS)) {
      return wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; },
                        timeout);
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
      test_resizer_switch();
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
      // U5: buffer metadata and the time base (MAS Table 160).
      test_frame_counter();
      test_metadata_rotation();
      test_metadata_snapshot_and_points();
      test_metadata_resets();
      test_frame_counter_wrap();
      // V107-1 review R1: buffer requests reach the engines on the next edge.
      test_odma_free_next_edge_and_reassert();
      test_idma_valid_next_edge();
      // V107-2: rejected input buffers and native input depth.
      test_idma_reject_keeps_rotation();
      test_input_depth();
      // V107-3: the profile controller on the CH0 stream.
      test_profile_admission();
      // Audit item 1: a reset while the target holds the engine inside b_transport.
      for (const bool idma_side : {true, false}) {
         for (const bool external : {false, true}) {
            test_reset_inside_blocking_target(idma_side, external);
         }
      }
      // Review V107-7..10 R1 / R3: the drain lasts the lifetime of every
      // accepted transaction; an error response in the drain is kept.
      for (const bool idma_side : {true, false}) {
         for (const bool common : {false, true}) {
            test_drain_blocking_target(idma_side, common);
         }
      }
      test_error_response_in_drain(true);
      test_error_response_in_drain(false);
      // Review R9: a hard reset wins over the transactions accepted before it.
      for (const bool read : {true, false}) {
         for (const bool blocking : {false, true}) {
            for (const bool held : {false, true}) {
               test_error_response_after_hard_reset(read, blocking, held);
            }
         }
      }
   }

   // An error response to a transaction accepted before i_rst_n has no
   // effect: no DMA_ERR, no interrupt, no recovery (SEQUENCE stays 0),
   // whether it arrives after the release or while the reset is held.
   void test_error_response_after_hard_reset(bool read, bool blocking, bool held) {
      const std::string ctx = std::string("error response after i_rst_n, ") + (read ? "read" : "write") +
                              (blocking ? ", blocking" : ", annotated") + (held ? ", reset held" : ", released");
      hard_reset();
      frame_cfg c;
      fill_input(0, 42, c);
      guard_output(0, c);
      program(c);
      mem->latency = sc_time(5, SC_US);
      mem->blocking = blocking;
      const std::uint64_t at = read ? in_buf(0) : out_y(0);
      mem->faults.push_back({read ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND, at, at + 64u, 1});
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      const auto accepted = [&] {
         if (blocking) {
            return (read ? mem->active_idma : mem->active_odma) > 0;  // its first transfer: line 0, the faulty one
         }
         for (const auto &a : mem->log) {
            if (a.status != tlm::TLM_OK_RESPONSE && a.end > sc_time_stamp()) {
               return true;
            }
         }
         return false;
      };
      FX1_CHECK_EQ_CTX(wait_until(accepted, sc_time(5, SC_MS), sc_time(10, SC_NS)) ? 1 : 0, 1,
                       ctx + ": failing transfer accepted");
      rst_n.write(false);
      wait(held ? sc_time(10, SC_US) : sc_time(1, SC_US));  // the response is 5 us away
      rst_n.write(true);
      wait(20, SC_US);
      FX1_CHECK_EQ_CTX(mem->active_idma + mem->active_odma, 0u, ctx + ": the response came back");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_ERR_OFFSET) & (err_idma_axi | err_odma_axi), 0u, ctx + ": no AXI error");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_AXI_ERROR_BIT, 0u,
                       ctx + ": no AXI error interrupt");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), 0u, ctx + ": no recovery");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET), 0u, ctx + ": recovery status at reset");
      mem->faults.clear();
   }

   // A recovery requested while a blocking target holds a transfer (MAS
   // v1.07 §9.9.8): BUSY / PHASE = drain for the whole time the target holds
   // it; once BUSY clears no access of the aborted work takes effect and the
   // memory is not written any more.
   void test_drain_blocking_target(bool idma_side, bool common) {
      const std::string ctx = std::string("drain, blocking ") + (idma_side ? "IDMA" : "ODMA") +
                              (common ? ", common scope" : ", DMA scope");
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
      FX1_CHECK_EQ_CTX(wait_until([&] { return active > 0; }, sc_time(5, SC_MS), sc_time(100, SC_NS)) ? 1 : 0, 1,
                       ctx + ": transfer inside the target");
      if (common) {
         write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
      } else {
         write32(FX1_ISP_DMA_CTRL_OFFSET, peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      }
      const std::uint32_t busy = FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK;
      const auto phase = [](std::uint32_t v) {
         return (v & FX1_ISP_COMMON_RECOVERY_STATUS_PHASE_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_PHASE_SHIFT;
      };
      bool drain_ok = true, held = false;
      std::uint32_t st = read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET);
      const sc_time limit = sc_time_stamp() + sc_time(1, SC_MS);
      while ((st & busy) && sc_time_stamp() < limit) {
         if (mem->active_idma + mem->active_odma > 0) {
            held = true;
            drain_ok = drain_ok && phase(st) == 1u;
         }
         wait(100, SC_NS);
         st = read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET);
      }
      const sc_time done_at = sc_time_stamp();
      FX1_CHECK_EQ_CTX(held ? 1 : 0, 1, ctx + ": the target held a transfer during the recovery");
      FX1_CHECK_EQ_CTX(drain_ok ? 1 : 0, 1, ctx + ": BUSY / PHASE drain while the target holds it");
      FX1_CHECK_EQ_CTX(st & (busy | FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK), FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK,
                       ctx + ": recovery done");
      FX1_CHECK_EQ_CTX(mem->active_idma + mem->active_odma, 0u, ctx + ": nothing held at DONE");
      bool before = true;
      for (const auto &a : mem->log) {
         before = before && a.end <= done_at;
      }
      FX1_CHECK_EQ_CTX(before ? 1 : 0, 1, ctx + ": every access ended before BUSY cleared");
      const geom g = expected_geometry(c);
      const std::size_t n = std::size_t{c.y_stride} * g.oh;
      const std::vector<std::uint8_t> snap(mem->at(out_y(0)), mem->at(out_y(0)) + n);
      wait(50, SC_US);
      FX1_CHECK_EQ_CTX(std::equal(snap.begin(), snap.end(), mem->at(out_y(0))) ? 1 : 0, 1,
                       ctx + ": no memory side effect after DONE");
      FX1_CHECK_EQ_CTX(no_access_after(done_at) ? 1 : 0, 1, ctx + ": no access after DONE");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET) | peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0u,
                       ctx + ": ownership cleared at the final retirement");
   }

   // An AXI error response that arrives in the drain of a software recovery
   // sets its sticky bit and joins it: no new SEQUENCE, the captured cause
   // (3, DMA command) unchanged (MAS v1.07 §9.9.8).
   void test_error_response_in_drain(bool read) {
      const std::string ctx = std::string("error response in the drain, ") + (read ? "read" : "write");
      hard_reset();
      frame_cfg c;
      fill_input(0, 41, c);
      guard_output(0, c);
      program(c);
      mem->latency = sc_time(5, SC_US);  // annotated
      const std::uint64_t at = read ? in_buf(0) : out_y(0);
      mem->faults.push_back({read ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND, at, at + 64u, 1});
      const std::uint32_t seq0 = read32(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      // The failing transaction is accepted; its response is 5 us away.
      const auto in_flight = [&] {
         for (const auto &a : mem->log) {
            if (a.status != tlm::TLM_OK_RESPONSE && a.end > sc_time_stamp()) {
               return true;
            }
         }
         return false;
      };
      FX1_CHECK_EQ_CTX(wait_until(in_flight, sc_time(5, SC_MS), sc_time(10, SC_NS)) ? 1 : 0, 1,
                       ctx + ": failing transfer accepted");
      write32(FX1_ISP_DMA_CTRL_OFFSET, peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & 0x301u, 0x101u, ctx + ": draining");
      FX1_CHECK(wait_until([&] { return (read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & 0x3u) == 0x2u; },
                           sc_time(1, SC_MS), sc_time(100, SC_NS)));
      const std::uint32_t bit = read ? err_idma_axi : err_odma_axi;
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_ERR_OFFSET) & bit, bit, ctx + ": sticky AXI error kept");
      FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_AXI_ERROR_BIT,
                       FX1_ISP_DMA_IRQ_STAT_IRQ_AXI_ERROR_BIT, ctx + ": AXI error interrupt");
      FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), seq0 + 1u, ctx + ": joined, one recovery");
      const std::uint32_t st = read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET);
      if ((st & FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_MASK) != 0) {
         FX1_CHECK_EQ_CTX((read32(FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET) >> 8) & 7u, 3u, ctx + ": cause as captured");
      }
      FX1_CHECK_EQ_CTX(read ? 1u : (st & FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_MASK) != 0, 1u,
                       ctx + ": an output in progress belongs to an admitted frame");
      mem->faults.clear();
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
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);  // taken at frame start (MAS Table 159)
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

   // Resizer modes between frames (MAS v1.07 §6.20.1.1, §9.9.3): RESIZER_CTRL
   // is IDLE class, so software stops the input and drains before a mode
   // change; the ODMA takes each frame's size from the frame, without
   // re-arming. A write while a frame is in flight is rejected with MODE and
   // changes nothing. A frame wider than the stride checked at arm is refused
   // (U4-A2).
   void test_resizer_switch() {
      hard_reset();
      const std::uint32_t en = 1u, sh = FX1_ISP_RESIZER_CTRL_SCALE_SHIFT;
      frame_cfg c;
      c.width = 1280;
      c.height = 720;
      c.in_stride = 2560;
      c.y_stride = c.uv_stride = 1280;  // sized for the largest mode (FullRes)
      c.resizer_ctrl = en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X360 << sh);
      frame_cfg qhd = c, vga = c;
      qhd.resizer_ctrl = en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_960X540 << sh);
      vga.resizer_ctrl = en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << sh);
      for (unsigned k = 0; k < 4; ++k) {
         fill_input(k, 20 + k, c);
      }
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xF);  // armed once for all modes
      enable_dma(c);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ_CTX(check_output(0, 20, c), 0, "640x360");
      // New mode between frames (input stopped, drained), no re-arm: the next
      // frame is 960x540.
      enable_dma(c, false, true);
      FX1_CHECK(wait_idle());
      write32(FX1_ISP_RESIZER_CTRL_OFFSET, qhd.resizer_ctrl);
      resizer_settle();
      enable_dma(c);
      FX1_CHECK_EQ(read32(FX1_ISP_RESIZER_OUT_W_OFFSET), 960);
      guard_output(1, qhd);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2);
      FX1_CHECK(wait_frames(2));
      FX1_CHECK_EQ_CTX(check_output(1, 21, qhd), 0, "960x540 without re-arm");
      // A mode written while the frame is in the pipeline: rejected (MODE),
      // RESIZER_CTRL unchanged; that frame and the next keep 960x540.
      guard_output(2, qhd);
      guard_output(3, qhd);
      write32(FX1_ISP_TUNE_ERROR_OFFSET, 0xFFFFFFFFu);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x4);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0; },
                           sc_time(10, SC_MS), sc_time(10, SC_NS)));
      write32(FX1_ISP_RESIZER_CTRL_OFFSET, vga.resizer_ctrl);
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK);
      FX1_CHECK_EQ(read32(FX1_ISP_RESIZER_CTRL_OFFSET), qhd.resizer_ctrl);
      FX1_CHECK(wait_frames(3));
      FX1_CHECK_EQ_CTX(check_output(2, 22, qhd), 0, "mode written during the frame: rejected");
      resizer_settle();
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x8);
      FX1_CHECK(wait_frames(4));
      FX1_CHECK_EQ_CTX(check_output(3, 23, qhd), 0, "the next frame keeps 960x540");
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_align, 0);
      // A frame wider than the strides: the claim at the output SOF rejects
      // the buffer (MAS v1.07 §6.25.3.4, §6.25.3.8): ALIGN and CFG_ERROR, FREE
      // cleared, nothing written, rotation kept; the frame waits (overflow)
      // until software corrects the stride and frees the buffer again.
      hard_reset();
      frame_cfg narrow = c;
      narrow.y_stride = narrow.uv_stride = 640;  // enough for 640x360 only
      program(narrow);
      guard_output(0, narrow);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);  // armed: 640 <= stride
      enable_dma(narrow, false, true);
      write32(FX1_ISP_RESIZER_CTRL_OFFSET, en | (FX1_ISP_RESIZER_CTRL_SCALE_RES_960X540 << sh));
      resizer_settle();
      enable_dma(narrow);
      fill_input(0, 30, narrow);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_align) != 0; }, sc_time(10, SC_MS)));
      settle();
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT,
                   FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT,
                   FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT);  // the frame waits
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 0u);
      std::size_t written = 0;
      for (std::uint32_t i = 0; i < 640u * 360u; ++i) {
         written += *mem->at(out_y(0) + i) != guard;
      }
      FX1_CHECK_EQ_CTX(written, 0u, "no byte written by the rejected claim");
      frame_cfg wide = narrow;
      wide.resizer_ctrl = qhd.resizer_ctrl;
      wide.y_stride = wide.uv_stride = 960;
      guard_output(0, wide);  // nothing was written: guard the larger footprint
      write32(FX1_ISP_ODMA_Y_STRIDE_OFFSET, 960);
      write32(FX1_ISP_ODMA_UV_STRIDE_OFFSET, 960);
      write32(FX1_ISP_DMA_ERR_OFFSET, err_align);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ_CTX(check_output(0, 30, wide), 0, "the waiting frame after the correction");
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
                   0u);  // flow control is not an error (MAS v1.07 Table 9-188)
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
      // The status stays set while the stall lasts (MAS Table 161); the
      // interrupt is raised once per stall episode.
      write32(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, irq_odma_overflow);
      enable_dma(c);  // make the ODMA re-evaluate while still stalled
      write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x1);
      wait(100, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, err_overflow);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_overflow, 0);
      // Disabling the ODMA ends the condition (U5-A5): the bit can be cleared.
      enable_dma(c, true, false);
      settle();
      write32(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, 0);
      // Enabled again, still no buffer: a new stall, a new interrupt.
      enable_dma(c);
      settle();
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, err_overflow);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_overflow, irq_odma_overflow);
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, irq_odma_overflow);
      // Freeing a buffer resumes the frame without loss (DEC-18) and ends the
      // stall: the sticky bit can then be cleared.
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 5, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 1);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, err_overflow);
      write32(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_overflow, 0);
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
         // MAS v1.07 Table 6-153: the external capacity is 3842x2162.
         {"width > 3842", true, [](frame_cfg &c) { c.width = 3844; c.in_stride = 7696; c.y_stride = 3856; c.uv_stride = 3856; }, nullptr},
         {"height > 2162", true, [](frame_cfg &c) { c.height = 2164; }, nullptr},
         {"depth 2", true, nullptr, [&] { write32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 2); }},
         {"depth 3", true, nullptr, [&] { write32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, 3); }},
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
         // An illegal start configuration also raises IRQ_CFG_ERROR (MAS Table 162).
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_cfg_error, irq_cfg_error, ctx + ": CFG_ERROR");
         FX1_CHECK_EQ_CTX(read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK,
                          FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, ctx);
         if (bc.input) {
            FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_READ_COMMAND, true), 0, ctx + ": nothing read");
            FX1_CHECK_EQ_CTX(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_start, 0, ctx + ": did not start");
            // MAS v1.07 §6.25.3.4: the rejected buffer is returned, the rotation stays.
            FX1_CHECK_EQ_CTX(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0, ctx + ": VALID cleared");
         }
         FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_WRITE_COMMAND, false), 0, ctx + ": nothing written");
         FX1_CHECK_EQ_CTX(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, ctx);
      }
   }

   void test_isp_en_trap() {
      // MAS v1.07 §6.25.3.8: FREE only makes a buffer available; the claim at
      // the output SOF captures the frame's geometry. A buffer freed before
      // ISP_EN is therefore fine (DEC-20 superseded): no error, and the frame
      // that uses it gets its own geometry.
      hard_reset();
      frame_cfg c;
      fill_input(0, 6, c);
      guard_output(0, c);
      program(c, false);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      enable_dma(c, false, true);
      settle();
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_align, 0);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_cfg_error, 0);
      write32(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
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
      set_in_meta(0, 0x5500u + failed_row, 0x7700u);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_idma_axi) != 0; }, sc_time(1, SC_MS)));
      wait(100, SC_US);
      // MAS v1.07 §9.9.8: terminal; automatic DMA-scope recovery of every
      // unretired frame (DEC-19 superseded): DMA_ERR / DMA_IRQ_STAT kept,
      // every ownership bit cleared, rotations restarted, records published.
      const std::uint32_t rs = read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET);
      FX1_CHECK_EQ(rs & 0x3u, FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK);  // BUSY clear, DONE
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), 1u);
      const unsigned n = (rs & FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_SHIFT;
      FX1_CHECK(n <= 1u);  // the frame, if admitted before the failing line
      if (n == 1) {
         FX1_CHECK_EQ(read32(FX1_ISP_COMMON_ABORT0_FRAME_OFFSET), 1u);
         FX1_CHECK_EQ(read32(FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET), 0x1u | (1u << 8));  // CH0, read error
      }
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_axi_error, irq_axi_error);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & (irq_idma_done | irq_odma_done), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(dut->debug_idma_head(), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);  // final retirement: re-arm every output
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);
      FX1_CHECK(no_out_meta(0));  // an aborted frame publishes no metadata
      FX1_CHECK_EQ(dut->debug_odma_head(), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_STAT_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_underrun, err_underrun);  // waiting for re-arm
      // Software clears the errors and re-arms input and output from buffer 0.
      write32(FX1_ISP_DMA_ERR_OFFSET, 0x1F);
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, 0x7E);
      set_in_meta(0, 0x5600u + failed_row, 0x7800u);  // the retry is a new frame start
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 7, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 1);
      const meta m = out_meta(0);
      FX1_CHECK_EQ(m.tag, 0x5600u + failed_row);
      FX1_CHECK_EQ(m.ts, 0x7800u);
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
      set_in_meta(0, 0x8800u, 0x1);
      set_in_meta(1, 0x8801u, 0x2);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi) != 0; }, sc_time(1, SC_MS)));
      wait(100, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi, err_odma_axi);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_axi_error, irq_axi_error);
      // Automatic recovery: the frame is recorded as failed on CH0, write error.
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & 0x3u, FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_ABORT0_FRAME_OFFSET), 1u);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET), 0x1u | (2u << 8));
      FX1_CHECK_EQ(read32(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET) & FX1_ISP_OUTPUT_IRQ_STATUS_ERROR_MASK,
                   1u << FX1_ISP_OUTPUT_IRQ_STATUS_ERROR_SHIFT);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_done, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);  // released
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);  // not marked done
      FX1_CHECK(no_out_meta(0));                            // nor published
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
      // Software clears the error and re-arms from buffer 0 (the rotations restarted).
      write32(FX1_ISP_DMA_ERR_OFFSET, err_odma_axi);
      guard_output(0, c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 8, c), 0);
      FX1_CHECK_EQ(out_meta(0).tag, 0x8800u);
      FX1_CHECK_EQ(out_meta(0).ts, 0x1u);
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
      const std::uint32_t irq_before = peek(FX1_ISP_DMA_IRQ_STAT_OFFSET);
      const sc_time tr = sc_time_stamp();
      write32(FX1_ISP_DMA_CTRL_OFFSET, ctrl);  // soft reset, enables kept
      wait(2, SC_MS);
      FX1_CHECK(no_access_after(tr));  // only a burst already in flight may complete
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0);
      // DMA_IRQ_STAT is kept by the DMA scope (MAS v1.07 §9.9.8): no event
      // other than underrun appeared after the request (no late DONE).
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & ~irq_before & ~irq_idma_underrun, 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_odma_done, 0);
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

   // Bursts of the first lines of a 3840 x 32 frame (MAS v1.07: active images
   // below 32 rows are not a legal channel geometry, so the 07/10 3840 x 2
   // frame is no longer admitted).
   std::size_t writes_in(std::uint64_t lo, std::uint64_t hi) const {
      std::size_t n = 0;
      for (const auto &a : mem->log) {
         n += a.cmd == tlm::TLM_WRITE_COMMAND && a.addr >= lo && a.addr < hi;
      }
      return n;
   }
   void test_burst_formation() {
      hard_reset();
      frame_cfg c;
      c.width = 3840;
      c.height = 32;
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
      FX1_CHECK_EQ(writes_in(out_y(0), out_y(0) + 3840), 4u);
      FX1_CHECK_EQ(writes_in(out_y(0) + 3840, out_y(0) + 2 * 3840), 5u);
      FX1_CHECK_EQ(writes_in(out_uv(0), out_uv(0) + 3840), 4u);

      hard_reset();
      c.max_burst_m1 = 15;  // 16 beats = 256 bytes
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 32u * 30u);  // 7680 / 256 per line
      FX1_CHECK_EQ(writes_in(out_y(0), out_y(0) + 3840), 15u);
      FX1_CHECK_EQ(writes_in(out_y(0) + 3840, out_y(0) + 2 * 3840), 15u);
      FX1_CHECK_EQ(writes_in(out_uv(0), out_uv(0) + 3840), 15u);
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
      wait(1, SC_US);  // past the 15-cycle pulse
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0; },
                           sc_time(100, SC_US)));
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_IRQ_STAT_OFFSET) & irq_idma_start, irq_idma_start);
      FX1_CHECK_EQ(mem->count(tlm::TLM_WRITE_COMMAND, false), 0);  // nothing from the old frame
   }

   // ---- U5: buffer metadata and time base (MAS Table 160) -------------------------

   // FRAME_COUNTER counts core clocks from the release of i_rst_n, read at
   // the edge of the access, without any per-clock activity.
   void test_frame_counter() {
      hard_reset();
      for (int i = 0; i < 3; ++i) {
         const sc_time t = sc_time_stamp();
         FX1_CHECK_EQ(read32(FX1_ISP_FRAME_COUNTER_OFFSET), counter_at(t));
         wait(dut->params().core_period * (1000.0 * i + 7));
      }
      FX1_CHECK_EQ(peek(FX1_ISP_FRAME_COUNTER_OFFSET), 0u);  // the stored word is not the count
      // Read-only: a write changes nothing.
      write32(FX1_ISP_FRAME_COUNTER_OFFSET, 0x12345678u);
      const sc_time t = sc_time_stamp();
      FX1_CHECK_EQ(read32(FX1_ISP_FRAME_COUNTER_OFFSET), counter_at(t));
      // A debugger read sees the current count too.
      std::uint32_t v = 0;
      tlm::tlm_generic_payload g;
      g.set_command(tlm::TLM_READ_COMMAND);
      g.set_address(FX1_ISP_FRAME_COUNTER_OFFSET);
      g.set_data_ptr(reinterpret_cast<unsigned char *>(&v));
      g.set_data_length(4);
      g.set_streaming_width(4);
      FX1_CHECK_EQ(socket->transport_dbg(g), 4u);
      FX1_CHECK_EQ(v, counter_at(sc_time_stamp()));
      // Held at zero while i_rst_n is asserted.
      rst_n.write(false);
      wait(1, SC_US);
      FX1_CHECK_EQ(socket->transport_dbg(g), 4u);
      FX1_CHECK_EQ(v, 0u);
   }

   // Input and output rotations out of step: the tag follows the frame, not
   // the buffer index. An output failure keeps the ODMA on buffer 0 while the
   // IDMA moves on, so input k + 1 completes into output k.
   void test_metadata_rotation() {
      hard_reset();
      frame_cfg c;
      for (unsigned k = 0; k < 4; ++k) {
         fill_input(k, 20 + k, c);
         guard_output(k, c);
      }
      program(c);
      for (unsigned k = 0; k < 4; ++k) {
         set_in_meta(k, 0xC0DE0000u + k, 0x1000u + 0x111u * k);
      }
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, out_y(0) + 3u * c.y_stride, out_y(0) + 3u * c.y_stride + 1u, 1});
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi) != 0; }, sc_time(1, SC_MS)));
      wait(100, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_odma_axi, err_odma_axi);
      for (unsigned k = 0; k < 4; ++k) {
         FX1_CHECK(no_out_meta(k));
      }
      // After the recovery both rotations restart at buffer 0.
      write32(FX1_ISP_DMA_ERR_OFFSET, err_odma_axi);
      guard_output(0, c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x7);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x7);
      FX1_CHECK(wait_frames(3));
      settle();
      std::uint32_t prev_eof = 0;
      for (unsigned k = 0; k < 3; ++k) {
         FX1_CHECK_EQ(check_output(k, 20 + k, c), 0);
         const meta m = out_meta(k);
         FX1_CHECK_EQ_CTX(m.tag, 0xC0DE0000u + k, "output " + std::to_string(k));
         FX1_CHECK_EQ(m.ts, 0x1000u + 0x111u * k);
         FX1_CHECK(m.sof < m.eof);
         FX1_CHECK(m.sof > prev_eof);  // one frame at a time through the ODMA
         prev_eof = m.eof;
      }
      FX1_CHECK(no_out_meta(3));
      FX1_CHECK(prev_eof < read32(FX1_ISP_FRAME_COUNTER_OFFSET));
   }

   // The working set captures the input metadata at frame start; SOF_TS is
   // the output start of frame, EOF_TS the last line accepted, both published
   // at completion. The ODMA takes its buffer (FREE cleared) at frame start.
   void test_metadata_snapshot_and_points() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 30, c);
      guard_output(0, c);
      program(c);
      mem->latency = sc_time(30, SC_NS);  // the last line's writes take time
      set_in_meta(0, 0x11u, 0x22u);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      mem->log.clear();
      enable_dma(c);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK) != 0; },
                           sc_time(1, SC_MS), dut->params().core_period));
      const std::uint32_t started = read32(FX1_ISP_FRAME_COUNTER_OFFSET);
      set_in_meta(0, 0x99u, 0x98u);  // the buffer is still owned by the IDMA
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_ODMA_BUSY_MASK) != 0; },
                           sc_time(1, SC_MS), dut->params().core_period));
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0u);  // taken at frame start (MAS Table 159)
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0u);
      FX1_CHECK(no_out_meta(0));  // nothing published before completion
      FX1_CHECK(wait_until([&] { return peek(FX1_ISP_ODMA_BUF_DONE_OFFSET) != 0; }, sc_time(1, SC_MS),
                           dut->params().core_period));
      const sc_time done = sc_time_stamp();
      FX1_CHECK_EQ(check_output(0, 30, c), 0);
      const meta m = out_meta(0);
      FX1_CHECK_EQ(m.tag, 0x11u);  // the value at frame start
      FX1_CHECK_EQ(m.ts, 0x22u);
      FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_TAG0_OFFSET), 0x99u);
      const auto span = odma_write_span();
      FX1_CHECK(m.sof >= started);
      FX1_CHECK_EQ(m.sof, counter_at(span.first));  // first output write
      FX1_CHECK_EQ(m.eof, counter_at(span.second));  // last line of the frame
      FX1_CHECK(counter_at(done) > m.eof);           // published after its writes returned
      FX1_CHECK(counter_at(done) - m.eof >= cycle(mem->latency));
   }

   // The IDMA tag and timestamp are configuration (kept). Published ODMA
   // metadata and DONE of a completed buffer: kept by the DMA scope, cleared
   // by the common scope (MAS v1.07 §9.9.8, U5-A2 refined); FRAME_COUNTER is
   // free-running across soft resets (VP decision U5-A1).
   void test_metadata_resets() {
      for (const bool common : {false, true}) {
         hard_reset();
         frame_cfg c;
         fill_input(0, 31, c);
         guard_output(0, c);
         program(c);
         set_in_meta(0, 0xAB, 0xCD);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         enable_dma(c);
         FX1_CHECK(wait_frames(1));
         settle();
         FX1_CHECK_EQ(out_meta(0).tag, 0xABu);
         const std::uint32_t before = read32(FX1_ISP_FRAME_COUNTER_OFFSET);
         if (common) {
            write32(FX1_ISP_COMMON_CTRL_OFFSET, peek(FX1_ISP_COMMON_CTRL_OFFSET) | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
         } else {
            write32(FX1_ISP_DMA_CTRL_OFFSET, peek(FX1_ISP_DMA_CTRL_OFFSET) | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
         }
         settle();
         FX1_CHECK_EQ_CTX(no_out_meta(0) ? 1 : 0, common ? 1 : 0, common ? "common soft reset" : "DMA soft reset");
         if (!common) {
            FX1_CHECK_EQ(out_meta(0).tag, 0xABu);
            FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_DONE_OFFSET), 1u);
         }
         FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_TAG0_OFFSET), 0xABu);
         FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_TS0_OFFSET), 0xCDu);
         const sc_time t = sc_time_stamp();
         const std::uint32_t after = read32(FX1_ISP_FRAME_COUNTER_OFFSET);
         FX1_CHECK(after > before);
         FX1_CHECK_EQ(after, counter_at(t));
      }
      // i_rst_n: everything to zero, the count restarts from the release.
      hard_reset();
      FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_TAG0_OFFSET), 0u);
      FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_TS0_OFFSET), 0u);
      FX1_CHECK(no_out_meta(0));
      const sc_time t = sc_time_stamp();
      FX1_CHECK_EQ(read32(FX1_ISP_FRAME_COUNTER_OFFSET), counter_at(t));
      FX1_CHECK(counter_at(t) < 100u);
   }

   // The count wraps modulo 2^32; a frame across the wrap keeps a correct
   // span EOF_TS - SOF_TS. Waiting 2^32 clocks costs no simulation activity.
   void test_frame_counter_wrap() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 32, c);
      guard_output(0, c);
      program(c);
      set_in_meta(0, 0x77, 0);
      const std::uint32_t now = read32(FX1_ISP_FRAME_COUNTER_OFFSET);
      wait(dut->params().core_period * static_cast<double>(0xFFFFFFFFull - now - 300));
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      mem->log.clear();
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      settle();
      FX1_CHECK_EQ(check_output(0, 32, c), 0);
      const meta m = out_meta(0);
      const auto span = odma_write_span();
      FX1_CHECK(m.sof > 0xFFFF0000u);  // before the wrap
      FX1_CHECK(m.eof < 0x10000u);     // after it
      FX1_CHECK_EQ(m.eof - m.sof, static_cast<std::uint32_t>(cycle(span.second) - cycle(span.first)));
      FX1_CHECK_EQ(m.sof, counter_at(span.first));
      const sc_time t = sc_time_stamp();
      FX1_CHECK_EQ(read32(FX1_ISP_FRAME_COUNTER_OFFSET), counter_at(t));
   }

   void settle() { wait(1, SC_US); }

   // ---- V107-1 review R1: W1S requests and their consumers (MAS Table 8-180) ----
   sc_time edge_time(std::uint64_t c) const { return dut->params().core_period * static_cast<double>(c); }
   // A write applied now whose access latency is consumed later, so the
   // caller can observe the edge of the write (CSR-23).
   sc_time write32_now(std::uint32_t addr, std::uint32_t v) {
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
      return d;
   }
   void wait_deltas() {
      for (int i = 0; i < 4; ++i) {
         wait(SC_ZERO_TIME);
      }
   }

   // The ODMA samples ODMA_BUF_FREE as registered on its edge: a buffer freed
   // on edge c is taken on edge c + 1 (SOF_TS, U5-A4), never on c. A free
   // re-asserted on the edge that takes the buffer wins over the take (set
   // wins, either order): the buffer stays free and a later frame uses it
   // without another write.
   void test_odma_free_next_edge_and_reassert() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 5, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);  // the frame waits for a free output buffer
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & err_overflow) != 0; }, sc_time(1, SC_MS)));
      const std::uint64_t cw = cycle(sc_time_stamp());
      const sc_time lat = write32_now(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      wait_deltas();
      FX1_CHECK_EQ(cycle(sc_time_stamp()), cw);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 0x1u, 0x1u);  // not taken on the edge that frees it
      wait(lat);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 5, c), 0);
      FX1_CHECK_EQ(out_meta(0).sof, static_cast<std::uint32_t>(cw + 1 - release_cycle));

      // Frame 2 waits on buffer 1; free it, then free it again on the take edge.
      fill_input(1, 6, c);
      guard_output(1, c);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK) != 0; },
                           sc_time(1, SC_MS)));
      settle();
      const std::uint64_t cw2 = cycle(sc_time_stamp());
      const sc_time lat2 = write32_now(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x2);
      wait(edge_time(cw2 + 1) - sc_time_stamp());
      FX1_CHECK_EQ(cycle(sc_time_stamp()), cw2 + 1);
      write32_now(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x2);  // re-assert on the take edge
      wait(lat2);
      FX1_CHECK(wait_frames(2));
      FX1_CHECK_EQ(check_output(1, 6, c), 0);
      FX1_CHECK_EQ(out_meta(1).sof, static_cast<std::uint32_t>(cw2 + 1 - release_cycle));
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 0x2u, 0x2u);  // the re-assert is a new request

      // Frames 3..6 use input buffers 2, 3, 0, 1; software frees output
      // buffers 2, 3 and 0 only. Frame 6 lands in buffer 1 on the kept request.
      const unsigned tags[4] = {9, 10, 7, 8};  // by input buffer 0..3
      for (unsigned k = 0; k < 4; ++k) {
         fill_input(k, tags[k], c);
         guard_output(k, c);
      }
      write32(FX1_ISP_ODMA_BUF_DONE_OFFSET, 0x3);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0xF);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xD);
      FX1_CHECK(wait_frames(6));
      for (unsigned k = 0; k < 4; ++k) {
         FX1_CHECK_EQ_CTX(check_output(k, tags[k], c), 0u, "output buffer " + std::to_string(k));
      }
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 6u);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0u);
   }

   // MAS v1.07 §6.25.3.4: a rejected input buffer is not read, its VALID bit
   // is cleared and the rotation stays, so a later valid buffer is not taken
   // ahead of it; software corrects the configuration and sets VALID again.
   void test_idma_reject_keeps_rotation() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 3, c);
      fill_input(1, 4, c);
      guard_output(0, c);
      guard_output(1, c);
      program(c);
      set64(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET, in_buf(0) + 8);  // buffer 0 misaligned
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      enable_dma(c);
      wait(100, SC_US);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_ERR_OFFSET) & err_align, err_align);
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0x2u);  // buffer 0 returned, buffer 1 still queued
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0u);  // buffer 1 not taken ahead of the head
      FX1_CHECK_EQ(peek(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0u);
      set64(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET, in_buf(0));
      write32(FX1_ISP_DMA_ERR_OFFSET, err_align);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_frames(2));
      FX1_CHECK_EQ(check_output(0, 3, c), 0);  // head 0 first, then buffer 1
      FX1_CHECK_EQ(check_output(1, 4, c), 0);
      FX1_CHECK_EQ(peek(FX1_ISP_DMA_ERR_OFFSET) & err_align, 0u);
   }

   // COMMON_INPUT_FORMAT.DEPTH (MAS v1.07 Table 6-18): native RAW10 right-
   // aligned with DEPTH = 1 equals the same scene normalized to RAW12 with
   // DEPTH = 0; normalized data declared RAW10 (a double shift) is wrong.
   void test_input_depth() {
      frame_cfg c;
      auto fill_raw10 = [&](unsigned buf, unsigned tag) {
         for (std::uint32_t y = 0; y < c.height; ++y) {
            for (std::uint32_t x = 0; x < c.width; ++x) {
               mem->put16(in_buf(buf) + std::uint64_t{y} * c.in_stride + 2u * x,
                          static_cast<std::uint16_t>(sample(tag, x, y) >> 2));
            }
         }
      };
      for (const bool raw10 : {false, true}) {
         hard_reset();
         const std::string ctx = raw10 ? "RAW10 native" : "RAW12 normalized";
         if (raw10) {
            fill_raw10(0, 9);
         } else {
            fill_input(0, 9, c);
         }
         guard_output(0, c);
         program(c);
         write32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, raw10 ? FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_RAW10 : 0u);
         write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
         write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         enable_dma(c);
         FX1_CHECK(wait_frames(1));
         FX1_CHECK_EQ_CTX(check_output(0, 9, c), 0u, ctx);
      }
      hard_reset();
      fill_input(0, 9, c);  // normalized RAW12 ...
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_RAW10);  // ... declared RAW10
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      enable_dma(c);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK(check_output(0, 9, c) > 0u);  // the double shift is visible
   }

   // MAS v1.07 §9.7: profile mode holds ingress until the first APPLY; a NEXT
   // APPLY while streaming activates on the first SOF after the old epoch has
   // drained; references retire once; DMA recovery keeps the active profile.
   void test_profile_admission() {
      hard_reset();
      frame_cfg c;
      for (unsigned k = 0; k < 4; ++k) {
         fill_input(k, 20 + k, c);
         guard_output(k, c);
      }
      program(c);
      auto cmd = [&](std::uint32_t v) { write32(FX1_ISP_TUNE_CMD_OFFSET, v); };
      auto prepare = [&](unsigned bank, std::uint32_t id) {
         write32(FX1_ISP_TUNE_BANK_OFFSET, bank);
         cmd(FX1_ISP_TUNE_CMD_INIT_MASK);
         write32(FX1_ISP_TUNE_PROFILE_ID_OFFSET, id);
         cmd(FX1_ISP_TUNE_CMD_SEAL_MASK);
      };
      write32(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);  // idle: accepted
      prepare(0, 0x11);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x3);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      enable_dma(c);
      wait(300, SC_US);
      FX1_CHECK_EQ(peek(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0u);  // held: no active profile
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_NEXT_FRAME_OFFSET), 1u);  // waiting does not count
      cmd(FX1_ISP_TUNE_CMD_APPLY_MASK);
      FX1_CHECK(wait_frames(2));
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 0x11u);
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET), 1u);
      FX1_CHECK_EQ(check_output(0, 20, c), 0);
      FX1_CHECK_EQ(check_output(1, 21, c), 0);
      // Bank 1 while bank 0 streams; the input DMA is enabled, so the IDLE
      // class (here COMMON_BAYER) is rejected with MODE.
      prepare(1, 0x22);
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ERROR_OFFSET), 0u);
      write32(FX1_ISP_COMMON_BAYER_OFFSET, 3);
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ERROR_OFFSET), FX1_ISP_TUNE_ERROR_MODE_MASK);
      write32(FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_MODE_MASK);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0xC);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0xC);
      cmd(FX1_ISP_TUNE_CMD_APPLY_MASK);
      FX1_CHECK(wait_frames(4));
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 0x22u);
      const std::uint32_t switched = read32(FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET);
      FX1_CHECK(switched == 3u || switched == 4u);  // the first SOF after APPLY and the drain
      FX1_CHECK_EQ(check_output(2, 22, c), 0);
      FX1_CHECK_EQ(check_output(3, 23, c), 0);
      settle();
      const std::uint32_t st = read32(FX1_ISP_TUNE_STATUS_OFFSET);
      FX1_CHECK_EQ(st & (FX1_ISP_TUNE_STATUS_BANK0_REFERENCED_MASK | FX1_ISP_TUNE_STATUS_BANK1_REFERENCED_MASK), 0u);
      FX1_CHECK_EQ(st & FX1_ISP_TUNE_STATUS_ACTIVE_BANK_MASK, 1u);
      // DMA recovery keeps the sealed active profile.
      write32(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      settle();
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_STATUS_OFFSET) & FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK,
                   FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK);
      FX1_CHECK_EQ(read32(FX1_ISP_TUNE_ACTIVE_ID_OFFSET), 0x22u);
   }

   // The IDMA samples IDMA_BUF_VALID as registered on its edge: a buffer
   // made valid on edge c is read from edge c + 1.
   void test_idma_valid_next_edge() {
      hard_reset();
      frame_cfg c;
      fill_input(0, 5, c);
      guard_output(0, c);
      program(c);
      write32(FX1_ISP_ODMA_BUF_FREE_OFFSET, 0x1);
      enable_dma(c);  // enabled, no valid input buffer: idle
      settle();
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0u);
      const std::uint64_t cw = cycle(sc_time_stamp());
      const sc_time lat = write32_now(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      wait_deltas();
      FX1_CHECK_EQ(cycle(sc_time_stamp()), cw);
      FX1_CHECK_EQ(mem->count(tlm::TLM_READ_COMMAND, true), 0u);  // not armed on the edge that validates it
      wait(lat);
      FX1_CHECK(wait_frames(1));
      FX1_CHECK_EQ(check_output(0, 5, c), 0);
      std::uint64_t first = 0;
      for (const auto &a : mem->log) {
         if (a.cmd == tlm::TLM_READ_COMMAND && a.from_idma) {
            first = cycle(a.start);
            break;
         }
      }
      FX1_CHECK_EQ(first, cw + 1);
   }
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
