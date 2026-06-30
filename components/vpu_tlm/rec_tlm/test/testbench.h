#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "top.h"
#include "coeff_monitor.cpp"
#include "db_monitor.cpp"
#include "frame_buffer.cpp"
using namespace sc_core;

class TestBench : sc_module {
    public:
    TestBench (sc_module_name name) : sc_module(name), top("top_test"), coeff_monitor("coeff_monitor"), db_monitor("db_monitor"), frame_buffer("frame_buffer") {};
    SC_HAS_PROCESS(TestBench);
    tlm_utils::simple_initiator_socket<TestBench> intra_socket;
    tlm_utils::simple_initiator_socket<TestBench> mc_socket;
    Top top;
    CoeffMonitor coeff_monitor;
    DBMonitor db_monitor;
    FrameBuffer frame_buffer;
    bool dataflow_test(const RecPacket &pkt = RecPacket());
    bool recIntra_DCMode_test(const RecPacket &pkt = RecPacket());
};

#endif