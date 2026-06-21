//Author: QuanNH107

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <dma_tlm.h>

using namespace cdc::components;

namespace {

std::string hex32(uint32_t value) {
   std::ostringstream os;
   os << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
   return os.str();
}

void append_u32(std::vector<uint8_t> &data, uint32_t value) {
   data.push_back(static_cast<uint8_t>(value & 0xFFu));
   data.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
   data.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
   data.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void append_dmamov(std::vector<uint8_t> &program, uint8_t reg, uint32_t value) {
   program.push_back(0xBC);
   program.push_back(static_cast<uint8_t>(reg << 3));
   append_u32(program, value);
}

uint32_t make_ccr_4x32_incrementing() {
   return dma_tlm::CCR_SRC_INC |
          (2u << dma_tlm::CCR_SRC_BURST_SIZE_SHIFT) |
          (3u << dma_tlm::CCR_SRC_BURST_LEN_SHIFT) |
          dma_tlm::CCR_DST_INC |
          (2u << dma_tlm::CCR_DST_BURST_SIZE_SHIFT) |
          (3u << dma_tlm::CCR_DST_BURST_LEN_SHIFT);
}

uint32_t debug_inst0(uint8_t byte0, uint8_t byte1, bool channel_thread = false, unsigned int channel = 0) {
   return (static_cast<uint32_t>(byte0) << 16) |
          (static_cast<uint32_t>(byte1) << 24) |
          ((channel & 0x7u) << 8) |
          (channel_thread ? 1u : 0u);
}

uint32_t debug_inst1(uint32_t imm) {
   return imm;
}

class ram_tlm : public sc_core::sc_module {
public:
   tlm_utils::simple_target_socket<ram_tlm> target_socket;

   explicit ram_tlm(sc_core::sc_module_name name, std::size_t size)
       : sc_core::sc_module(name)
       , target_socket("target_socket")
       , m_data(size, 0) {
      target_socket.register_b_transport(this, &ram_tlm::b_transport);
   }

   void write_bytes(uint32_t addr, const std::vector<uint8_t> &bytes) {
      std::copy(bytes.begin(), bytes.end(), m_data.begin() + addr);
   }

   std::vector<uint8_t> read_bytes(uint32_t addr, std::size_t length) const {
      return std::vector<uint8_t>(m_data.begin() + addr, m_data.begin() + addr + length);
   }

private:
   std::vector<uint8_t> m_data;

   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
      const uint64_t addr = trans.get_address();
      const unsigned int length = trans.get_data_length();
      unsigned char *ptr = trans.get_data_ptr();

      delay += sc_core::sc_time(1, sc_core::SC_NS);

      if (ptr == nullptr) {
         trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
         return;
      }

      if (addr > m_data.size() || length > m_data.size() - addr) {
         trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
         return;
      }

      switch (trans.get_command()) {
      case tlm::TLM_READ_COMMAND:
         std::memcpy(ptr, m_data.data() + addr, length);
         trans.set_response_status(tlm::TLM_OK_RESPONSE);
         break;
      case tlm::TLM_WRITE_COMMAND:
         std::memcpy(m_data.data() + addr, ptr, length);
         trans.set_response_status(tlm::TLM_OK_RESPONSE);
         break;
      default:
         trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
         break;
      }
   }
};

class Testbench : public sc_core::sc_module {
public:
   tlm_utils::simple_initiator_socket<Testbench> initiator_socket;
   sc_core::sc_in<bool> reset_n;
   sc_core::sc_in<uint32_t> irq;
   sc_core::sc_in<bool> irq_abort;

   SC_HAS_PROCESS(Testbench);

   Testbench(sc_core::sc_module_name name, ram_tlm &ram)
       : sc_core::sc_module(name)
       , initiator_socket("initiator_socket")
       , reset_n("reset_n")
       , irq("irq")
       , irq_abort("irq_abort")
       , m_ram(ram)
       , m_errors(0) {
      SC_THREAD(run);
   }

private:
   static const uint32_t PROGRAM_ADDR = 0x00000100;
   static const uint32_t SRC_ADDR = 0x00000200;
   static const uint32_t DST_ADDR = 0x00000300;
   static const uint32_t EVENT_DONE = 3;

   ram_tlm &m_ram;
   unsigned int m_errors;

