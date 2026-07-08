#include "testbench.h"
#include "../include/rec_fetch_gateway.h"
#include "../../include/block_coord_codec.h"
#if defined(USE_FETCH)
#include "../../fetch/include/fetch_wrapper_tlm.h"
#endif

bool TestBench::dataflow_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::RESIDUAL;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
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
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
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
    if (out_pkt.data.size() < 16) return false;
    // DBMonitor may see a PRE (before ResBuffer) or a RESIDUAL (after ResBuffer).
    // In this test environment RecMemory uses dummy=128 so predictions/reconstructions are 128.
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}

bool TestBench::recIntra_PlanarMode_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    // Planar prediction
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    pkt.mode = 0; // Planar
    pkt.pre_sel = 0;
    pkt.i4x4_x = 0; pkt.i4x4_y = 0;

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}

bool TestBench::recIntra_AngularMode_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    // Angular prediction (choose a representative angular mode)
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    pkt.mode = 10; // an angular mode (maps to pred_angle 0 in implementation)
    pkt.pre_sel = 0;
    pkt.i4x4_x = 0; pkt.i4x4_y = 0;

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}


bool TestBench::recMc_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::MC);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    // Ensure MC request fields
    pkt.pred_type = static_cast<uint8_t>(PredType::MC);
    pkt.pre_sel = 0;
    pkt.i4x4_x = 0; pkt.i4x4_y = 0;

    // Install a simple MV (zero offset) for this block index so RecMc
    // will use the MV path instead of the fallback.
    top.rec_mv.writeMV(
        cdc::components::make_extended_block_address(pkt.block_idx, pkt.x, pkt.y),
        MotionVector(0, 0));

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // Send to RecMc
    mc_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    // Validate DBMonitor got a RESIDUAL packet (res_buffer now computes residuals)
    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
    if (out_pkt.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}


bool TestBench::residual_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
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

    // send the transaction into `RecIntra` via the testbench initiator socket
    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    // Validate that DBMonitor got a RESIDUAL packet with 4x4 residuals
    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
    if (out_pkt.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) {
        if (out_pkt.data[i] != 128) return false;
    }
    return true;
}


bool TestBench::tq_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    // Ensure prediction header fields are explicit for this test
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

    // send to RecIntra to drive the pipeline -> ResBuffer -> RecTQ -> CoeffMonitor
    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (coeff_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(coeff_monitor.last_data.data(), coeff_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::COEFF) return false;
    // Expect at least 16 int16 coefficients (32 bytes)
    if (out_pkt.data.size() < 32) return false;
    return true;
}


bool TestBench::inv_tq_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    // Ensure prediction header fields are explicit for this test
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

    // send to RecIntra to drive the pipeline -> ResBuffer -> RecTQ -> InvTQ -> DB
    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out_pkt = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out_pkt.cmd != RecCmd::RESIDUAL) return false;
    int side = 4 << pkt.size;
    if ((int)out_pkt.data.size() < side * side) return false;
    return true;
}

// New tests exercising alternate dataflow paths
bool TestBench::mcPre_to_db_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::PRE;
        pkt.block_idx = 6;
        pkt.x = 0; pkt.y = 0;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::MC);
        // prediction all zeros so residual = original(128) - 0 -> biased 128
        pkt.data = std::vector<uint8_t>(16, 0);
    }

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // send PRE into RecMc path
    mc_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out.cmd != RecCmd::RESIDUAL) return false;
    if (out.data.size() < 16) return false;
    // With dummy RecMemory=128 and prediction=0, residual = 128 - 0 + 128 = 256 -> clamped to 255
    for (size_t i = 0; i < 16; ++i) if (out.data[i] != 255) return false;
    return true;
}

bool TestBench::mcReadReq_no_mv_fallback_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = RecCmd::READ_REQ;
        pkt.block_idx = 7;
        pkt.x = 1; pkt.y = 1;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::MC);
    }

    std::vector<uint8_t> buf = packRecPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // Do NOT write an MV -> exercise fallback path in RecMc
    mc_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out.cmd != RecCmd::RESIDUAL) return false;
    if (out.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) if (out.data[i] != 128) return false;
    return true;
}

bool TestBench::coeffThroughInvTq_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    // Build a COEFF packet with zeroed qcoeffs (16 int16 -> 32 bytes)
    pkt.cmd = RecCmd::COEFF;
    pkt.block_idx = 1;
    pkt.x = 0; pkt.y = 0; pkt.size = 0; pkt.sel = 0; pkt.qp = 22;
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    pkt.data.assign(32, 0);

    std::vector<uint8_t> buf = packRecPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // Send through RecIntra -> ResBuffer -> RecTQ -> InvTQ -> DB
    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out.cmd != RecCmd::RESIDUAL) return false;
    if (out.data.size() < 16) return false;
    for (size_t i = 0; i < 16; ++i) if (out.data[i] != 128) return false;
    return true;
}

