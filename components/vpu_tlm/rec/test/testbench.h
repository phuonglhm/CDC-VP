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
    bool dataflow_test(const CustomPacket &pkt = CustomPacket());
    bool recIntra_DCMode_test(const CustomPacket &pkt = CustomPacket());
    bool recIntra_PlanarMode_test(const CustomPacket &pkt = CustomPacket());
    bool recIntra_AngularMode_test(const CustomPacket &pkt = CustomPacket());
    bool recMc_test(const CustomPacket &pkt = CustomPacket());
    bool mcPre_to_db_test(const CustomPacket &pkt = CustomPacket());
    bool mcReadReq_no_mv_fallback_test(const CustomPacket &pkt = CustomPacket());
    bool coeffThroughInvTq_test(const CustomPacket &pkt = CustomPacket());
    bool large_block_tq_test(const CustomPacket &pkt = CustomPacket());
    bool pack_unpack_roundtrip_test(const CustomPacket &pkt = CustomPacket());
    bool subblock_offset_test(const CustomPacket &pkt = CustomPacket());
    bool qp_extremes_test(const CustomPacket &pkt = CustomPacket());
    bool short_payload_handling_test(const CustomPacket &pkt = CustomPacket());
    bool residual_test(const CustomPacket &pkt = CustomPacket());
    bool tq_test(const CustomPacket &pkt = CustomPacket());
    bool inv_tq_test(const CustomPacket &pkt = CustomPacket());
};

#endif