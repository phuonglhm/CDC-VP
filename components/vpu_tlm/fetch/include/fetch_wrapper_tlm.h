#ifndef FETCH_WRAPPER_TLM_H
#define FETCH_WRAPPER_TLM_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "simple_memory.h"

class FetchWrapper : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<FetchWrapper> start_socket;
    tlm_utils::simple_initiator_socket<FetchWrapper> out_socket;
    // Local memory used by the wrapper for simplified tests
    SimpleMemory simple_mem;

    FetchWrapper(sc_core::sc_module_name name);
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
};

#endif
