// SPDX-License-Identifier: Apache-2.0
// Standalone SystemC/TLM testbench of the FX1 ISP CSR port (M1):
// a TLM test master in place of the CPU, a memory stub on both DMA masters,
// an IRQ monitor and a reset driver.

#include <cstdint>
#include <cstring>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"

using namespace sc_core;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;

namespace {

// Memory stub for the DMA masters; M1 issues no DMA traffic, so any access
// is reported as a failure.
struct memory_stub : sc_module {
   tlm_utils::simple_target_socket<memory_stub> idma;
   tlm_utils::simple_target_socket<memory_stub> odma;
   unsigned accesses = 0;

   explicit memory_stub(sc_module_name n) : sc_module(n), idma("idma"), odma("odma") {
      idma.register_b_transport(this, &memory_stub::b_transport);
      odma.register_b_transport(this, &memory_stub::b_transport);
   }
   void b_transport(tlm::tlm_generic_payload &t, sc_time &) {
      ++accesses;
      t.set_response_status(tlm::TLM_OK_RESPONSE);
   }
};

struct irq_monitor : sc_module {
   sc_in<bool> irq;
   std::vector<std::pair<sc_time, bool>> edges;

   SC_HAS_PROCESS(irq_monitor);
   explicit irq_monitor(sc_module_name n) : sc_module(n), irq("irq") {
      SC_METHOD(on_change);
      sensitive << irq;
      dont_initialize();
   }
   void on_change() { edges.emplace_back(sc_time_stamp(), irq.read()); }
};

struct test_master : sc_module {
   tlm_utils::simple_initiator_socket<test_master> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   irq_monitor *monitor = nullptr;
   memory_stub *mem = nullptr;
   bool finished = false;

   // Observer running concurrently with the master, used to sample the DUT
   // while a master access with an annotated delay is in progress.
   sc_event probe_ev;
   sc_time probe_after;
   std::uint32_t probe_set_offset = 0;  // non-zero: hw-set this bit at the probe time
   std::uint32_t probe_set_mask = 0;
   bool probe_irq = false;
   std::uint32_t probe_status = 0;

   SC_HAS_PROCESS(test_master);
   explicit test_master(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") {
      SC_THREAD(run);
      SC_THREAD(observer);
   }

   void observer() {
      for (;;) {
         wait(probe_ev);
         wait(probe_after);
         probe_irq = irq.read();
         probe_status = dut->debug_peek(FX1_ISP_COMMON_IRQ_STATUS_OFFSET);
         if (probe_set_offset) {
            dut->debug_hw_set(probe_set_offset, probe_set_mask);
         }
      }
   }

   // One access issued with a local-time annotation and NO wait afterwards,
   // as a temporally decoupled initiator would issue it.
   tlm::tlm_response_status access_decoupled(tlm::tlm_command cmd, std::uint32_t addr, std::uint32_t &v,
                                             const sc_time &local, sc_time &returned) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      returned = local;
      socket->b_transport(t, returned);
      std::memcpy(&v, b, 4);
      return t.get_response_status();
   }

   tlm::tlm_response_status access(tlm::tlm_command cmd, std::uint64_t addr, unsigned char *data,
                                   unsigned len, const unsigned char *be = nullptr, unsigned be_len = 0,
                                   sc_time *annotated = nullptr) {
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(data);
      t.set_data_length(len);
      t.set_streaming_width(len);
      t.set_byte_enable_ptr(const_cast<unsigned char *>(be));
      t.set_byte_enable_length(be_len);
      t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
      sc_time delay = SC_ZERO_TIME;
      socket->b_transport(t, delay);
      if (annotated) {
         *annotated = delay;
      }
      wait(delay);  // stay synchronised: no temporal decoupling in this bench
      return t.get_response_status();
   }

