// Author: hoangv11
// Verified by: quannh107
#ifndef SPI_TLM_H
#define SPI_TLM_H

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>

namespace cdc::components {

class spi_tlm : public sc_core::sc_module {
public:
   tlm_utils::simple_target_socket<spi_tlm> from_apb_socket;
   tlm_utils::simple_initiator_socket<spi_tlm> to_peri_socket;

   sc_core::sc_out<bool> irq;
   sc_core::sc_in<bool> reset_n;

   SC_HAS_PROCESS(spi_tlm);
   explicit spi_tlm(sc_core::sc_module_name name);

   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

private:
   static constexpr int FIFO_SIZE = 8; // 8-location deep
   const std::uint64_t base_addr = 0x0;
   const std::uint32_t clk = 100 * 1000000; // 100 MHz

   std::uint16_t reg_cr0;
   std::uint16_t reg_cr1;
   std::uint16_t reg_dr;
   std::uint16_t reg_sr;
   std::uint16_t reg_cpsr;
   std::uint16_t reg_imsc;
   std::uint16_t reg_ris;
   std::uint16_t reg_mis;
   std::uint16_t reg_icr;
   std::uint16_t reg_dmacr;

   // readonly IDs
   const std::uint8_t reg_periph_id0 = 0x22;
   const std::uint8_t reg_periph_id1 = 0x10;
   const std::uint8_t reg_periph_id2 = 0x34;
   const std::uint8_t reg_periph_id3 = 0x00;

   // readonly IDs
   const std::uint8_t reg_cell_id0 = 0x0d;
   const std::uint8_t reg_cell_id1 = 0xf0;
   const std::uint8_t reg_cell_id2 = 0x05;
   const std::uint8_t reg_cell_id3 = 0xb1;

   sc_core::sc_fifo<std::uint16_t> rx_fifo;
   sc_core::sc_fifo<std::uint16_t> tx_fifo;

   sc_core::sc_event transmission_event;
   void transmit(); // bound to a thread to be called whenever SPI receives a payload

   bool write_reg(std::uint8_t offset, std::uint16_t value);
   std::uint16_t read_reg(std::uint16_t offset);

   sc_core::sc_event possible_intr_event;
   void update_intr();

   sc_core::sc_event status_event;
   void update_status_reg();
   // this is 1 if the simulated hardware is currently transmitting bits to peripheral, 0 otherwise.
   bool is_transmitting = false;

   void handle_reset();
};

} // namespace cdc::components

#endif
