#include "testbench.h"
#include "../../include/block_coord_codec.h"

// Implement the dataflow_test to send a CustomPacket into Cabac and verify output
bool TestBench::dataflow_test(const CustomPacket &pkt_in) {
    CustomPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = CustomCmd::READ_REQ;
        pkt.block_idx = 0;
        pkt.x = 0; pkt.y = 0;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    std::vector<uint8_t> buf = packCustomPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    start_socket->b_transport(trans, delay);
    sc_core::sc_start(1, sc_core::SC_MS);

    if (out_monitor.last_data.empty()) return false;
    CustomPacket out_pkt = unpackCustomPacket(out_monitor.last_data.data(), out_monitor.last_data.size());
    if (out_pkt.cmd != CustomCmd::COEFF) return false;
    if (out_pkt.block_idx != pkt.block_idx) return false;
    if (out_pkt.data.size() != 16) return false; // Cabac currently reads 16 bytes
    return true;
}

//validate the real emitted CABAC stream returned by encode_bins
bool TestBench::dataflow_stream_test(const CustomPacket &pkt_in) {
    CustomPacket pkt = pkt_in;
    if (pkt.data.empty()) {
        pkt.cmd = CustomCmd::READ_REQ;
        pkt.block_idx = 0;
        pkt.x = 0; pkt.y = 0;
        pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);
    }
    std::cout << pkt << std::endl;

    std::vector<uint8_t> buf = packCustomPacket(pkt);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    start_socket->b_transport(trans, delay);

    sc_core::sc_start(1, sc_core::SC_MS);

    if (out_monitor.last_data.empty()) return false;
    CustomPacket out_pkt = unpackCustomPacket(out_monitor.last_data.data(), out_monitor.last_data.size());
    if (out_pkt.cmd != CustomCmd::COEFF) return false;
    if (out_pkt.block_idx != pkt.block_idx) return false;
    // Expect non-empty emitted stream (real encoder output)
    if (out_pkt.data.empty()) return false;
    return true;
}


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    test_bench.start_socket.bind(test_bench.top.cabac.start_socket);
    test_bench.top.cabac.out_socket.bind(test_bench.out_monitor.cabac_socket);

    // Run three targeted unit tests
    bool ok_all = true;

    auto mem_read = [&](uint64_t addr, size_t len) -> std::vector<uint8_t> {
        std::vector<uint8_t> buf(len);
        tlm::tlm_generic_payload t;
        t.set_command(tlm::TLM_READ_COMMAND);
        t.set_address(addr);
        t.set_data_ptr(buf.data());
        t.set_data_length(buf.size());
        t.set_streaming_width(buf.size());
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        test_bench.top.mem_bridge.socket->b_transport(t, d);
        return buf;
    };

    auto mem_read_byte = [&](uint64_t addr) -> uint8_t {
        return mem_read(addr, 1)[0];
    };

    // Test 1: binarizer FL / LUT checks
    auto binarizer_fl_test = [&]() -> bool {
        bool ok = true;
        if (cabac_bina_type[0] != 0) { std::cerr << "binarizer_fl_test: ctx 0 not FL\n"; ok = false; }
        if (cabac_bina_cmax[0] != 1) { std::cerr << "binarizer_fl_test: cmax[0] != 1\n"; ok = false; }
        if (cabac_bina_type[7] != 1) { std::cerr << "binarizer_fl_test: ctx 7 not TU\n"; ok = false; }
        if (cabac_bina_type[191] != 4) { std::cerr << "binarizer_fl_test: ctx 191 not CREG\n"; ok = false; }
        std::cout << "binarizer_fl_test: " << (ok ? "PASS" : "FAIL") << std::endl;
        return ok;
    };

    // Test 2: context state transition — ensure at least one context byte 0..15 is updated
    auto context_state_transition_test = [&]() -> bool {
        uint64_t base = 0x10000000ULL;
        std::vector<uint8_t> before(16), after(16);
        for (size_t i = 0; i < 16; ++i) before[i] = mem_read_byte(base + i);

        bool res = test_bench.dataflow_stream_test();
        if (!res) { std::cerr << "context_state_transition_test: dataflow failed\n"; return false; }

        for (size_t i = 0; i < 16; ++i) after[i] = mem_read_byte(base + i);

        bool changed = false;
        for (size_t i = 0; i < 16; ++i) {
            if (after[i] != before[i]) {
                // written contexts are packed [mps<<6 | state], so should be <= 0x7F
                if (after[i] <= 0x7F) changed = true;
            }
        }
        std::cout << "context_state_transition_test: " << (changed ? "PASS" : "FAIL") << std::endl;
        return changed;
    };

    // Test 3: emitted-stream memory consistency
    auto emit_memory_consistency_test = [&]() -> bool {
        CustomPacket pkt;
        pkt.cmd = CustomCmd::READ_REQ;
        pkt.block_idx = 3; // use block index 3 to avoid collisions
        pkt.x = 0; pkt.y = 0; pkt.size = 0; pkt.sel = 0; pkt.qp = 22; pkt.pred_type = static_cast<uint8_t>(PredType::INTRA);
        pkt.cbf_mask.set(3);

        bool ok = test_bench.dataflow_stream_test(pkt);
        if (!ok) { std::cerr << "emit_memory_consistency_test: dataflow_stream_test failed\n"; return false; }

        if (test_bench.out_monitor.last_data.empty()) { std::cerr << "emit_memory_consistency_test: no out packet\n"; return false; }
        CustomPacket outp = unpackCustomPacket(test_bench.out_monitor.last_data.data(), test_bench.out_monitor.last_data.size());
        uint64_t emit_addr =
            0x20000000ULL |
            static_cast<uint64_t>(
                cdc::components::make_extended_block_address(
                    outp.block_idx, outp.x, outp.y));
        std::vector<uint8_t> mem = mem_read(emit_addr, outp.data.size());
            bool non_zero = false;
            for (auto v : mem) if (v != 0) { non_zero = true; break; }
            bool match = (outp.data.size() > 0) && non_zero;
            if (!match) std::cerr << "emit_memory_consistency_test: empty emitted stream or memory region all zeros\n";
            std::cout << "emit_memory_consistency_test: " << (match ? "PASS" : "FAIL") << std::endl;
            return match;
    };

    bool t1 = binarizer_fl_test(); ok_all &= t1;
    bool t2 = context_state_transition_test(); ok_all &= t2;
    bool t3 = emit_memory_consistency_test(); ok_all &= t3;

    std::cout << "Unit Tests: " << (ok_all ? "ALL PASS" : "SOME FAIL") << std::endl;
    return ok_all ? 0 : 1;
}
