#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "adc_model.h"

namespace cdc::components {

// SystemC/TLM wrapper around the pure-C++ ADC_Model register logic.
//
// Register window: 32-bit aligned accesses, offsets 0x00..0x0C. Addresses are
// region-local because bus_router subtracts the peripheral base before forwarding.
class adc_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<adc_tlm> socket;
    sc_core::sc_out<bool> irq_out;

    explicit adc_tlm(sc_core::sc_module_name name,
                     sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    void start_of_simulation() override;
    void update_irq();

    ADC_Model core_;
    sc_core::sc_time access_latency_;
};

} // namespace cdc::components
