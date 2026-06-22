// Author: hoangv11

#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>

#include "trng_model.h"

namespace cdc::components {

// SystemC/TLM wrapper around the pure-C++ TRNG_Model register logic.
//
// Register window: 32-bit aligned accesses, offsets 0x100..0x1E8. Addresses are
// region-local because bus_router subtracts the peripheral base before forwarding.
class trng_tlm : public sc_core::sc_module {
public:
   SC_HAS_PROCESS(trng_tlm);

   tlm_utils::simple_target_socket<trng_tlm> socket;
   sc_core::sc_in<bool> reset_n;
   sc_core::sc_in<bool> clk;
   sc_core::sc_out<bool> irq_out;

   explicit trng_tlm(sc_core::sc_module_name name,
                     sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

private:
   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
   unsigned int transport_dbg(tlm::tlm_generic_payload &trans);

   void start_of_simulation() override;
   void drive_outputs();
   void update_irq();

   TRNG_Model core_;
   sc_core::sc_time access_latency_;
   sc_core::sc_event irq_update_event_;
   bool irq_level_ = false;

   bool check_valid_offset(std::uint32_t offset);
};

} // namespace cdc::components
