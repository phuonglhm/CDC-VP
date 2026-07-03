#include "testbench.h"

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


int sc_main(int argc, char* argv[]) {
    TestBench test_bench("testbench");
    test_bench.bs_socket.bind(test_bench.top.db_bs.start_socket);
    test_bench.mv_socket.bind(test_bench.top.db_mv.start_socket);
    test_bench.top.db_filter_sao.out_socket.bind(test_bench.out_monitor.filter_socket);

    return 0;
}