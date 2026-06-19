//author: linhtk55-fpt

#ifndef DMIC_TESTBENCH_H
#define DMIC_TESTBENCH_H

#include <systemc.h>
#include <tlm.h>
#include "tlm_utils/simple_initiator_socket.h"
#include <cmath>
#include <iostream>
#include "pdm_payload.h"
using namespace sc_core;

class TestBench : public sc_module
{
public:
    tlm_utils::simple_initiator_socket<TestBench> initiator_socket;

    SC_HAS_PROCESS(TestBench);
    TestBench(sc_module_name name) : sc_module(name)
    {
        SC_THREAD(stimulus_process);
    }

private:
    void stimulus_process();
};

class PCM_Monitor : public sc_module
{
public:
    sc_port<sc_fifo_in_if<int>> pcm_in_port;

    SC_HAS_PROCESS(PCM_Monitor);
    PCM_Monitor(sc_module_name name) : sc_module(name)
    {
        SC_THREAD(monitor_process);
    }

private:
    void monitor_process();
};

#endif