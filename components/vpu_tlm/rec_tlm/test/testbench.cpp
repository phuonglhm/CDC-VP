#include "testbench.h"

bool TestBench::dataflow_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::RESIDUAL;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.type = false;
        pkt.cbf_mask.set(3);
        // sample payload bytes (use 16 bytes for 4x4 pixels example)
        pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
                    0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
    }
    std::cout << pkt << std::endl;

    // Serialize RecPacket into a contiguous buffer: header then payload (with extended header)
    std::vector<uint8_t> buf = packRecPacket(pkt);

    // create TLM transaction and send via the TestBench initiator
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // send the transaction into `RecIntra` via the testbench initiator socket so
    // it traverses: top -> rec_intra -> res_buffer -> rec_tq -> inv_tq -> db_monitor
    intra_socket->b_transport(trans, delay);

    // run a small time for propagation
    sc_start(1, SC_MS);

    // Verify DBMonitor got the same bytes
    return (db_monitor.last_data == buf);
}

bool TestBench::recIntra_DCMode_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.type = false;
        pkt.cbf_mask.set(3);
        // sample payload bytes (use 16 bytes for 4x4 pixels example)
        pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
                    0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
    }
    std::cout << pkt << std::endl;

    // Ensure prediction header fields are explicit for this test and use packRecPacket
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    pkt.mode = 1; // DC
    pkt.pre_sel = 0;
    pkt.i4x4_x = 0; pkt.i4x4_y = 0;

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // send the transaction into `RecIntra` via the testbench initiator socket so
    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    // Validate that DBMonitor got a PRE packet with a 4x4 prediction of constant 128
    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::PRE) return false;
    if (out_pkt.data.size() < 16) return false;
    // expected all-128 4x4
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    test_bench.mc_socket.bind(test_bench.top.rec_mc.start_socket);
    test_bench.intra_socket.bind(test_bench.top.rec_intra.start_socket);
    test_bench.top.res_buffer.frame_socket.bind(test_bench.frame_buffer.mc_socket);
    test_bench.top.rec_tq.cabac_socket.bind(test_bench.coeff_monitor.tq_socket);
    test_bench.top.inv_tq.db_socket.bind(test_bench.db_monitor.inv_tq_socket);

    bool ok = test_bench.recIntra_DCMode_test();

    // Print the final packet received by DBMonitor (test harness level)
    if (!test_bench.db_monitor.last_data.empty()) {
        RecPacket out_pkt = unpackRecPacket(test_bench.db_monitor.last_data.data(), test_bench.db_monitor.last_data.size());
        std::cout << "-------------RecIntra Packet--------------" << std::endl;
        std::cout << out_pkt << std::endl;
    }

    if (ok) std::cout << "Test PASS: DBMonitor received identical bytes\n";
    else std::cout << "Test FAIL\n";
    return ok ? 0 : 1;
}