   void run() {
      while (!reset_n.read()) {
         wait(reset_n.posedge_event());
      }
      wait(sc_core::SC_ZERO_TIME);

      std::cout << "\n[TB] ARM CoreLink DMA-330 LT functional tests begin\n\n";

      unsigned int errors_before = m_errors;
      std::cout << "[TB] Test 1: reset and ID/configuration registers\n";
      expect_eq("DSR reset status", read32(dma_tlm::DSR) & 0xFu, dma_tlm::STATUS_STOPPED);
      expect_eq("CSR0 reset status", read32(dma_tlm::CSR0) & 0xFu, dma_tlm::STATUS_STOPPED);
      expect_eq("CCR0 reset value", read32(dma_tlm::CCR0), 0x00800200u);
      expect_eq("periph_id_0", read32(dma_tlm::PERIPH_ID0), dma_tlm::PERIPH_ID0_VALUE);
      expect_eq("periph_id_1", read32(dma_tlm::PERIPH_ID1), dma_tlm::PERIPH_ID1_VALUE);
      expect_eq("periph_id_2", read32(dma_tlm::PERIPH_ID2), dma_tlm::PERIPH_ID2_VALUE);
      expect_eq("pcell_id_0", read32(dma_tlm::PCELL_ID0), dma_tlm::PCELL_ID0_VALUE);
      print_test_result("Test 1", errors_before);

      errors_before = m_errors;
      std::cout << "\n[TB] Test 2: debug-launched DMA memory transfer with completion IRQ\n";
      load_transfer_program();
      write32(dma_tlm::INTEN, 1u << EVENT_DONE);
      write32(dma_tlm::DBGINST0, debug_inst0(0xA0, 0x00));
      write32(dma_tlm::DBGINST1, debug_inst1(PROGRAM_ADDR));
      write32(dma_tlm::DBGCMD, 0);
      wait(200, sc_core::SC_NS);
      settle_outputs();

      const std::vector<uint8_t> expected = m_ram.read_bytes(SRC_ADDR, 32);
      const std::vector<uint8_t> actual = m_ram.read_bytes(DST_ADDR, 32);
      expect_buffer("copied payload", actual, expected);
      expect_eq("CSR0 stopped after DMAEND", read32(dma_tlm::CSR0) & 0xFu, dma_tlm::STATUS_STOPPED);
      expect_eq("SAR0 final", read32(dma_tlm::SAR0), SRC_ADDR + 32);
      expect_eq("DAR0 final", read32(dma_tlm::DAR0), DST_ADDR + 32);
      expect_eq("FSRC after successful transfer", read32(dma_tlm::FSRC), 0);
      expect_eq("INT_EVENT_RIS done bit", read32(dma_tlm::INT_EVENT_RIS), 1u << EVENT_DONE);
      expect_eq("INTMIS done bit", read32(dma_tlm::INTMIS), 1u << EVENT_DONE);
      expect_signal("IRQ vector", irq.read(), 1u << EVENT_DONE);
      expect_signal("irq_abort after successful transfer", irq_abort.read(), false);
      write32(dma_tlm::INTCLR, 1u << EVENT_DONE);
      settle_outputs();
      expect_eq("INTMIS after INTCLR", read32(dma_tlm::INTMIS), 0);
      expect_signal("IRQ vector after INTCLR", irq.read(), 0);
      print_test_result("Test 2", errors_before);

      errors_before = m_errors;
      std::cout << "\n[TB] Test 3: read-only channel registers ignore APB writes\n";
      write32(dma_tlm::SAR0, 0xAAAAAAAAu);
      expect_eq("SAR0 remains architectural value", read32(dma_tlm::SAR0), SRC_ADDR + 32);
      write32(dma_tlm::CCR0, 0);
      expect_eq("CCR0 remains architectural value", read32(dma_tlm::CCR0), make_ccr_4x32_incrementing());
      print_test_result("Test 3", errors_before);

      std::cout << "\n[TB] Result: " << (m_errors == 0 ? "PASS" : "FAIL")
                << " (" << m_errors << " error(s))\n";

      sc_core::sc_stop();
   }

   void load_transfer_program() {
      std::vector<uint8_t> source(32, 0);
      for (std::size_t i = 0; i < source.size(); ++i) {
         source[i] = static_cast<uint8_t>(0x40u + i);
      }

      std::vector<uint8_t> zeros(32, 0);
      m_ram.write_bytes(SRC_ADDR, source);
      m_ram.write_bytes(DST_ADDR, zeros);

      std::vector<uint8_t> program;
      append_dmamov(program, 1, make_ccr_4x32_incrementing());
      append_dmamov(program, 0, SRC_ADDR);
      append_dmamov(program, 2, DST_ADDR);
      program.push_back(0x20);
      program.push_back(0x01);
      const uint32_t loop_body = PROGRAM_ADDR + static_cast<uint32_t>(program.size());
      program.push_back(0x04);
      program.push_back(0x08);
      const uint32_t loop_end = PROGRAM_ADDR + static_cast<uint32_t>(program.size());
      program.push_back(0x38);
      program.push_back(static_cast<uint8_t>(loop_end - loop_body));
      program.push_back(0x13);
      program.push_back(0x34);
      program.push_back(static_cast<uint8_t>(EVENT_DONE << 3));
      program.push_back(0x00);
      m_ram.write_bytes(PROGRAM_ADDR, program);
   }