bool TestBench::large_block_tq_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    // Build a RESIDUAL packet for an 8x8 block (size=1) with biased 128 values
    pkt.cmd = RecCmd::RESIDUAL;
    pkt.block_idx = 2;
    pkt.x = 0; pkt.y = 0; pkt.size = 1; pkt.sel = 0; pkt.qp = 22;
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    // 8x8 = 64 values
    pkt.data.assign(64, 128);

    std::vector<uint8_t> buf = packRecPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    intra_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    // Expect RecTQ to emit COEFF of 64 int16 -> 128 bytes
    if (coeff_monitor.last_data.empty()) return false;
    if (coeff_monitor.last_data.size() < 128) return false;
    return true;
}

// New high-priority tests
bool TestBench::pack_unpack_roundtrip_test(const RecPacket &pkt_in) {
    RecPacket p;
    p.cmd = RecCmd::READ_REQ;
    p.block_idx = 42;
    p.x = 3; p.y = 4;
    p.size = 1; p.sel = 2; p.qp = 18;
    p.pred_type = static_cast<uint8_t>(PredType::MC);
    p.mode = 5; p.pre_sel = 1; p.i4x4_x = 2; p.i4x4_y = 1;
    p.cbf_mask.set(0); p.cbf_mask.set(7);
    p.data = {1,2,3,4,5,6,7};

    std::vector<uint8_t> buf = packRecPacket(p);
    RecPacket out = unpackRecPacket(buf.data(), buf.size());

    if (p.cmd != out.cmd) return false;
    if (p.block_idx != out.block_idx) return false;
    if (p.x != out.x || p.y != out.y) return false;
    if (p.size != out.size || p.sel != out.sel || p.qp != out.qp) return false;
    if (p.pred_type != out.pred_type) return false;
    if (p.mode != out.mode || p.pre_sel != out.pre_sel) return false;
    if (p.i4x4_x != out.i4x4_x || p.i4x4_y != out.i4x4_y) return false;
    if (p.cbf_mask != out.cbf_mask) return false;
    if (p.data != out.data) return false;
    return true;
}

bool TestBench::subblock_offset_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    pkt.cmd = RecCmd::READ_REQ;
    pkt.block_idx = 11;
    pkt.x = 4; pkt.y = 4; // pick an interior block so ref window varies
    pkt.size = 1; pkt.sel = 0; pkt.qp = 0; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    pkt.mode = 0; // planar
    // Use patterned RecMemory for this test so sub-block offsets produce different predictions (top constructs rec_mem with dummy=true by default).
    top.rec_mem.setUseDummy(false);

    pkt.i4x4_x = 0; pkt.i4x4_y = 0;
    std::vector<uint8_t> buf1 = packRecPacket(pkt);
    tlm::tlm_generic_payload t1; t1.set_command(tlm::TLM_WRITE_COMMAND); t1.set_address(0); t1.set_data_ptr(buf1.data()); t1.set_data_length(buf1.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    intra_socket->b_transport(t1, delay);
    sc_start(1, SC_MS);
    RecPacket out1;
    if (!coeff_monitor.last_data.empty()) {
        out1 = unpackRecPacket(coeff_monitor.last_data.data(), coeff_monitor.last_data.size());
    } else if (!db_monitor.last_data.empty()) {
        out1 = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    } else {
        return false;
    }

    pkt.i4x4_x = 1; pkt.i4x4_y = 1;
    std::vector<uint8_t> buf2 = packRecPacket(pkt);
    tlm::tlm_generic_payload t2; t2.set_command(tlm::TLM_WRITE_COMMAND); t2.set_address(0); t2.set_data_ptr(buf2.data()); t2.set_data_length(buf2.size());
    intra_socket->b_transport(t2, delay);
    sc_start(1, SC_MS);
    RecPacket out2;
    if (!coeff_monitor.last_data.empty()) {
        out2 = unpackRecPacket(coeff_monitor.last_data.data(), coeff_monitor.last_data.size());
    } else if (!db_monitor.last_data.empty()) {
        out2 = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    } else {
        // restore dummy before returning
        top.rec_mem.setUseDummy(true, 128);
        return false;
    }

    // Restore dummy behavior to avoid impacting other tests
    top.rec_mem.setUseDummy(true, 128);

    // Expect the 4x4 outputs to differ for different sub-block offsets
    if (out1.data.size() < 16 || out2.data.size() < 16) return false;
    if (out1.data == out2.data) {
        // Debug output to help diagnose why these match
        std::cerr << "[DBG] subblock out1 (first16): ";
        for (size_t i = 0; i < 16 && i < out1.data.size(); ++i) std::cerr << static_cast<int>(out1.data[i]) << " ";
        std::cerr << std::endl;
        std::cerr << "[DBG] subblock out2 (first16): ";
        for (size_t i = 0; i < 16 && i < out2.data.size(); ++i) std::cerr << static_cast<int>(out2.data[i]) << " ";
        std::cerr << std::endl;
        return false;
    }
    return true;
}

