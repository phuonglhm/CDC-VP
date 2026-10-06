// SPDX-License-Identifier: Apache-2.0
// Minimal platform-style user of the installed package: instantiates the
// model, binds every port, drives it with the installed reference driver
// through the CSR socket and checks the ID register and the derived geometry.
#include <cstdint>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_tlm.h"

using namespace sc_core;

struct ram : sc_module {  // memory stub for the two DMA masters (not used here)
   tlm_utils::simple_target_socket<ram> idma, odma;
   explicit ram(sc_module_name n) : sc_module(n), idma("idma"), odma("odma") {
      idma.register_b_transport(this, &ram::b);
      odma.register_b_transport(this, &ram::b);
   }
   void b(tlm::tlm_generic_payload &t, sc_time &) { t.set_response_status(tlm::TLM_OK_RESPONSE); }
};

struct cpu : sc_module {
   tlm_utils::simple_initiator_socket<cpu> socket;
   sc_out<bool> rst_n;
   int result = 1;
   SC_HAS_PROCESS(cpu);
   explicit cpu(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n") { SC_THREAD(run); }
   std::uint32_t access(tlm::tlm_command c, std::uint32_t a, std::uint32_t v) {
      unsigned char d[4];
      std::memcpy(d, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(c);
      t.set_address(a);
      t.set_data_ptr(d);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time delay = SC_ZERO_TIME;
      socket->b_transport(t, delay);
      wait(delay);
      std::memcpy(&v, d, 4);
      return v;
   }
   static std::uint32_t rd(void *c, std::uint32_t a) { return static_cast<cpu *>(c)->access(tlm::TLM_READ_COMMAND, a, 0); }
   static void wr(void *c, std::uint32_t a, std::uint32_t v) { static_cast<cpu *>(c)->access(tlm::TLM_WRITE_COMMAND, a, v); }
   static void dly(void *c, std::uint32_t n) { static_cast<cpu *>(c)->wait(sc_time(2.0 * n, SC_NS)); }
   void run() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      fx1_isp_dev dev{};
      const fx1_isp_hal hal{&cpu::rd, &cpu::wr, &cpu::dly, this};
      const bool ok = fx1_isp_init(&dev, &hal) == FX1_ISP_OK &&
                      rd(this, FX1_ISP_COMMON_VER_DATE_OFFSET) == FX1_ISP_COMMON_VER_DATE_RESET &&
                      fx1_isp_set_geometry(&dev, 2688, 1520, 3) == FX1_ISP_OK &&
                      rd(this, FX1_ISP_RESIZER_OUT_W_OFFSET) == 2686 && rd(this, FX1_ISP_RESIZER_OUT_H_OFFSET) == 1518;
      std::cout << "fx1_isp package consumer: " << (ok ? "OK" : "FAILED") << "\n";
      result = ok ? 0 : 1;
      sc_stop();
   }
};

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n"), irq("irq");
   cdc::components::fx1_isp::fx1_isp_tlm isp("isp", cdc::components::fx1_isp::fx1_isp_params{});
   ram mem("mem");
   cpu c("cpu");
   c.socket.bind(isp.csr_socket);
   isp.idma_socket.bind(mem.idma);
   isp.odma_socket.bind(mem.odma);
   isp.rst_n(rst_n);
   isp.irq(irq);
   c.rst_n(rst_n);
   sc_start();
   return c.result;
}
