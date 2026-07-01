#include "testbench.h"
#include "../include/rec_packet.h"

// Implement the dataflow_test to send a RecPacket into Cabac and verify output
bool TestBench::dataflow_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 0;
        pkt.x = 0; pkt.y = 0;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // Send via the testbench initiator socket to Cabac
    start_socket->b_transport(trans, delay);

    // advance simulation briefly
    sc_core::sc_start(1, sc_core::SC_MS);

    if (out_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(out_monitor.last_data.data(), out_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::COEFF) return false;
    if (out_pkt.block_idx != pkt.block_idx) return false;
    if (out_pkt.data.size() != 16) return false; // Cabac currently reads 16 bytes
    return true;
}

// bool TestBench::dataflow_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::RESIDUAL;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//         // sample payload bytes (use 16 bytes for 4x4 pixels example)
//         pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
//                     0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
//     }
//     std::cout << pkt << std::endl;

//     // Serialize RecPacket into a contiguous buffer: header then payload (with extended header)
//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     // create TLM transaction and send via the TestBench initiator
//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // send the transaction into `RecIntra` via the testbench initiator socket so
//     // it traverses: top -> rec_intra -> res_buffer -> rec_tq -> inv_tq -> db_monitor
//     intra_socket->b_transport(trans, delay);

//     // run a small time for propagation
//     sc_start(1, SC_MS);

//     // Verify DBMonitor got the same bytes
//     return (db_monitor.last_data == buf);
// }

// bool TestBench::recIntra_DCMode_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//         // sample payload bytes (use 16 bytes for 4x4 pixels example)
//         pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
//                     0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
//     }
//     std::cout << pkt << std::endl;

//     // Ensure prediction header fields are explicit for this test and use packRecPacket
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 1; // DC
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // send the transaction into `RecIntra` via the testbench initiator socket so
//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     // Validate that DBMonitor got a PRE packet with a 4x4 prediction of constant 128
//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::PRE) return false;
//     if (out_pkt.data.size() < 16) return false;
//     // expected all-128 4x4
//     for (size_t i = 0; i < 16; ++i) {
//         if (out_pkt.data[i] != 128) return false;
//     }
//     return true;
// }

// bool TestBench::recIntra_PlanarMode_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//     }
//     std::cout << pkt << std::endl;

//     // Planar prediction
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 0; // Planar
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::PRE) return false;
//     if (out_pkt.data.size() < 16) return false;
//     for (size_t i = 0; i < 16; ++i) {
//         if (out_pkt.data[i] != 128) return false;
//     }
//     return true;
// }

// bool TestBench::recIntra_AngularMode_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//     }
//     std::cout << pkt << std::endl;

//     // Angular prediction (choose a representative angular mode)
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 10; // an angular mode (maps to pred_angle 0 in implementation)
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::PRE) return false;
//     if (out_pkt.data.size() < 16) return false;
//     for (size_t i = 0; i < 16; ++i) {
//         if (out_pkt.data[i] != 128) return false;
//     }
//     return true;
// }


// bool TestBench::recMc_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::MC);
//         pkt.cbf_mask.set(3);
//     }
//     std::cout << pkt << std::endl;

//     // Ensure MC request fields
//     pkt.pred_type = static_cast<uint8_t>(PredType::MC);
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     // Install a simple MV (zero offset) for this block index so RecMc
//     // will use the MV path instead of the fallback.
//     top.rec_mv.writeMV(static_cast<uint32_t>(pkt.block_idx), MotionVector(0, 0));

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // Send to RecMc
//     mc_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     // Validate DBMonitor got a RESIDUAL packet (res_buffer now computes residuals)
//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
//     if (out_pkt.data.size() < 16) return false;
//     for (size_t i = 0; i < 16; ++i) {
//         if (out_pkt.data[i] != 128) return false;
//     }
//     return true;
// }


// bool TestBench::residual_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//         // sample payload bytes (use 16 bytes for 4x4 pixels example)
//         pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
//                     0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
//     }
//     std::cout << pkt << std::endl;

//     // Ensure prediction header fields are explicit for this test and use packRecPacket
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 1; // DC
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // send the transaction into `RecIntra` via the testbench initiator socket
//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     // Validate that DBMonitor got a RESIDUAL packet with 4x4 residuals
//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
//     if (out_pkt.data.size() < 16) return false;
//     for (size_t i = 0; i < 16; ++i) {
//         if (out_pkt.data[i] != 128) return false;
//     }
//     return true;
// }


// bool TestBench::tq_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//     }
//     std::cout << pkt << std::endl;

//     // Ensure prediction header fields are explicit for this test
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 1; // DC
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // send to RecIntra to drive the pipeline -> ResBuffer -> RecTQ -> CoeffMonitor
//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     if (coeff_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(coeff_monitor.last_data.data(), coeff_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::COEFF) return false;
//     // Expect at least 16 int16 coefficients (32 bytes)
//     if (out_pkt.data.size() < 32) return false;
//     return true;
// }


// bool TestBench::inv_tq_test(const RecPacket &pkt_in) {
//     RecPacket pkt = pkt_in;
//     if (pkt.data.empty()) {
//         pkt.cmd = RecCmd::READ_REQ;
//         pkt.block_idx = 5;
//         pkt.x = 1; pkt.y = 2;
//         pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//         pkt.cbf_mask.set(3);
//     }
//     std::cout << pkt << std::endl;

//     // Ensure prediction header fields are explicit for this test
//     pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
//     pkt.mode = 1; // DC
//     pkt.pre_sel = 0;
//     pkt.i4x4_x = 0; pkt.i4x4_y = 0;

//     std::vector<uint8_t> buf = packRecPacket(pkt);

//     tlm::tlm_generic_payload trans;
//     trans.set_command(tlm::TLM_WRITE_COMMAND);
//     trans.set_address(0);
//     trans.set_data_ptr(buf.data());
//     trans.set_data_length(buf.size());
//     sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

//     // send to RecIntra to drive the pipeline -> ResBuffer -> RecTQ -> InvTQ -> DB
//     intra_socket->b_transport(trans, delay);
//     sc_start(1, SC_MS);

//     if (db_monitor.last_data.empty()) return false;
//     RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
//     if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
//     int side = 4 << pkt.size;
//     if ((int)out_pkt.data.size() < side * side) return false;
//     return true;
// }


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    test_bench.start_socket.bind(test_bench.top.cabac.start_socket);
    test_bench.top.cabac.out_socket.bind(test_bench.out_monitor.cabac_socket);

    bool ok = test_bench.dataflow_test();
    std::cout << "Dataflow Test: " << (ok ? "PASS" : "FAIL") << std::endl;
    return ok ? 0 : 1;
}