#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

class CoeffMonitor : sc_core::sc_module {
    public:
    CoeffMonitor(sc_core::sc_module_name name);
    SC_HAS_PROCESS(CoeffMonitor);
    tlm_utils::simple_target_socket<CoeffMonitor> tq_socket;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

// Implementation
CoeffMonitor::CoeffMonitor(sc_core::sc_module_name name)
  : sc_module(name), tq_socket("tq_socket") {
    tq_socket.register_b_transport(this, &CoeffMonitor::b_transport);
}

void CoeffMonitor::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // simple sink: discard payload and acknowledge
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}