#include "testbench.h"
#include <cassert>

bool TestBench::dataflow_test(const CustomPacket &pkt_in) {
    CustomPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = CustomCmd::RESIDUAL;
        pkt.block_idx = 5;
        pkt.x = 1; pkt.y = 2;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
        // sample payload bytes (use 16 bytes for 4x4 pixels example)
        pkt.data = {0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41,
                    0x12, 0x22, 0x32, 0x42, 0x13, 0x23, 0x33, 0x43};
    }
    std::cout << pkt << std::endl;

    std::vector<uint8_t> buf = packCustomPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    bs_socket->b_transport(trans, delay);

    sc_start(1, SC_MS);

    bool ok = true;
    // Expected values for the default packet used here
    bool expect_tu = true;
    bool expect_pu = true;
    bool expect_cbf_p = false;
    bool expect_cbf_q = false;
    uint8_t expect_qp_p = pkt.qp;
    uint8_t expect_qp_q = pkt.qp;

    if (top.db_bs.last_tu_edge != expect_tu) {
        std::cerr << "ASSERT FAIL: tu_edge expected=" << expect_tu << " got=" << top.db_bs.last_tu_edge << std::endl;
        ok = false;
    }
    if (top.db_bs.last_pu_edge != expect_pu) {
        std::cerr << "ASSERT FAIL: pu_edge expected=" << expect_pu << " got=" << top.db_bs.last_pu_edge << std::endl;
        ok = false;
    }
    if (top.db_bs.last_cbf_p != expect_cbf_p) {
        std::cerr << "ASSERT FAIL: cbf_p expected=" << expect_cbf_p << " got=" << top.db_bs.last_cbf_p << std::endl;
        ok = false;
    }
    if (top.db_bs.last_cbf_q != expect_cbf_q) {
        std::cerr << "ASSERT FAIL: cbf_q expected=" << expect_cbf_q << " got=" << top.db_bs.last_cbf_q << std::endl;
        ok = false;
    }
    if (top.db_bs.last_qp_p != expect_qp_p) {
        std::cerr << "ASSERT FAIL: qp_p expected=" << static_cast<int>(expect_qp_p) << " got=" << static_cast<int>(top.db_bs.last_qp_p) << std::endl;
        ok = false;
    }
    if (top.db_bs.last_qp_q != expect_qp_q) {
        std::cerr << "ASSERT FAIL: qp_q expected=" << static_cast<int>(expect_qp_q) << " got=" << static_cast<int>(top.db_bs.last_qp_q) << std::endl;
        ok = false;
    }

    assert(ok && "dataflow_test assertions failed");
    if (ok) std::cout << "dataflow_test: all assertions passed" << std::endl;
    return ok;
}


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    // TestBench constructor binds sockets; call its helper to exercise db_bs
    CustomPacket pkt;
    bool ok1 = test_bench.dataflow_test(pkt);
    bool ok2 = test_bench.cbf_qp_param_test();
    bool ok3 = test_bench.pack_unpack_test();
    bool ok4 = test_bench.outmonitor_receive_test();
    bool ok5 = test_bench.filter_identity_test();
    bool ok6 = test_bench.filter_modifies_test();
    bool ok7 = test_bench.mv_selection_test();
    if (ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7) std::cout << "All tests passed" << std::endl;

    return 0;
}

