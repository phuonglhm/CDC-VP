#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

#include "fetch_packet.h"
#include "testbench.h"
#include "out_monitor.cpp"
#include "top.h"
#include <iostream>

#include "testbench.h"
using namespace sc_core;


// Initiator that sends a LOAD and a WRITE_4x4 request
struct FetchInitiator : sc_core::sc_module {
    SC_HAS_PROCESS(FetchInitiator);
    tlm_utils::simple_initiator_socket<FetchInitiator> socket;

    FetchInitiator(sc_core::sc_module_name name) : sc_core::sc_module(name), socket("socket") {
        SC_THREAD(run);
    }

    void run() {
        wait(sc_core::sc_time(1, sc_core::SC_NS));

        // 1) LOAD request (8x4 rectangle)
        FetchPacket load;
        load.cmd = FetchCmd::LOAD;
        load.plane = 0; load.x_px = 10; load.y_px = 20; load.width = 8; load.height = 4; load.req_id = 0xA1;
        auto buf = packFetchPacket(load);

        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(const_cast<uint8_t*>(buf.data()));
        trans.set_data_length(buf.size());
        trans.set_streaming_width(buf.size());
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) std::cerr << "FetchInitiator: LOAD failed\n";

        wait(sc_core::sc_time(50, sc_core::SC_NS));

        // 2) WRITE_4x4 request
        FetchPacket w;
        w.cmd = FetchCmd::WRITE_4x4; w.plane = 0; w.x_px = 12; w.y_px = 24; w.req_id = 0xB2;
        w.data.resize(16);
        for (int i = 0; i < 16; ++i) w.data[i] = static_cast<uint8_t>(200 + i);
        auto wbuf = packFetchPacket(w);

        tlm::tlm_generic_payload wtrans;
        wtrans.set_command(tlm::TLM_WRITE_COMMAND);
        wtrans.set_address(0);
        wtrans.set_data_ptr(const_cast<uint8_t*>(wbuf.data()));
        wtrans.set_data_length(wbuf.size());
        wtrans.set_streaming_width(wbuf.size());
        wtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(wtrans, delay);
        if (wtrans.get_response_status() != tlm::TLM_OK_RESPONSE) std::cerr << "FetchInitiator: WRITE_4x4 failed\n";

        wait(sc_core::sc_time(100, sc_core::SC_NS));
        sc_stop();
    }
};

// Implement TestBench constructor and run thread
TestBench::TestBench(sc_core::sc_module_name name)
    : sc_core::sc_module(name), top("top"), init(nullptr), out(nullptr)
{
    // instantiate helper modules
    init = new FetchInitiator("init");
    out = new FetchOutReceiver("out");

    // bind sockets
    init->socket.bind(top.fetch.start_socket);
    top.fetch.out_socket.bind(out->start_socket);

}

void TestBench::run() {
    // Post-simulation check: by the time sc_start() returns the initiator
    // should have completed and `out` should have the response.
    bool ok = true;
    if (!out->got) { std::cerr << "test_fetch_wrapper: no response received\n"; ok = false; }
    if (ok && out->last.cmd != FetchCmd::LOAD_RESP) { std::cerr << "test_fetch_wrapper: unexpected cmd\n"; ok = false; }
    if (ok && out->last.req_id != 0xA1) { std::cerr << "test_fetch_wrapper: req_id mismatch\n"; ok = false; }
    if (ok && out->last.data.size() != static_cast<size_t>(8 * 4)) { std::cerr << "test_fetch_wrapper: data size mismatch\n"; ok = false; }

    auto encodeAddr = [](uint8_t plane, uint32_t x, uint32_t y) -> uint64_t {
        return (static_cast<uint64_t>(plane) << 56) | (static_cast<uint64_t>(y) << 16) | (static_cast<uint64_t>(x));
    };

    if (ok) {
        std::vector<uint8_t> mem = top.fetch.simple_mem.read_region(encodeAddr(0, 12, 24), 16);
        for (int i = 0; i < 16; ++i) {
            if (mem[i] != static_cast<uint8_t>(200 + i)) {
                std::cerr << "test_fetch_wrapper: mem mismatch at " << i << " got=" << int(mem[i]) << " expected=" << (200 + i) << "\n";
                ok = false; break;
            }
        }
    }

    std::cout << "fetch_wrapper_test: " << (ok ? "PASS" : "FAIL") << std::endl;
}

int sc_main(int argc, char* argv[]) {
    TestBench tb("testbench");
    sc_core::sc_start();
    tb.run();
    return 0;
}
