// Author: hoangv11
//verified: linhtk55-fpt

#include "trng_tlm.h"

#include <cstdint>
#include <cstring>

namespace cdc::components {

trng_tlm::trng_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , clk("clk")
    , irq_out("irq_out")
    , access_latency_(access_latency) {
   socket.register_b_transport(this, &trng_tlm::b_transport);
   socket.register_transport_dbg(this, &trng_tlm::transport_dbg);

   SC_METHOD(drive_outputs);
   sensitive << reset_n << irq_update_event_;
   dont_initialize();
}

void trng_tlm::start_of_simulation() {
   if (!reset_n.read()) {
      core_.reset();
   }
   irq_level_ = false;
   irq_out.write(irq_level_);
}

void trng_tlm::drive_outputs() {
   if (!reset_n.read()) {
      core_.reset();
      irq_level_ = false;
   }
   irq_out.write(irq_level_);
}

void trng_tlm::update_irq() {
   irq_level_ = core_.hasInterrupt();
   irq_update_event_.notify(sc_core::SC_ZERO_TIME);
}

void trng_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   delay += access_latency_;

   const std::uint64_t offset = trans.get_address();
   const unsigned len = trans.get_data_length();
   unsigned char *ptr = trans.get_data_ptr();

   if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || !check_valid_offset(offset) ||
       ptr == nullptr) {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return;
   }

   if (trans.get_command() == tlm::TLM_READ_COMMAND) {
      const std::uint32_t value = core_.readReg(static_cast<std::uint32_t>(offset));
      std::memcpy(ptr, &value, sizeof(value));
   } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::uint32_t value = 0;
      std::memcpy(&value, ptr, sizeof(value));
      core_.writeReg(static_cast<std::uint32_t>(offset), value);
   } else {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
   }

   update_irq();
   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int trng_tlm::transport_dbg(tlm::tlm_generic_payload &trans) {
   const std::uint64_t offset = trans.get_address();
   const unsigned len = trans.get_data_length();
   unsigned char *ptr = trans.get_data_ptr();

   if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || !check_valid_offset(offset) ||
       ptr == nullptr) {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return 0;
   }

   if (trans.get_command() == tlm::TLM_READ_COMMAND) {
      const std::uint32_t value = core_.debugReadReg(static_cast<std::uint32_t>(offset));
      std::memcpy(ptr, &value, sizeof(value));
   } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::uint32_t value = 0;
      std::memcpy(&value, ptr, sizeof(value));
      core_.debugWriteReg(static_cast<std::uint32_t>(offset), value);
   } else {
      return 0;
   }

   trans.set_response_status(tlm::TLM_OK_RESPONSE);
   return sizeof(std::uint32_t);
}

bool trng_tlm::check_valid_offset(std::uint32_t offset) {
   if (offset >= 0x100 && offset <= 0x128)
      return true;
   if (offset >= 0x12C && offset <= 0x138)
      return true;
   if (offset == 0x140)
      return true;
   if (offset == 0x1B8)
      return true;
   if (offset == 0x1BC)
      return true;
   if (offset >= 0x1E0 && offset <= 0x1E8)
      return true;
   return false;
}

} // namespace cdc::components