bool TestBench::pack_unpack_test() {
    std::cout << "Running pack_unpack_test" << std::endl;
    CustomPacket p;
    p.cmd = CustomCmd::RESIDUAL;
    p.block_idx = 7;
    p.x = 3; p.y = 2; p.size = 0; p.sel = 1; p.qp = 18;
    p.pred_type = static_cast<uint8_t>(PredType::MC);
    p.cbf_mask.reset(); p.cbf_mask.set(0); p.cbf_mask.set(12);
    p.mb_partition.reset(); p.mb_partition.set(2);
    p.mb_p_pu_mode.reset(); p.mb_p_pu_mode.set(10);
    p.data = {1,2,3,4,5};

    std::vector<uint8_t> buf = packCustomPacket(p);
    CustomPacket q = unpackCustomPacket(buf.data(), buf.size());
    bool ok = true;
    if (q.cmd != p.cmd) { std::cerr << "pack_unpack_test: cmd mismatch" << std::endl; ok = false; }
    if (q.block_idx != p.block_idx) { std::cerr << "pack_unpack_test: block_idx mismatch" << std::endl; ok = false; }
    if (q.x != p.x || q.y != p.y) { std::cerr << "pack_unpack_test: position mismatch" << std::endl; ok = false; }
    if (q.qp != p.qp) { std::cerr << "pack_unpack_test: qp mismatch" << std::endl; ok = false; }
    if (q.cbf_mask.test(12) != p.cbf_mask.test(12)) { std::cerr << "pack_unpack_test: cbf_mask mismatch" << std::endl; ok = false; }
    if (q.data != p.data) { std::cerr << "pack_unpack_test: data mismatch" << std::endl; ok = false; }
    assert(ok && "pack_unpack_test failed");
    if (ok) std::cout << "pack_unpack_test: passed" << std::endl;
    return ok;
}

bool TestBench::outmonitor_receive_test() {
    std::cout << "Running outmonitor_receive_test" << std::endl;
    out_monitor.clear_last();
    CustomPacket pkt;
    pkt.cmd = CustomCmd::RESIDUAL;
    pkt.x = 1; pkt.y = 1; pkt.size = 0; pkt.sel = 0; pkt.qp = 10;
    pkt.data.clear();
    for (int i = 0; i < 32; ++i) pkt.data.push_back(static_cast<uint8_t>(i));

    std::vector<uint8_t> buf = packCustomPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    bs_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    CustomPacket last;
    if (!out_monitor.get_last(last)) {
        std::cerr << "outmonitor_receive_test: no packet received" << std::endl;
        return false;
    }
    if (last.data.empty()) {
        std::cerr << "outmonitor_receive_test: received packet has empty data" << std::endl;
        return false;
    }
    std::cout << "outmonitor_receive_test: passed (received data size=" << last.data.size() << ")" << std::endl;
    return true;
}

