#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "frame_memory.h"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    FrameMemory frame_memory;
    tlm_utils::simple_target_socket<Top> db_socket;

    Top(sc_module_name name);
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
};

#endif