#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "rtc_model.h"

namespace cdc::components {

// SystemC/TLM-2.0 wrapper around the pure-C++ RTC_Model.
//
// Register window: 32-bit aligned accesses, offsets 0x00..0x1C. Addresses are
// region-local because bus_router subtracts the peripheral base before
// forwarding. A free-running counter advances one LSB every `tick_period`
// while CR.EN is set; when DR reaches MR the alarm latches in RIS and, if
// unmasked via IMSC, drives `irq_out`.
class rtc_tlm : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(rtc_tlm);

    tlm_utils::simple_target_socket<rtc_tlm> socket;
    sc_core::sc_in<bool> reset_n;   // active-low reset
    sc_core::sc_out<bool> irq_out;  // alarm interrupt to PLIC

    // tick_period is the simulated-time meaning of one RTC count. The default
    // models a real 1 Hz time-of-day clock; tests typically pass a small value
    // so the counter advances quickly.
    explicit rtc_tlm(sc_core::sc_module_name name,
                     sc_core::sc_time tick_period = sc_core::sc_time(1, sc_core::SC_SEC),
                     sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    void start_of_simulation() override;
    void tick_thread();
    void drive_outputs();
    void update_irq();

    RTC_Model core_;
    sc_core::sc_time tick_period_;
    sc_core::sc_time access_latency_;
    sc_core::sc_event irq_update_event_;
    bool irq_level_ = false;
};

} // namespace cdc::components
