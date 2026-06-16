#include "systemc"
#include "tlm.h"
#include "spi_tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

using namespace sc_core;
using namespace std;
using namespace cdc::components;

SC_MODULE(Tester) {
   tlm_utils::simple_initiator_socket<Tester> socket;
   sc_out<bool> reset_out;
   sc_in<bool> irq_in;

   SC_CTOR(Tester)
       : socket("socket") {
      SC_THREAD(run_test);
      SC_METHOD(irq_monitor);
      sensitive << irq_in;
   }

   void irq_monitor() {
      cout << "@" << sc_time_stamp() << " [Tester] IRQ signal changed to: " << irq_in.read() << endl;
   }

   void run_test() {
      uint16_t data;
      tlm::tlm_response_status status;

      // 1. Initial Reset (active low)
      reset_out.write(true);
      wait(10, SC_NS);
      reset_out.write(false);
      wait(20, SC_NS);
      reset_out.write(true);
      wait(10, SC_NS);

      cout << "\n--- STRESS TEST 1: BITRATE VALIDATION ---" << endl;
      data = (255 << 8) | 0x07;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x00, data); // CR0
      data = 254;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x10, data); // CPSR
      data = 0x02;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x04, data); // SSE=1

      sc_time start_t = sc_time_stamp();
      data = 0x11;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x08, data);

      while (true) {
         do_transaction(tlm::TLM_READ_COMMAND, 0x0C, data);
         if (data & 0x04)
            break;
         wait(10, SC_NS);
      }

      sc_time end_t = sc_time_stamp();
      cout << "   Slow Transmission took: " << (end_t - start_t) << endl;

      cout << "\n--- STRESS TEST 2: WORD SIZE MASKING (4-BIT MODE) ---" << endl;
      reset_out.write(false);
      wait(20, SC_NS);
      reset_out.write(true);
      wait(10, SC_NS);
      data = 0x02;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x04, data);

      data = 0x03;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x00, data);
      data = 0xFF;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x08, data);

      wait(500, SC_NS);
      do_transaction(tlm::TLM_READ_COMMAND, 0x08, data);
      cout << "   Sent 0xFF in 4-bit mode, Read back: 0x" << hex << data << dec << endl;

      cout << "\n--- STRESS TEST 3: TX FIFO OVERRUN (ERROR CHECK) ---" << endl;
      reset_out.write(false);
      wait(20, SC_NS);
      reset_out.write(true);
      wait(10, SC_NS);
      // Keep SSE=0 so the transmit thread doesn't consume data yet
      data = 0x00;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x04, data);

      // Fill to 8
      for (int i = 0; i < 8; i++) {
         data = i;
         do_transaction(tlm::TLM_WRITE_COMMAND, 0x08, data);
      }

      // Try 9th write
      status = do_transaction(tlm::TLM_WRITE_COMMAND, 0x08, data);
      if (status == tlm::TLM_GENERIC_ERROR_RESPONSE) {
         cout << "   SUCCESS: System correctly rejected 9th write to full FIFO" << endl;
      } else {
         cout << "   FAILURE: System accepted 9th write to full FIFO" << endl;
      }

      // Now enable SSE so the transmit thread can clear the FIFO for the next test
      data = 0x02;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x04, data);
      wait(500, SC_NS);

      cout << "\n--- STRESS TEST 4: INTERRUPT CLEARING (ICR) ---" << endl;
      // Reset to clear RX FIFO from Test 3
      reset_out.write(false); wait(20, SC_NS); reset_out.write(true); wait(10, SC_NS);

      data = 0x01; do_transaction(tlm::TLM_WRITE_COMMAND, 0x14, data);
      do_transaction(tlm::TLM_READ_COMMAND, 0x08, data);
      wait(1, SC_NS);
      cout << "   IRQ Pin state after error: " << irq_in.read() << endl;


      data = 0x01;
      do_transaction(tlm::TLM_WRITE_COMMAND, 0x20, data);
      wait(1, SC_NS);
      cout << "   IRQ Pin state after ICR clear: " << irq_in.read() << endl;

      cout << "\n--- ALL STRESS TESTS FINISHED ---" << endl;
      sc_stop();
   }

   tlm::tlm_response_status do_transaction(tlm::tlm_command cmd, uint64_t addr, uint16_t &data) {
      tlm::tlm_generic_payload trans;
      sc_time delay = SC_ZERO_TIME;
      trans.set_command(cmd);
      trans.set_address(addr);
      trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
      trans.set_data_length(2);
      trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

      socket->b_transport(trans, delay);
      wait(delay);
      return trans.get_response_status();
   }
};

SC_MODULE(DummyPeripheral) {
   tlm_utils::simple_target_socket<DummyPeripheral> socket;
   SC_CTOR(DummyPeripheral)
       : socket("socket") {
      socket.register_b_transport(this, &DummyPeripheral::b_transport);
   }
   void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay) {
      uint16_t *ptr = reinterpret_cast<uint16_t *>(trans.get_data_ptr());
      *ptr = (*ptr) | 0xF000;
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
   }
};

int sc_main(int argc, char *argv[]) {
   Tester tester("tester");
   spi_tlm spi("spi");
   DummyPeripheral peri("peri");

   sc_signal<bool> reset_sig;
   sc_signal<bool> irq_sig;

   tester.socket.bind(spi.from_apb_socket);
   spi.to_peri_socket.bind(peri.socket);

   tester.reset_out(reset_sig);
   spi.reset_n(reset_sig);
   spi.irq(irq_sig);
   tester.irq_in(irq_sig);

   sc_start();
   return 0;
}
