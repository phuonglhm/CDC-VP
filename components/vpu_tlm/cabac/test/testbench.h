#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "top.h"
#include "out_monitor.h"
#include "custom_packet.h"
#include "../include/cabac_tables.h"
#include <vector>
#include <cstring>

class TestBench : public sc_core::sc_module {
public:
    explicit TestBench(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          start_socket("start_socket"),
          top("top_test"),
          out_monitor("out_monitor")
    {
    }
    SC_HAS_PROCESS(TestBench);
    tlm_utils::simple_initiator_socket<TestBench> start_socket;
    CabacTop top;
    OutMonitor out_monitor;
    bool dataflow_test(const CustomPacket &pkt = CustomPacket());
    bool dataflow_stream_test(const CustomPacket &pkt = CustomPacket());
};

#endif
