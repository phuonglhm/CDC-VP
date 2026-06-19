//author: linhtk55-fpt

#include "dmic.h"
using namespace sc_core;

SC_HAS_PROCESS(DmicTLM);
DmicTLM::DmicTLM(sc_module_name name, int decimation) : sc_module(name), decimation_factor(decimation),
                                                        integrator(0), prev_integrator(0), counter(0)
{
    target_socket.register_b_transport(this, &DmicTLM::b_transport);
}

void DmicTLM::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    auto *data = reinterpret_cast<pdm_payload *>(trans.get_data_ptr());
    integrator += data->density;
    counter++;
    if (counter >= decimation_factor)
    {
        counter = 0;

        int pcm_sample = (int)(integrator - prev_integrator);
        prev_integrator = integrator;

        //Push to output
        pcm_out_port->write(pcm_sample);
    }

    //update time
    delay += sc_time(1.0 / 3000000.0, SC_SEC);
}