bool TestBench::qp_extremes_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    pkt.cmd = RecCmd::RESIDUAL;
    pkt.block_idx = 21;
    pkt.x = 0; pkt.y = 0; pkt.size = 0; pkt.sel = 0; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);

    // prepare a varying residual pattern (biased-by-128 values)
    pkt.data.resize(16);
    for (int i = 0; i < 16; ++i) pkt.data[i] = static_cast<uint8_t>(128 + (i - 8));

    // low qp
    pkt.qp = 0;
    std::vector<uint8_t> buf_low = packRecPacket(pkt);
    tlm::tlm_generic_payload tl; tl.set_command(tlm::TLM_WRITE_COMMAND); tl.set_address(0); tl.set_data_ptr(buf_low.data()); tl.set_data_length(buf_low.size());
    sc_core::sc_time delay2 = sc_core::SC_ZERO_TIME;
    intra_socket->b_transport(tl, delay2);
    sc_start(1, SC_MS);
    if (coeff_monitor.last_data.empty()) return false;
    std::vector<uint8_t> low = coeff_monitor.last_data;

    // high qp
    pkt.qp = 63;
    std::vector<uint8_t> buf_high = packRecPacket(pkt);
    tlm::tlm_generic_payload th; th.set_command(tlm::TLM_WRITE_COMMAND); th.set_address(0); th.set_data_ptr(buf_high.data()); th.set_data_length(buf_high.size());
    intra_socket->b_transport(th, delay2);
    sc_start(1, SC_MS);
    if (coeff_monitor.last_data.empty()) return false;
    std::vector<uint8_t> high = coeff_monitor.last_data;

    // Both should produce COEFF outputs and should differ under different QP
    if (low.size() < 32 || high.size() < 32) return false;
    if (low == high) return false;
    return true;
}

bool TestBench::short_payload_handling_test(const RecPacket &pkt_in) {
    RecPacket pkt = pkt_in;
    pkt.cmd = RecCmd::COEFF;
    pkt.block_idx = 99;
    pkt.x = 0; pkt.y = 0; pkt.size = 0; pkt.sel = 0; pkt.qp = 22;
    pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
    // truncated payload (only 8 bytes rather than 32)
    pkt.data.assign(8, 0xAA);

    std::vector<uint8_t> buf = packRecPacket(pkt);
    tlm::tlm_generic_payload tr; tr.set_command(tlm::TLM_WRITE_COMMAND); tr.set_address(0); tr.set_data_ptr(buf.data()); tr.set_data_length(buf.size());
    sc_core::sc_time delay3 = sc_core::SC_ZERO_TIME;
    intra_socket->b_transport(tr, delay3);
    sc_start(1, SC_MS);

    if (db_monitor.last_data.empty()) return false;
    RecPacket out = unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    if (out.cmd != RecCmd::RESIDUAL) return false;
    if (out.data.size() < 16) return false;
    return true;
}


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    test_bench.mc_socket.bind(test_bench.top.rec_mc.start_socket);
    test_bench.intra_socket.bind(test_bench.top.rec_intra.start_socket);
    test_bench.top.res_buffer.frame_socket.bind(test_bench.frame_buffer.mc_socket);
    test_bench.top.rec_tq.cabac_socket.bind(test_bench.coeff_monitor.tq_socket);
    test_bench.top.inv_tq.db_socket.bind(test_bench.db_monitor.inv_tq_socket);

    // Test harness: (opt-in) route Rec memory requests through a FetchWrapper
    // via the RecFetchGateway so we can validate fetch integration without
    // changing production `Top` wiring.
