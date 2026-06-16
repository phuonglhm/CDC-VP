// Author: hoangv11
// Verified by: quannh107
#include "spi_tlm.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace cdc::components {

spi_tlm::spi_tlm(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , from_apb_socket("from_apb_socket")
    , to_peri_socket("to_peri_socket")
    , irq("irq")
    , reset_n("reset_n")
    , rx_fifo(FIFO_SIZE)
    , tx_fifo(FIFO_SIZE) {

   // register the callback function
   from_apb_socket.register_b_transport(this, &spi_tlm::b_transport);

   SC_THREAD(transmit);

   SC_METHOD(update_intr);
   sensitive << possible_intr_event;
   dont_initialize();

   SC_METHOD(update_status_reg);
   sensitive << status_event;
   dont_initialize();

   SC_METHOD(handle_reset);
   sensitive << reset_n.neg();
   // no dont_initialize() to reset at start time. neat, huh?
}

void spi_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   tlm::tlm_command cmd = trans.get_command();
   sc_dt::uint64 addr = trans.get_address();
   unsigned char *ptr = trans.get_data_ptr();
   unsigned int len = trans.get_data_length();

   // msg.len should be at most 2 bytes
   if (addr > 0xffc || len > 2) {
      SC_REPORT_ERROR("TLM-2", "Target does not support given generic payload transaction");
      return;
   }

   switch (cmd) {
   case tlm::TLM_WRITE_COMMAND: {
      std::uint16_t val = 0;
      std::memcpy(&val, ptr, std::min(len, (unsigned int)sizeof(val)));
      bool wr_success = write_reg(static_cast<std::uint8_t>(addr - base_addr), val);
      if (!wr_success) {
         trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
         break;
      }
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
      break;
   }

   case tlm::TLM_READ_COMMAND: {
      std::uint16_t rd_data = read_reg(static_cast<std::uint16_t>(addr));
      std::memcpy(ptr, &rd_data, std::min(len, (unsigned int)sizeof(rd_data)));
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
      break;
   }

   default:
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
      break;
   }

   delay += sc_core::sc_time(20, sc_core::SC_NS); // add negligible time since read/write is fast
}

void spi_tlm::transmit() {
   while (true) {
      if (tx_fifo.num_available() == 0 || !(reg_cr1 & 0x2)) {
         is_transmitting = false;
         status_event.notify(sc_core::SC_ZERO_TIME);
         wait(transmission_event);
         continue;
      }

      is_transmitting = true;
      status_event.notify(sc_core::SC_ZERO_TIME);

      std::uint16_t data_buffer = tx_fifo.read();

      int bits_per_frame = (reg_cr0 & 0xF) + 1;
      std::uint16_t mask = (bits_per_frame == 16) ? 0xFFFF : (1 << bits_per_frame) - 1;
      std::uint32_t cpsr = (reg_cpsr > 0) ? reg_cpsr : 2;
      std::uint32_t scr = (reg_cr0 >> 8) & 0xFF;
      double ssp_clk = (double)clk / (cpsr * (1 + scr));
      double transmission_time = (double)bits_per_frame / ssp_clk;
      sc_core::sc_time delay = sc_core::sc_time(transmission_time, sc_core::SC_SEC);

      // 4. Peripheral interaction
      tlm::tlm_generic_payload trans;
      trans.set_write();
      trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data_buffer));
      trans.set_data_length(sizeof(data_buffer));
      to_peri_socket->b_transport(trans, delay);

      // 5. Wait for total hardware time (Shift time + Peripheral time)
      wait(delay);

      // 6. Store result and update status
      if (rx_fifo.num_free() == 0) {
         reg_ris |= 0x1;
      } else {
         rx_fifo.write(data_buffer & mask);
      }

      wait(sc_core::SC_ZERO_TIME);
      status_event.notify(sc_core::SC_ZERO_TIME);
      possible_intr_event.notify(sc_core::SC_ZERO_TIME);
   }
}

