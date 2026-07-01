#include "out_monitor.h"
#include <cstring>
#include <iostream>

OutMonitor::OutMonitor(sc_core::sc_module_name name)
    : sc_module(name), cabac_socket("cabac_socket") {
    cabac_socket.register_b_transport(this, &OutMonitor::b_transport);
}

void OutMonitor::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    last_data.clear();
    if (trans.get_data_ptr() && trans.get_data_length() > 0) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        last_data.assign(p, p + trans.get_data_length());
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}