#ifndef REC_INV_TQ_H
#define REC_INV_TQ_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_packet.h"

class InvTQ : sc_core::sc_module {
    public:
    InvTQ (sc_core::sc_module_name name);
    SC_HAS_PROCESS(InvTQ);
    tlm_utils::simple_initiator_socket<InvTQ> db_socket;
    tlm_utils::simple_target_socket<InvTQ> tq_socket;

    // Test helper: allow direct invocation of the transport handler
    void invoke_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        b_transport(trans, delay);
    }

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};
#endif