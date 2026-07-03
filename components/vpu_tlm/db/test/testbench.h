#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "top.h"
#include "out_monitor.h"

using namespace sc_core;

class TestBench : public sc_module {
    public:
        TestBench(sc_module_name name) : sc_module(name), out_monitor("out_monitor"), top("top_test") {};
        SC_HAS_PROCESS(TestBench);
        tlm_utils::simple_initiator_socket<TestBench> bs_socket;
        tlm_utils::simple_initiator_socket<TestBench> mv_socket;
        tlm_utils::simple_target_socket<TestBench> out_socket;
        OutMonitor out_monitor;
        Top top;
};

#endif