   std::uint32_t read32(std::uint32_t addr) {
      unsigned char b[4] = {};
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, addr, b, 4), tlm::TLM_OK_RESPONSE);
      std::uint32_t v = 0;
      std::memcpy(&v, b, 4);  // host is little-endian, as is the AXI4-Lite lane order
      return v;
   }

   void write32(std::uint32_t addr, std::uint32_t v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      FX1_CHECK_EQ(access(tlm::TLM_WRITE_COMMAND, addr, b, 4), tlm::TLM_OK_RESPONSE);
   }

   void settle() { wait(1, SC_NS); }

   void run() {
      // i_rst_n asserted at start: writes are ignored, reads return reset values.
      rst_n.write(false);
      wait(10, SC_NS);
      write32(FX1_ISP_COMMON_SCRATCH_OFFSET, 0xFFFFFFFFu);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_SCRATCH_OFFSET), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_VER_ID_OFFSET), 0x00010002);
      rst_n.write(true);
      wait(10, SC_NS);

      test_identity_and_subword();
      test_decode_errors();
      test_invalid_payloads();
      test_latency();
      test_irq_pin();
      test_annotated_delay();
      test_soft_reset_window();
      test_external_reset_mid_run();
      test_debug_transport();

      FX1_CHECK_EQ(mem->accesses, 0);
      finished = true;
      sc_stop();
   }

   void test_identity_and_subword() {
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_VER_DATE_OFFSET), 0x18072026);
      unsigned char b[4] = {};
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_VER_ID_OFFSET + 2, b, 1), tlm::TLM_OK_RESPONSE);
      FX1_CHECK_EQ(b[0], 0x01);  // byte lane 2 of 0x00010002
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_VER_ID_OFFSET, b, 2), tlm::TLM_OK_RESPONSE);
      FX1_CHECK_EQ(b[0] | (b[1] << 8), 0x0002);

      const std::uint32_t s = FX1_ISP_COMMON_SCRATCH_OFFSET;
      write32(s, 0xDEADBEEFu);
      unsigned char one = 0x11;
      FX1_CHECK_EQ(access(tlm::TLM_WRITE_COMMAND, s + 1, &one, 1), tlm::TLM_OK_RESPONSE);
      FX1_CHECK_EQ(read32(s), 0xDEAD11EF);
      unsigned char w[4] = {0x11, 0x22, 0x33, 0x44};
      const unsigned char be[4] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED, TLM_BYTE_DISABLED};
      FX1_CHECK_EQ(access(tlm::TLM_WRITE_COMMAND, s, w, 4, be, 4), tlm::TLM_OK_RESPONSE);
      FX1_CHECK_EQ(read32(s), 0xDE331111);
      // A 4-byte access ignores address bits [1:0] (HAS Table 7-10, CSR-23).
      FX1_CHECK_EQ(read32(s + 2), 0xDE331111);
      write32(s + 3, 0x01020304u);
      FX1_CHECK_EQ(read32(s), 0x01020304);
      // Unmapped words read zero and ignore writes.
      write32(0x0024, 0xFFFFFFFFu);
      FX1_CHECK_EQ(read32(0x0024), 0);
      FX1_CHECK_EQ(read32(0xFFFC), 0);
   }

   void test_decode_errors() {
      unsigned char b[8] = {};
      // Crosses the word: not one AXI4-Lite beat.
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_SCRATCH_OFFSET + 3, b, 2),
                   tlm::TLM_BURST_ERROR_RESPONSE);
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_SCRATCH_OFFSET, b, 8),
                   tlm::TLM_BURST_ERROR_RESPONSE);
      // Outside the 64 KiB aperture: a routing error of the integrator.
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, 0x10000u, b, 4), tlm::TLM_ADDRESS_ERROR_RESPONSE);
   }

   void test_invalid_payloads() {
      const auto off = FX1_ISP_COMMON_SCRATCH_OFFSET;
      write32(off, 0x12345678);
      unsigned char bytes[4] = {0xFF, 0xFF, 0xFF, 0xFF};
      const unsigned char be = TLM_BYTE_ENABLED;
      FX1_CHECK_EQ(access(tlm::TLM_WRITE_COMMAND, off, bytes, 4, &be, 0),
                   tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
      FX1_CHECK_EQ(read32(off), 0x12345678);
      FX1_CHECK_EQ(access(tlm::TLM_READ_COMMAND, off, nullptr, 4), tlm::TLM_GENERIC_ERROR_RESPONSE);
      FX1_CHECK_EQ(access(tlm::TLM_WRITE_COMMAND, off, nullptr, 4), tlm::TLM_GENERIC_ERROR_RESPONSE);
      FX1_CHECK_EQ(read32(off), 0x12345678);
      tlm::tlm_generic_payload t;
      t.set_command(tlm::TLM_READ_COMMAND);
      t.set_address(off);
      t.set_data_length(4);
      t.set_streaming_width(4);
      FX1_CHECK_EQ(socket->transport_dbg(t), 0); // no data buffer: no access
   }

   void test_latency() {
      unsigned char b[4] = {};
      sc_time d;
      access(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_VER_ID_OFFSET, b, 4, nullptr, 0, &d);
      FX1_CHECK(d == dut->params().csr_latency);
   }

   void test_irq_pin() {
      const std::size_t e0 = monitor->edges.size();
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK);
      dut->debug_hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK);
      settle();
      FX1_CHECK(!irq.read());  // masked, still latched
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET), FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK);
      dut->debug_hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
      settle();
      FX1_CHECK(irq.read());
      write32(FX1_ISP_DMA_IRQ_EN_OFFSET, FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_DONE_BIT);
      dut->debug_hw_set(FX1_ISP_DMA_IRQ_STAT_OFFSET, FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT);
      write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
      settle();
      FX1_CHECK(irq.read());  // DMA group keeps the line asserted
      write32(FX1_ISP_DMA_IRQ_STAT_OFFSET, FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT);
      settle();
      FX1_CHECK(!irq.read());
      // One rising and one falling edge over the sequence.
      FX1_CHECK_EQ(monitor->edges.size() - e0, 2);
      // DEC-15: an error flag raises error_irq; with its enable set the pin rises.
      write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, 0x7);
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_ERROR_EN_MASK);
      dut->debug_hw_hold(FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, true);
      settle();
      FX1_CHECK(irq.read());
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_ERROR_MASK,
                   FX1_ISP_COMMON_STATUS_ERROR_MASK);
      write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK);
      settle();
      FX1_CHECK(!irq.read());
      dut->debug_hw_hold(FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, false);
      write32(FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_ERROR_MASK, 0);
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, 0);
   }

   // Review finding: an access carrying an annotated delay must take effect at
   // that time, not when b_transport is called.
   void test_annotated_delay() {
      const sc_time local(20, SC_NS);
      const std::uint32_t fd = FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK;
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK);
      dut->debug_hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, fd);
      settle();
      FX1_CHECK(irq.read());

      // W1C with 20 ns of local time: at +5 ns IRQ and status are unchanged.
      const std::size_t e0 = monitor->edges.size();
      const sc_time t0 = sc_time_stamp();
      probe_after = sc_time(5, SC_NS);
      probe_set_offset = 0;
      probe_ev.notify();
      std::uint32_t v = fd;
      sc_time returned;
      FX1_CHECK_EQ(access_decoupled(tlm::TLM_WRITE_COMMAND, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, v, local, returned),
                   tlm::TLM_OK_RESPONSE);
      FX1_CHECK(probe_irq);
      FX1_CHECK_EQ(probe_status & fd, fd);
      FX1_CHECK(returned == dut->params().csr_latency);  // local time consumed by the target
      wait(returned);
      settle();
      FX1_CHECK(!irq.read());
      FX1_CHECK_EQ(monitor->edges.size() - e0, 1);
      if (monitor->edges.size() > e0) {
         FX1_CHECK(monitor->edges.back().first == t0 + local);
         FX1_CHECK(!monitor->edges.back().second);
      }

      // A read with 20 ns of local time sees a hardware event at +10 ns.
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, 0);
      const std::uint32_t sr = FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK;
      probe_after = sc_time(10, SC_NS);
      probe_set_offset = FX1_ISP_COMMON_IRQ_STATUS_OFFSET;
      probe_set_mask = sr;
      probe_ev.notify();
      v = 0;
      FX1_CHECK_EQ(access_decoupled(tlm::TLM_READ_COMMAND, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, v, local, returned),
                   tlm::TLM_OK_RESPONSE);
      FX1_CHECK_EQ(v & sr, sr);
      wait(returned);
      probe_set_offset = 0;
      write32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, 0x7);
   }

   void test_soft_reset_window() {
      const sc_time period = dut->params().core_period;
      write32(FX1_ISP_COMMON_SCRATCH_OFFSET, 0xA5A5A5A5u);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      // Align to a cycle boundary so the window arithmetic is exact.
      const sc_time into = sc_time_stamp() % period;
      if (into != SC_ZERO_TIME) {
         wait(period - into);
      }
      unsigned char b[4];
      const std::uint32_t v = FX1_ISP_DMA_CTRL_SOFT_RESET_MASK | 0x3F00u;
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(tlm::TLM_WRITE_COMMAND);
      t.set_address(FX1_ISP_DMA_CTRL_OFFSET);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time delay = SC_ZERO_TIME;
      const sc_time t0 = sc_time_stamp();
      socket->b_transport(t, delay);  // applied at t0
      FX1_CHECK(dut->debug_soft_reset_active());
      FX1_CHECK_EQ(dut->debug_peek(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(dut->debug_peek(FX1_ISP_COMMON_SCRATCH_OFFSET), 0xA5A5A5A5u);
      wait(t0 + period * 31 - sc_time_stamp());
      FX1_CHECK(dut->debug_soft_reset_active());
      wait(period);
      FX1_CHECK(!dut->debug_soft_reset_active());  // exactly 32 core cycles
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_CTRL_OFFSET), 0x3F00);
   }

   void test_external_reset_mid_run() {
      write32(FX1_ISP_COMMON_SCRATCH_OFFSET, 0x12345678u);
      write32(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      // Every command register is consumed by a block model in the wrapper; the
      // AEC commit stays pending (reads 1) until an SOF that never comes here.
      write32(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK);
      FX1_CHECK_EQ(dut->debug_pending_commands(), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_AEC_CTRL_OFFSET), FX1_ISP_AEC_CTRL_COMMIT_MASK);
      write32(FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK);
      dut->debug_hw_set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
      settle();
      FX1_CHECK(irq.read());
      rst_n.write(false);
      wait(8, SC_NS);  // 4 core cycles, HAS Table 6-11 minimum
      FX1_CHECK(!irq.read());
      rst_n.write(true);
      settle();
      FX1_CHECK(!irq.read());
      FX1_CHECK_EQ(dut->debug_pending_commands(), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_SCRATCH_OFFSET), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_IRQ_EN_OFFSET), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0);
      FX1_CHECK_EQ(read32(FX1_ISP_DMA_CTRL_OFFSET), 0x3F00);
      FX1_CHECK_EQ(read32(FX1_ISP_AEC_CTRL_OFFSET), 0);  // pending commit dropped
   }

   void test_debug_transport() {
      unsigned char b[4] = {};
      tlm::tlm_generic_payload t;
      t.set_command(tlm::TLM_READ_COMMAND);
      t.set_address(FX1_ISP_COMMON_VER_ID_OFFSET);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      FX1_CHECK_EQ(socket->transport_dbg(t), 4);
      FX1_CHECK_EQ(b[0] | (b[2] << 16), 0x00010002);
      t.set_command(tlm::TLM_WRITE_COMMAND);
      FX1_CHECK_EQ(socket->transport_dbg(t), 0);  // debug writes unsupported
   }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");

   fx1_isp_tlm dut("isp", fx1_isp_params{});
   memory_stub mem("mem");
   irq_monitor monitor("monitor");
   test_master master("master");

   master.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   master.rst_n(rst_n);
   master.irq(irq);
   monitor.irq(irq);
   master.dut = &dut;
   master.monitor = &monitor;
   master.mem = &mem;

   sc_start(1, SC_MS);
   FX1_CHECK(master.finished);
   return fx1_test::summary("fx1_isp_test_csr_tlm");
}
