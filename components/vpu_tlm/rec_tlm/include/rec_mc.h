#ifndef REC_MC_H
#define REC_MC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_packet.h"

class RecMc : sc_core::sc_module {
    public:
    RecMc (sc_core::sc_module_name name);
    SC_HAS_PROCESS(RecMc);
    tlm_utils::simple_initiator_socket<RecMc> buffer_socket;
    tlm_utils::simple_target_socket<RecMc> start_socket;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};
#endif