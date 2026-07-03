#include "db_mv.h"

MotionVector::MotionVector(sc_core::sc_module_name name) : sc_module(name), filter_socket("filter_socket"), start_socket("start_socket") {
    start_socket.register_b_transport(this, &MotionVector::b_transport);
}
void MotionVector::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    filter_socket->b_transport(trans, delay);
}