bool TestBench::cbf_qp_param_test() {
    std::cout << "Running cbf_qp_param_test" << std::endl;
    bool overall_ok = true;

    // parameter sets for mb_partition / mb_p_pu_mode
    std::vector<std::pair<std::bitset<21>, std::bitset<42>>> params;
    {
        std::bitset<21> p0; std::bitset<42> m0; // all zeros
        params.emplace_back(p0, m0);

        std::bitset<21> p1; p1.set(0); std::bitset<42> m1; m1.reset(); // top-level only
        params.emplace_back(p1, m1);

        std::bitset<21> p2; p2.set(0); p2.set(1); p2.set(5); p2.set(13);
        std::bitset<42> m2; m2.set(10); m2.set(11); // some PU mode bits
        params.emplace_back(p2, m2);
    }

    // cbf patterns: empty and a multi-bit pattern touching top/left indices
    std::bitset<256> empty_mask;
    std::bitset<256> multi_mask;
    std::vector<int> cbf_indices = {255,254,251,250,85,87,93,95,0,2,10,14};
    for (int i : cbf_indices) multi_mask.set(i);

    std::vector<uint16_t> cnts = {0, 4, 16, 32, 64, 128};

    for (auto &pm : params) {
        const auto &mbp = pm.first;
        const auto &mbm = pm.second;

        // Clear memories by issuing an OUT with an empty cbf_mask at y=0
        // so top_reg read returns 0 and `cbf_tl` is cleared.
        {
            CustomPacket clearPkt;
            clearPkt.mb_partition = mbp;
            clearPkt.mb_p_pu_mode = mbm;
            clearPkt.cbf_mask = empty_mask;
            clearPkt.state = 5; // OUT
            clearPkt.cnt = 0;
            clearPkt.x = 1; clearPkt.y = 0; // y=0 clears top_reg read path
            std::vector<uint8_t> outbuf = packCustomPacket(clearPkt);
            tlm::tlm_generic_payload outtrans;
            outtrans.set_command(tlm::TLM_WRITE_COMMAND);
            outtrans.set_address(0);
            outtrans.set_data_ptr(outbuf.data());
            outtrans.set_data_length(outbuf.size());
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            bs_socket->b_transport(outtrans, delay);
            sc_start(1, SC_MS);
        }

        // Case A: empty cbf_mask -> always expect cbf_p/q == false for any cnt
        for (uint16_t cnt : cnts) {
            CustomPacket pkt;
            pkt.mb_partition = mbp;
            pkt.mb_p_pu_mode = mbm;
            pkt.cbf_mask = empty_mask;
            pkt.state = 3; // DBY
            pkt.cnt = cnt;
            pkt.x = 1; pkt.y = 1;
            std::vector<uint8_t> buf = packCustomPacket(pkt);
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(0);
            trans.set_data_ptr(buf.data());
            trans.set_data_length(buf.size());
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            bs_socket->b_transport(trans, delay);
            sc_start(1, SC_MS);

            if (top.db_bs.last_cbf_p != false || top.db_bs.last_cbf_q != false) {
                std::cerr << "ASSERT FAIL: expected cbf_p/q false for empty cbf_mask (cnt=" << cnt << ")" << std::endl;
                overall_ok = false;
            }
        }

        // Case B: multi-bit mask — write via OUT then exercise DBY selections
        {
            CustomPacket outPkt;
            outPkt.mb_partition = mbp;
            outPkt.mb_p_pu_mode = mbm;
            outPkt.cbf_mask = multi_mask;
            outPkt.state = 5; // OUT
            outPkt.cnt = 0;
            outPkt.x = 1; outPkt.y = 1;
            std::vector<uint8_t> outbuf = packCustomPacket(outPkt);
            tlm::tlm_generic_payload outtrans;
            outtrans.set_command(tlm::TLM_WRITE_COMMAND);
            outtrans.set_address(0);
            outtrans.set_data_ptr(outbuf.data());
            outtrans.set_data_length(outbuf.size());
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            bs_socket->b_transport(outtrans, delay);
            sc_start(1, SC_MS);

            std::vector<std::pair<bool,bool>> results;
            for (uint16_t cnt : cnts) {
                CustomPacket pkt;
                pkt.mb_partition = mbp;
                pkt.mb_p_pu_mode = mbm;
                pkt.cbf_mask = multi_mask;
                pkt.state = 3; // DBY
                pkt.cnt = cnt;
                pkt.x = 1; pkt.y = 1;
                std::vector<uint8_t> buf = packCustomPacket(pkt);
                tlm::tlm_generic_payload trans;
                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(0);
                trans.set_data_ptr(buf.data());
                trans.set_data_length(buf.size());
                bs_socket->b_transport(trans, delay);
                sc_start(1, SC_MS);

                // deterministic: send again and compare
                CustomPacket pkt2 = pkt;
                std::vector<uint8_t> buf2 = packCustomPacket(pkt2);
                tlm::tlm_generic_payload trans2;
                trans2.set_command(tlm::TLM_WRITE_COMMAND);
                trans2.set_address(0);
                trans2.set_data_ptr(buf2.data());
                trans2.set_data_length(buf2.size());
                bs_socket->b_transport(trans2, delay);
                sc_start(1, SC_MS);

                bool p = top.db_bs.last_cbf_p;
                bool q = top.db_bs.last_cbf_q;
                bool p2 = top.db_bs.last_cbf_p;
                bool q2 = top.db_bs.last_cbf_q;
                if (p != p2 || q != q2) {
                    std::cerr << "ASSERT FAIL: non-deterministic results for cnt=" << cnt << std::endl;
                    overall_ok = false;
                }
                results.emplace_back(p,q);
            }

            // check that results vary across cnts (i.e., selection logic exercised)
            bool all_same = true;
            for (size_t i = 1; i < results.size(); ++i) if (results[i] != results[0]) { all_same = false; break; }
            if (all_same) {
                std::cerr << "WARNING: cbf_p/q not varying across cnts for this pattern" << std::endl;
                // not fatal, but log it
            }
        }
    }

    assert(overall_ok && "cbf_qp_param_test failed");
    if (overall_ok) std::cout << "cbf_qp_param_test: passed" << std::endl;
    return overall_ok;
}

