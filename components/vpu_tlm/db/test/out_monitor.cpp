#include "out_monitor.h"
#include <systemc>

using namespace sc_core;

// Implementation
OutMonitor::OutMonitor(sc_core::sc_module_name name)
    : sc_module(name), filter_socket("tq_socket") {
    filter_socket.register_b_transport(this, &OutMonitor::b_transport);
}

void OutMonitor::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}