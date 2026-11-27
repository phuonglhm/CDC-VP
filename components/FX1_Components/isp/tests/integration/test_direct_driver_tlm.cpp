// SPDX-License-Identifier: Apache-2.0
// The C driver's direct-input API (plan DIRECT_CSI_ISP D3): fx1_isp_start_direct,
// fx1_isp_direct_arm, fx1_isp_direct_find, on the TLM model, without frames
// (the frame path through the camera manager is covered on fx1_soc:
// fx1_*_camera_mgr_direct_*). Checks:
//   - start_direct: STREAMING, DMA_CTRL with ODMA_EN and without IDMA_EN (the
//     ingress needs IDMA_EN = 0), nothing armed (ODMA_BUF_FREE 0), no IDMA
//     buffer address or stride written
//   - the memory-path calls refuse a direct stream (queue / done / lost:
//     ESTATE, input_ready 0) and the direct calls refuse a memory stream and
//     the CONFIG state (ESTATE)
//   - arm: bad index EINVAL; slot k -> FREE bit k (W1S), DONE acknowledged
//   - find: no DONE output -> ENODATA; NULL index -> EINVAL
//   - start_direct while streaming refused; stop: CONFIG, DMA engines off,
//     the next memory start is a memory stream again
// Usage: fx1_isp_test_direct_driver_tlm

#include <cstdint>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "test_memory.h"

using namespace sc_core;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 4u << 20;

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   fx1_isp_dev dev{};

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") { SC_THREAD(run); }

   std::uint32_t access(tlm::tlm_command cmd, std::uint32_t addr, std::uint32_t v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      socket->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::memcpy(&v, b, 4);
      return v;
   }
   static std::uint32_t hal_read(void *ctx, std::uint32_t off) {
      return static_cast<bench *>(ctx)->access(tlm::TLM_READ_COMMAND, off, 0);
   }
   static void hal_write(void *ctx, std::uint32_t off, std::uint32_t v) {
      static_cast<bench *>(ctx)->access(tlm::TLM_WRITE_COMMAND, off, v);
   }
   static void hal_delay(void *ctx, std::uint32_t cycles) {
      bench *b = static_cast<bench *>(ctx);
      b->wait(b->dut->params().core_period * static_cast<double>(cycles));
   }
   std::uint32_t rd(std::uint32_t off) { return hal_read(this, off); }

   fx1_isp_buffers buffers() const {
      fx1_isp_buffers b{};
      for (unsigned k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
         b.in[k] = mem_base + 0x10000u * k;
         b.y[k] = mem_base + 0x100000u + 0x10000u * k;
         b.uv[k] = b.y[k] + 0x8000u;
      }
      b.in_stride = 128;
      b.y_stride = b.uv_stride = 64;
      b.max_burst_beats = 16;
      return b;
   }

   void run() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      const fx1_isp_hal hal{&bench::hal_read, &bench::hal_write, &bench::hal_delay, this};
      FX1_CHECK_EQ(fx1_isp_init(&dev, &hal), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 64, 32, 0), FX1_ISP_OK);
      const fx1_isp_buffers b = buffers();
      unsigned index = 0;

      // CONFIG: the direct calls refuse.
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, 0), FX1_ISP_ESTATE);
      FX1_CHECK_EQ(fx1_isp_direct_find(&dev, 1, &index), FX1_ISP_ESTATE);

      // Direct stream.
      const std::uint32_t stride_reset = rd(FX1_ISP_IDMA_STRIDE_OFFSET);
      FX1_CHECK(stride_reset != b.in_stride);
      FX1_CHECK_EQ(fx1_isp_start_direct(&dev, &b), FX1_ISP_OK);
      FX1_CHECK_EQ(dev.state, FX1_ISP_STATE_STREAMING);
      FX1_CHECK_EQ(dev.direct, 1);
      const std::uint32_t ctrl = rd(FX1_ISP_DMA_CTRL_OFFSET);
      FX1_CHECK((ctrl & FX1_ISP_DMA_CTRL_ODMA_EN_MASK) != 0);
      FX1_CHECK_EQ(ctrl & FX1_ISP_DMA_CTRL_IDMA_EN_MASK, 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 0xFu, 0u);  // nothing armed by the start
      FX1_CHECK_EQ(rd(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET), 0u);      // no input slot programmed
      FX1_CHECK_EQ(rd(FX1_ISP_IDMA_STRIDE_OFFSET), stride_reset);  // not written
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u), static_cast<std::uint32_t>(b.y[1]));

      // Memory-path calls refuse a direct stream.
      std::uint32_t seq = 0;
      unsigned input = 0;
      FX1_CHECK_EQ(fx1_isp_input_ready(&dev), 0);
      FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_ESTATE);
      FX1_CHECK_EQ(fx1_isp_queue_frame_meta(&dev, 1, 2, &seq), FX1_ISP_ESTATE);
      FX1_CHECK_EQ(fx1_isp_next_done(&dev, &index), FX1_ISP_ESTATE);
      FX1_CHECK_EQ(fx1_isp_next_lost(&dev, &seq, &input), FX1_ISP_ESTATE);
      FX1_CHECK_EQ(fx1_isp_start_direct(&dev, &b), FX1_ISP_ESTATE);  // already streaming
      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_ESTATE);

      // Arm / find.
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, FX1_ISP_NUM_BUFFERS), FX1_ISP_EINVAL);
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, 2), FX1_ISP_OK);
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 0xFu, 0x4u);
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, 0), FX1_ISP_OK);
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 0xFu, 0x5u);
      FX1_CHECK_EQ(fx1_isp_direct_find(&dev, 0x1234u, &index), FX1_ISP_ENODATA);
      FX1_CHECK_EQ(fx1_isp_direct_find(&dev, 0x1234u, nullptr), FX1_ISP_EINVAL);

      // Stop: CONFIG, engines off, back to the memory path.
      int dropped = FX1_ISP_EBUSY;
      for (int tries = 0; tries < 100 && dropped == FX1_ISP_EBUSY; ++tries) dropped = fx1_isp_stop(&dev);
      FX1_CHECK_EQ(dropped, 0);
      FX1_CHECK_EQ(dev.state, FX1_ISP_STATE_CONFIG);
      FX1_CHECK_EQ(dev.direct, 0);
      FX1_CHECK_EQ(rd(FX1_ISP_DMA_CTRL_OFFSET) & (FX1_ISP_DMA_CTRL_ODMA_EN_MASK | FX1_ISP_DMA_CTRL_IDMA_EN_MASK), 0u);
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, 0), FX1_ISP_ESTATE);

      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
      FX1_CHECK_EQ(dev.direct, 0);
      FX1_CHECK_EQ(fx1_isp_direct_arm(&dev, 0), FX1_ISP_ESTATE);    // a memory stream
      FX1_CHECK(fx1_isp_input_ready(&dev) != 0);                   // the memory path works again
      dropped = FX1_ISP_EBUSY;
      for (int tries = 0; tries < 100 && dropped == FX1_ISP_EBUSY; ++tries) dropped = fx1_isp_stop(&dev);
      FX1_CHECK_EQ(dropped, 0);
      sc_stop();
   }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_params params{};
   fx1_isp_tlm dut("isp", params);
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.irq(irq);
   tb.dut = &dut;
   sc_start();
   return fx1_test::summary("fx1_isp_test_direct_driver_tlm");
}