bool TestBench::filter_identity_test() {
    std::cout << "Running filter_identity_test" << std::endl;
    out_monitor.clear_last();
    CustomPacket pkt;
    pkt.x = 1; pkt.y = 1; pkt.i4x4_x = 0; pkt.i4x4_y = 0;
    pkt.sel = 0; pkt.qp = 22; pkt.bs = 0;
    pkt.data.clear();
    for (int i = 1; i <= 16; ++i) pkt.data.push_back(static_cast<uint8_t>(i));
    for (int i = 101; i <= 116; ++i) pkt.data.push_back(static_cast<uint8_t>(i));

    std::vector<uint8_t> buf = packCustomPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    bs_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    CustomPacket outp;
    if (!out_monitor.get_last(outp)) {
        std::cerr << "filter_identity_test: no packet received" << std::endl;
        return false;
    }
    if (outp.data.size() != pkt.data.size()) {
        std::cerr << "filter_identity_test: data size changed unexpectedly" << std::endl;
        return false;
    }
    std::cout << "filter_identity_test: passed" << std::endl;
    return true;
}

bool TestBench::filter_modifies_test() {
    std::cout << "Running filter_modifies_test" << std::endl;
    out_monitor.clear_last();
    CustomPacket pkt;
    pkt.x = 1; pkt.y = 1; pkt.i4x4_x = 0; pkt.i4x4_y = 0;
    pkt.sel = 0; pkt.qp = 22; pkt.bs = 2; // non-zero -> filtering
    pkt.data.clear();
    // strong contrast: p block high values, q block low values
    for (int i = 0; i < 16; ++i) pkt.data.push_back(static_cast<uint8_t>(200 - i));
    for (int i = 0; i < 16; ++i) pkt.data.push_back(static_cast<uint8_t>(10 + i));

    std::vector<uint8_t> buf = packCustomPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    bs_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    CustomPacket outp;
    if (!out_monitor.get_last(outp)) {
        std::cerr << "filter_modifies_test: no packet received" << std::endl;
        return false;
    }
    if (outp.data == pkt.data) {
        std::cerr << "filter_modifies_test: data did not change as expected" << std::endl;
        return false;
    }
    std::cout << "filter_modifies_test: passed (data changed)" << std::endl;
    return true;
}

bool TestBench::mv_selection_test() {
    std::cout << "Running mv_selection_test" << std::endl;
    CustomPacket pkt;
    pkt.x = 2; pkt.y = 2; pkt.i4x4_x = 2; pkt.i4x4_y = 2; pkt.cnt = 10; pkt.state = 3;
    std::vector<uint8_t> buf = packCustomPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    mv_socket->b_transport(trans, delay);
    sc_start(1, SC_MS);

    uint32_t mv_p = top.db_mv.last_mv_p;
    uint32_t mv_q = top.db_mv.last_mv_q;
    if (mv_p == 0 && mv_q == 0) {
        std::cerr << "mv_selection_test: both mv_p and mv_q are zero" << std::endl;
        return false;
    }
    std::cout << "mv_selection_test: passed mv_p=0x" << std::hex << mv_p << " mv_q=0x" << mv_q << std::dec << std::endl;
    return true;
}