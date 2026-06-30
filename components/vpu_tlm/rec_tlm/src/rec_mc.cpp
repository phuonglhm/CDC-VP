#include "rec_mc.h"

RecMc::RecMc(sc_core::sc_module_name name) : 
    sc_module(name), start_socket("start_socket"), buffer_socket("buffer_socket") {
    start_socket.register_b_transport(this, &RecMc::b_transport);  
}

void RecMc::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}