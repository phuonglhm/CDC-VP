//author: linhtk55-fpt

#ifndef DMIC_H
#define DMIC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include "pdm_payload.h"
using namespace sc_core;

class DmicTLM : public sc_core::sc_module {
    public:
    DmicTLM (sc_core::sc_module_name name, int decimation = 64);
    tlm_utils::simple_target_socket<DmicTLM> target_socket;
    sc_port<sc_fifo_out_if<int>> pcm_out_port;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
    int decimation_factor;
    long integrator;
    long prev_integrator;
    int counter;
};

#endif DMIC_H