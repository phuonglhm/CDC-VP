#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "top.h"
#include "out_monitor.h"
// Explicitly include RecPacket so this header doesn't rely on precompiled headers
#include "rec_packet.h"
using namespace sc_core;

class TestBench : sc_module {
    public:
    TestBench (sc_module_name name) : sc_module(name), start_socket("start_socket"), top("top_test"), out_monitor("out_monitor") {}
    SC_HAS_PROCESS(TestBench);
    tlm_utils::simple_initiator_socket<TestBench> start_socket;
    Top top;
    OutMonitor out_monitor;
    bool dataflow_test(const RecPacket &pkt = RecPacket());
    bool recIntra_DCMode_test(const RecPacket &pkt = RecPacket());
    bool recIntra_PlanarMode_test(const RecPacket &pkt = RecPacket());
    bool recIntra_AngularMode_test(const RecPacket &pkt = RecPacket());
    bool recMc_test(const RecPacket &pkt = RecPacket());
    bool residual_test(const RecPacket &pkt = RecPacket());
    bool tq_test(const RecPacket &pkt = RecPacket());
    bool inv_tq_test(const RecPacket &pkt = RecPacket());
};

#endif