#include "rec_inv_tq.h"

InvTQ::InvTQ(sc_core::sc_module_name name) : 
    sc_module(name), tq_socket("tq_socket"), db_socket("db_socket") {
    tq_socket.register_b_transport(this, &InvTQ::b_transport);  
}

void InvTQ::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // Forward the transaction unchanged to the DB path
    db_socket->b_transport(trans, delay);
}