   void settle_outputs() {
      wait(sc_core::SC_ZERO_TIME);
      wait(sc_core::SC_ZERO_TIME);
   }

   uint32_t read32(uint32_t addr) {
      uint32_t value = 0;
      tlm::tlm_generic_payload trans;
      sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

      trans.set_command(tlm::TLM_READ_COMMAND);
      trans.set_address(addr);
      trans.set_data_ptr(reinterpret_cast<unsigned char *>(&value));
      trans.set_data_length(sizeof(value));
      trans.set_streaming_width(sizeof(value));
      trans.set_byte_enable_ptr(nullptr);
      trans.set_dmi_allowed(false);
      trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

      initiator_socket->b_transport(trans, delay);
      wait(delay);

      if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
         ++m_errors;
         std::cout << "[TB][FAIL] read " << hex32(addr) << " response=" << trans.get_response_string() << '\n';
      }

      std::cout << sc_core::sc_time_stamp() << " [TB] READ  " << hex32(addr) << " -> " << hex32(value)
                << '\n';
      return value;
   }

   void write32(uint32_t addr, uint32_t data) {
      tlm::tlm_generic_payload trans;
      sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

      trans.set_command(tlm::TLM_WRITE_COMMAND);
      trans.set_address(addr);
      trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
      trans.set_data_length(sizeof(data));
      trans.set_streaming_width(sizeof(data));
      trans.set_byte_enable_ptr(nullptr);
      trans.set_dmi_allowed(false);
      trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

      initiator_socket->b_transport(trans, delay);
      wait(delay);

      if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
         ++m_errors;
         std::cout << "[TB][FAIL] write " << hex32(addr) << " response=" << trans.get_response_string() << '\n';
      }

      std::cout << sc_core::sc_time_stamp() << " [TB] WRITE " << hex32(addr) << " <= " << hex32(data)
                << '\n';
   }

   void expect_eq(const std::string &label, uint32_t actual, uint32_t expected) {
      if (actual != expected) {
         ++m_errors;
         std::cout << "[TB][FAIL] " << label << ": expected " << hex32(expected) << ", got "
                   << hex32(actual) << '\n';
         return;
      }

      std::cout << "[TB][PASS] " << label << " = " << hex32(actual) << '\n';
   }

   void expect_signal(const std::string &label, uint32_t actual, uint32_t expected) {
      if (actual != expected) {
         ++m_errors;
         std::cout << "[TB][FAIL] " << label << ": expected " << hex32(expected) << ", got "
                   << hex32(actual) << '\n';
         return;
      }

      std::cout << "[TB][PASS] " << label << " = " << hex32(actual) << '\n';
   }

   void expect_signal(const std::string &label, bool actual, bool expected) {
      if (actual != expected) {
         ++m_errors;
         std::cout << "[TB][FAIL] " << label << ": expected " << expected << ", got " << actual << '\n';
         return;
      }

      std::cout << "[TB][PASS] " << label << " = " << actual << '\n';
   }

   void expect_buffer(const std::string &label, const std::vector<uint8_t> &actual,
                      const std::vector<uint8_t> &expected) {
      if (actual != expected) {
         ++m_errors;
         std::cout << "[TB][FAIL] " << label << " mismatch\n";
         return;
      }

      std::cout << "[TB][PASS] " << label << " matches " << actual.size() << " byte(s)\n";
   }

   void print_test_result(const std::string &name, unsigned int errors_before) const {
      std::cout << "[TB] " << name << ' ' << (m_errors == errors_before ? "PASS" : "FAIL") << '\n';
   }
};

} // namespace

int sc_main(int argc, char *argv[]) {
   (void)argc;
   (void)argv;

   dma_tlm dut("dma_tlm");
   ram_tlm ram("ram", 4096);
   Testbench tb("tb", ram);

   sc_core::sc_signal<bool> rst_n_sig("rst_n_sig");
   sc_core::sc_signal<uint32_t> irq_sig("irq_sig");
   sc_core::sc_signal<bool> irq_abort_sig("irq_abort_sig");

   tb.initiator_socket.bind(dut.target_socket);
   dut.master_socket.bind(ram.target_socket);

   tb.reset_n(rst_n_sig);
   tb.irq(irq_sig);
   tb.irq_abort(irq_abort_sig);

   dut.reset_n(rst_n_sig);
   dut.irq(irq_sig);
   dut.irq_abort(irq_abort_sig);

   rst_n_sig.write(false);
   sc_core::sc_start(10, sc_core::SC_NS);
   rst_n_sig.write(true);
   sc_core::sc_start();

   return 0;
}
