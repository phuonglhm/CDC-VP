#include "../include/top.h"
#include "../../db/include/db_bs.h"
#include "../../db/include/db_mv.h"
#include "../../db/include/db_filter_sao.h"
#include "../../common/include/custom_packet.h"
#include <iostream>

int sc_main(int argc, char* argv[]) {
    Top mem_top("mem_top");

    // We'll simulate DB output by sending a packed CustomPacket directly
    // into mem_top.db_socket via a small driver initiator.
    struct Driver : public sc_core::sc_module {
        tlm_utils::simple_initiator_socket<Driver> init_socket;
        Driver(sc_core::sc_module_name n) : sc_module(n), init_socket("init_socket") {}
    } driver("driver");
    driver.init_socket.bind(mem_top.db_socket);

    // Prepare a frame and load into mem
    cdc::components::frame f(64, 64);
    f.fill(128, 128, 128);
    mem_top.frame_memory.load_frame(0, f);

    // Build a CustomPacket WRITE (RESIDUAL) with a 4x4 block
    CustomPacket pkt;
    pkt.cmd = CustomCmd::RESIDUAL;
    pkt.x = 2; pkt.y = 3; pkt.size = 0; pkt.sel = 0; // luma at block (2,3)
    pkt.data.resize(16);
    for (int i = 0; i < 16; ++i) pkt.data[i] = static_cast<uint8_t>(i + 1);

    std::vector<uint8_t> buf = packCustomPacket(pkt);
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());

    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    driver.init_socket->b_transport(trans, d);
    sc_core::sc_start(1, sc_core::SC_MS);

    // Now read back from mem frame to verify write
    cdc::components::frame got;
    mem_top.frame_memory.get_active_frame(got);
    uint32_t px = static_cast<uint32_t>(pkt.x) * 4u;
    uint32_t py = static_cast<uint32_t>(pkt.y) * 4u;
    bool ok = true;
    for (uint32_t r = 0; r < 4; ++r) {
        for (uint32_t c = 0; c < 4; ++c) {
            uint8_t v = got.get_luma(px + c, py + r);
            uint8_t expect = pkt.data[r*4 + c];
            if (v != expect) {
                std::cerr << "mismatch at " << r << "," << c << ": got=" << int(v) << " expected=" << int(expect) << "\n";
                ok = false;
            }
        }
    }
    if (ok) std::cout << "Integration test passed" << std::endl;
    else std::cout << "Integration test FAILED" << std::endl;
    return ok ? 0 : 1;
}