#if defined(USE_FETCH)
    RecFetchGateway gw("rec_fetch_gateway");
    FetchWrapper fetch("fetch_wrapper");
    gw.start_socket.bind(fetch.start_socket);
    fetch.out_socket.bind(gw.out_socket);

    // Preload the embedded SimpleMemory with dummy=128 to match RecMemory default.
    for (uint8_t plane = 0; plane < 3; ++plane) {
        for (uint32_t y = 0; y < 64; ++y) {
            for (uint32_t x = 0; x < 64; x += 16) {
                uint64_t addr = (static_cast<uint64_t>(plane) & 0xFFULL) << 56
                                | (static_cast<uint64_t>(y) << 16)
                                | (static_cast<uint64_t>(x) & 0xFFFFULL);
                std::vector<uint8_t> v(16, 128);
                fetch.simple_mem.load_data(addr, v);
            }
        }
    }

    // Override the bindMemory targets to use the gateway for this test run
    test_bench.top.rec_intra.bindMemory(gw);
    test_bench.top.rec_mc.bindMemory(gw);
    test_bench.top.res_buffer.bindMemory(gw);
#endif

    bool ok_dc = test_bench.recIntra_DCMode_test();
    std::cout << "DC Mode Test: " << (ok_dc ? "PASS" : "FAIL") << std::endl;

    bool ok_planar = test_bench.recIntra_PlanarMode_test();
    std::cout << "Planar Mode Test: " << (ok_planar ? "PASS" : "FAIL") << std::endl;

    bool ok_angular = test_bench.recIntra_AngularMode_test();
    std::cout << "Angular Mode Test: " << (ok_angular ? "PASS" : "FAIL") << std::endl;

    bool ok_mc_fallback = test_bench.mcReadReq_no_mv_fallback_test();
    std::cout << "RecMC fallback (no MV) Test: " << (ok_mc_fallback ? "PASS" : "FAIL") << std::endl;

    bool ok_mc_pre = test_bench.mcPre_to_db_test();
    std::cout << "RecMC PRE->DB Test: " << (ok_mc_pre ? "PASS" : "FAIL") << std::endl;

    bool ok_mc = test_bench.recMc_test();
    std::cout << "RecMC Test (with MV): " << (ok_mc ? "PASS" : "FAIL") << std::endl;

    bool ok_coeff_inv = test_bench.coeffThroughInvTq_test();
    std::cout << "COEFF->InvTQ->DB Test: " << (ok_coeff_inv ? "PASS" : "FAIL") << std::endl;

    bool ok_large_tq = test_bench.large_block_tq_test();
    std::cout << "Large-block TQ Test: " << (ok_large_tq ? "PASS" : "FAIL") << std::endl;

    bool ok_res = test_bench.residual_test();
    std::cout << "Residual Test: " << (ok_res ? "PASS" : "FAIL") << std::endl;

    bool ok_tq = test_bench.tq_test();
    std::cout << "TQ Test: " << (ok_tq ? "PASS" : "FAIL") << std::endl;

    bool ok_inv = test_bench.inv_tq_test();
    std::cout << "InvTQ Test: " << (ok_inv ? "PASS" : "FAIL") << std::endl;

    bool ok_pack = test_bench.pack_unpack_roundtrip_test();
    std::cout << "Pack/Unpack Roundtrip Test: " << (ok_pack ? "PASS" : "FAIL") << std::endl;

    bool ok_sub = test_bench.subblock_offset_test();
    std::cout << "Sub-block Offset Test: " << (ok_sub ? "PASS" : "FAIL") << std::endl;

    bool ok_qp = test_bench.qp_extremes_test();
    std::cout << "QP Extremes Test: " << (ok_qp ? "PASS" : "FAIL") << std::endl;

    bool ok_short = test_bench.short_payload_handling_test();
    std::cout << "Short-payload COEFF Test: " << (ok_short ? "PASS" : "FAIL") << std::endl;

    bool ok_all = ok_dc && ok_planar && ok_angular && ok_mc_fallback && ok_mc_pre && ok_mc && ok_coeff_inv && ok_large_tq && ok_res && ok_tq && ok_inv && ok_pack && ok_sub && ok_qp && ok_short;

    // Print a concise summary for debugging exit code mismatches
    std::cout << "SUMMARY: ok_dc=" << ok_dc
              << " ok_planar=" << ok_planar
              << " ok_angular=" << ok_angular
              << " ok_mc_fallback=" << ok_mc_fallback
              << " ok_mc_pre=" << ok_mc_pre
              << " ok_mc=" << ok_mc
              << " ok_coeff_inv=" << ok_coeff_inv
              << " ok_large_tq=" << ok_large_tq
              << " ok_res=" << ok_res
              << " ok_tq=" << ok_tq
              << " ok_inv=" << ok_inv
              << " ok_pack=" << ok_pack
              << " ok_sub=" << ok_sub
              << " ok_qp=" << ok_qp
              << " ok_short=" << ok_short
              << " -> ok_all=" << ok_all << std::endl;

    return ok_all ? 0 : 1;
}
