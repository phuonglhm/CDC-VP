#include "rec_tq.h"

RecTQ::RecTQ(sc_core::sc_module_name name) : 
    sc_module(name), buffer_socket("buffer_socket"), cabac_socket("cabac_socket"), inv_tq_socket("inv_tq_socket") {
    buffer_socket.register_b_transport(this, &RecTQ::b_transport);  
}

void RecTQ::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // Forward the received transaction to InvTQ
    inv_tq_socket->b_transport(trans, delay);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}