bool spi_tlm::write_reg(std::uint8_t offset, std::uint16_t value) {
   switch (offset) {
   case 0x00:
      reg_cr0 = value;
      return true;

   case 0x04:
      reg_cr1 = value & 0xf;
      if (reg_cr1 & 0x2)
         transmission_event.notify(sc_core::SC_ZERO_TIME);
      return true;

   case 0x08: {
      reg_dr = value;
      if (tx_fifo.num_free()) {
         tx_fifo.write(value);
         possible_intr_event.notify(sc_core::SC_ZERO_TIME);
         transmission_event.notify(sc_core::SC_ZERO_TIME); // wake up the transmit thread
         update_status_reg();
         return true;
      } else {
         return false;
      }
   }

   case 0x0c:
      SC_REPORT_WARNING("TLM-2", "Write attempt to Read-Only reg_sr at offset 0x0C.");
      return false;

   case 0x10:
      reg_cpsr = value & 0xff & 0xfe; // ensure the last bit is 0
      return true;

   case 0x14:
      reg_imsc = value & 0xf;
      possible_intr_event.notify(sc_core::SC_ZERO_TIME);
      return true;

   case 0x18:
      SC_REPORT_WARNING("TLM-2", "Write attempt to Read-Only reg_ris at offset 0x18.");
      return false;

   case 0x1c:
      SC_REPORT_WARNING("TLM-2", "Write attempt to Read-Only reg_mis at offset 0x18.");
      return false;

   case 0x20:
      reg_icr = ~(value & 0x3) & 0x3; // the second & 0x3 is to keep the other bits zero
      reg_ris &= ~(value & 0x03);
      possible_intr_event.notify(sc_core::SC_ZERO_TIME);
      return true;

   case 0x24:
      reg_dmacr = value & 0x3;
      return true;
   }

   return false;
}

std::uint16_t spi_tlm::read_reg(std::uint16_t offset) {
   switch (offset) {
   case 0x00:
      return reg_cr0;

   case 0x04:
      return reg_cr1;

   case 0x08:
      if (rx_fifo.num_available() == 0) {
         SC_REPORT_WARNING("TLM-2", "Reading from empty Receive FIFO");
         reg_ris |= 0x1;
         possible_intr_event.notify(sc_core::SC_ZERO_TIME);
         return 0; // return stale data
      } else {
         std::uint16_t data = rx_fifo.read();
         std::uint16_t word_size = (reg_cr0 & 0xf) + 1;
         std::uint16_t mask = (word_size == 16) ? 0xFFFF : (1 << word_size) - 1;
         possible_intr_event.notify(sc_core::SC_ZERO_TIME);
         status_event.notify(sc_core::SC_ZERO_TIME);
         return data & mask;
      }

   case 0x0c:
      return reg_sr & 0x1f;

   case 0x10:
      return reg_cpsr & 0xff;

   case 0x14:
      return reg_imsc & 0xf;

   case 0x18:
      return reg_ris & 0xf;

   case 0x1c:
      return reg_mis & 0xf;

   case 0x20:
      return 0; // return stale since reg_icr is Write-only

   case 0x24:
      return reg_dmacr & 0x3;

   case 0xFE0:
      return reg_periph_id0;
   case 0xFE4:
      return reg_periph_id1;
   case 0xFE8:
      return reg_periph_id2;
   case 0xFEC:
      return reg_periph_id3;
   case 0xFF0:
      return reg_cell_id0;
   case 0xFF4:
      return reg_cell_id1;
   case 0xFF8:
      return reg_cell_id2;
   case 0xFFC:
      return reg_cell_id3;
   }

   return 0;
}

void spi_tlm::update_intr() {
   std::uint16_t mask = reg_imsc & 0x0F;

   if (tx_fifo.num_available() <= (FIFO_SIZE / 2)) {
      reg_ris |= 0x08;
   } else {
      reg_ris &= ~0x08;
   }

   if (rx_fifo.num_available() >= (FIFO_SIZE / 2)) {
      reg_ris |= 0x04;
   } else {
      reg_ris &= ~0x04;
   }

   reg_mis = reg_ris & mask;

   irq.write((reg_mis & 0xf) != 0);
}

void spi_tlm::update_status_reg() {
   if (rx_fifo.num_available() == FIFO_SIZE)
      reg_sr |= 0x8;
   else
      reg_sr &= ~(0x8);

   if (rx_fifo.num_available() == 0)
      reg_sr &= ~(0x4);
   else
      reg_sr |= 0x4;

   if (tx_fifo.num_free() == 0) // = if there is no more slot to write
      reg_sr &= ~(0x2);
   else
      reg_sr |= 0x2;

   if (tx_fifo.num_free() == FIFO_SIZE)
      reg_sr |= 0x1;
   else
      reg_sr &= ~(0x1);

   if (is_transmitting || tx_fifo.num_free() != 8)
      reg_sr |= 0x10;
   else
      reg_sr &= ~(0x10);
}

void spi_tlm::handle_reset() {
   reg_cr0 = 0x0;
   reg_cr1 = 0x0;
   reg_sr = 0x03;
   reg_cpsr = 0x00;
   reg_imsc = 0x0;
   reg_ris = 0x8;
   reg_mis = 0x0;
   reg_icr = 0x0;
   reg_dmacr = 0x0;

   std::uint16_t dump_buffer;
   while (rx_fifo.nb_read(dump_buffer))
      ;

   while (tx_fifo.nb_read(dump_buffer))
      ;

   is_transmitting = false;

   possible_intr_event.notify(sc_core::SC_ZERO_TIME);
   update_status_reg();
}

} // namespace cdc::components
