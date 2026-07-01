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
    bool recIntra_PlanarMode_test(const RecPacket &pkt = RecPacket());
    bool recIntra_AngularMode_test(const RecPacket &pkt = RecPacket());
    bool recMc_test(const RecPacket &pkt = RecPacket());
    bool mcPre_to_db_test(const RecPacket &pkt = RecPacket());
    bool mcReadReq_no_mv_fallback_test(const RecPacket &pkt = RecPacket());
    bool coeffThroughInvTq_test(const RecPacket &pkt = RecPacket());
    bool large_block_tq_test(const RecPacket &pkt = RecPacket());
    bool pack_unpack_roundtrip_test(const RecPacket &pkt = RecPacket());
    bool subblock_offset_test(const RecPacket &pkt = RecPacket());
    bool qp_extremes_test(const RecPacket &pkt = RecPacket());
    bool short_payload_handling_test(const RecPacket &pkt = RecPacket());
    bool residual_test(const RecPacket &pkt = RecPacket());
    bool tq_test(const RecPacket &pkt = RecPacket());
    bool inv_tq_test(const RecPacket &pkt = RecPacket());
};

#endif