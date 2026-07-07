#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

#include <unordered_map>
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>

#include "fetch_packet.h"
#include "testbench.h"
#include "fetch_frame_loader.h"
#include "fetch_loader_registry.h"
#include "top.h"
#include <iostream>

#include "testbench.h"
// Test-only TLM bridge + memory used to bind FetchWrapper::mem_socket
#include "fetch_mem_bridge.h"
#include "fetch_simple_memory.h"
// IME integration headers (used by the integration runner)
#include "../../ime/include/ime.h"
#include "../../common/include/frame.h"
#include "../../common/include/block.h"
#include "../../common/include/encoder_defs.h"
#include "../../fme/include/fme.h"
// POSI/PREI for integration-run POSI checks
#include "../../posi/include/posi.h"
#include "../../prei/include/prei.h"
using namespace sc_core;

// must match FetchWrapper's magic bit so test-only monitors can recognize
static constexpr uint64_t FETCH_ADDR_MAGIC = (1ULL << 55);


// FetchFrameLoader acts as an initiator (sends LOAD/WRITE requests) and
// a target (receives LOAD_RESP responses). It also provides a blocking
// `load_rect()` helper to populate frame regions via TLM.

// Test-only TLM memory monitor: simple backing store + TLM target socket.
struct FetchMemMonitor : sc_core::sc_module {
    tlm_utils::simple_target_socket<FetchMemMonitor> t_socket;
    std::unordered_map<uint64_t, uint8_t> mem;

    FetchMemMonitor(sc_core::sc_module_name name)
        : sc_core::sc_module(name), t_socket("t_socket") {
        t_socket.register_b_transport(this, &FetchMemMonitor::b_transport);
    }

    void load_data(uint64_t addr, const std::vector<uint8_t>& data) {
        for (size_t i = 0; i < data.size(); ++i) mem[addr + i] = data[i];
    }

    std::vector<uint8_t> read_region(uint64_t addr, size_t len) {
        std::vector<uint8_t> out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            auto it = mem.find(addr + i);
            out.push_back(it != mem.end() ? it->second : 0);
        }
        return out;
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        auto cmd = trans.get_command();
        uint64_t addr = trans.get_address();
        // tolerate fetch-origin addresses marked with the magic bit
        if ((addr & FETCH_ADDR_MAGIC) != 0) addr &= ~FETCH_ADDR_MAGIC;
        unsigned char* ptr = trans.get_data_ptr();
        uint32_t len = trans.get_data_length();

        if (cmd == tlm::TLM_READ_COMMAND) {
            for (uint32_t i = 0; i < len; ++i) {
                auto it = mem.find(addr + i);
                ptr[i] = (it != mem.end()) ? it->second : 0;
            }
        } else if (cmd == tlm::TLM_WRITE_COMMAND) {
            for (uint32_t i = 0; i < len; ++i) mem[addr + i] = ptr[i];
            std::cerr << "FetchMemMonitor: WRITE addr=0x" << std::hex << addr << std::dec << " len=" << len << " <- ";
            for (uint32_t i = 0; i < len; ++i) std::cerr << int(ptr[i]) << (i+1<len?" ":"\n");
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// Implement TestBench constructor and run thread
TestBench::TestBench(sc_core::sc_module_name name)
    : sc_core::sc_module(name), top(nullptr), loader(nullptr)
{
    // instantiate helper modules
    loader = new FetchFrameLoader("loader");
    // register shared loader for other modules/tests to obtain
    fetch::register_loader(loader);

    // decide mode: env var FETCH_INTEGRATION=1 selects integration path
    bool integration = false;
    const char* env = std::getenv("FETCH_INTEGRATION");
    if (env && env[0] != '\0') integration = true;

    if (integration) {
        // Integration: test-scoped MemBridge + TLM memory (no CABAC tables)
        bridge = new FetchMemBridge("bridge");
        ext_mem = new FetchSimpleMemory("ext_mem");
        // bind bridge to external memory
        bridge->socket.bind(ext_mem->socket);

        // fetch instance (use mem_socket -> bridge)
        fetch_tb = new FetchWrapper("fetch_tb", true); // enable mem_socket
        fetch_tb->mem_socket.bind(bridge->t_socket);
    } else {
        // Unit-mode: use simple TLM monitor
        mem_mon = new FetchMemMonitor("mem_mon");
        fetch_tb = new FetchWrapper("fetch_tb", true); // enable mem_socket
        // bind fetch's mem initiator to the monitor target
        fetch_tb->mem_socket.bind(mem_mon->t_socket);
    }

    // bind sockets: use the test-local fetch instance
    loader->socket.bind(fetch_tb->start_socket);
    fetch_tb->out_socket.bind(loader->resp_socket);

    // Add an integration runner that will fetch a whole frame via the
    // FetchFrameLoader, call IME, and stop the simulation when done.
    struct IMEIntegrationRunner : sc_core::sc_module {
        FetchFrameLoader* loader;
        FetchWrapper* fetch;
        SC_HAS_PROCESS(IMEIntegrationRunner);
        IMEIntegrationRunner(sc_core::sc_module_name name, FetchFrameLoader* l, FetchWrapper* f)
            : sc_core::sc_module(name), loader(l), fetch(f) {
            SC_THREAD(run);
        }

        void run() {
            // wait until initial fetch activity completed
            while (!loader->got) wait(sc_core::sc_time(1, sc_core::SC_NS));

            const uint32_t W = 64, H = 64;
            std::vector<uint8_t> data;
            bool ok = loader->load_rect(0, 0, 0, W, H, data);
            if (!ok) {
                std::cerr << "IMEIntegrationRunner: load_rect failed\n";
                sc_core::sc_stop();
                return;
            }

            // build frame
            cdc::components::frame input(W, H);
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    input.set_luma(x, y, data[y * W + x]);
                }
            }

            // noisy reference
            cdc::components::frame reference = input;
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    const std::uint8_t old_value = reference.get_luma(x, y);
                    const std::uint8_t noise = static_cast<std::uint8_t>(((x * 3u + y * 5u) & 0x07u));
                    reference.set_luma(x, y, static_cast<std::uint8_t>(old_value ^ noise));
                }
            }

            cdc::components::block ctu(16, 16, 16, cdc::components::block_type::ctu);
            cdc::components::ime dut;
            auto result = dut.run(input, reference, ctu, cdc::components::INIT_QP);

            std::cerr << "IMEIntegrationRunner: result.valid=" << result.valid << " best_mv=(" << result.best_mv.x << "," << result.best_mv.y << ")\n";

            // allow other integration runners (FME, POSI) time to run before
            // stopping the simulation. Increase wait to give them time.
            wait(sc_core::sc_time(5000, sc_core::SC_NS));
            sc_core::sc_stop();
        }
    };

    // FME integration runner: fetch a full frame, run IME to get integer
    // motion candidates, then call FME to refine and print result.
    struct FMEIntegrationRunner : sc_core::sc_module {
        FetchFrameLoader* loader;
        FetchWrapper* fetch;
        SC_HAS_PROCESS(FMEIntegrationRunner);
        FMEIntegrationRunner(sc_core::sc_module_name name, FetchFrameLoader* l, FetchWrapper* f)
            : sc_core::sc_module(name), loader(l), fetch(f) {
            SC_THREAD(run);
        }

        void run() {
            // wait until initial fetch activity completed
            while (!loader->got) wait(sc_core::sc_time(1, sc_core::SC_NS));

            const uint32_t W = 64, H = 64;
            std::vector<uint8_t> data;
            bool ok = loader->load_rect(0, 0, 0, W, H, data);
            if (!ok) {
                std::cerr << "FMEIntegrationRunner: load_rect failed\n";
                return;
            }

            // build frame
            cdc::components::frame input(W, H);
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    input.set_luma(x, y, data[y * W + x]);
                }
            }

            // noisy reference
            cdc::components::frame reference = input;
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    const std::uint8_t old_value = reference.get_luma(x, y);
                    const std::uint8_t noise = static_cast<std::uint8_t>(((x * 3u + y * 5u) & 0x07u));
                    reference.set_luma(x, y, static_cast<std::uint8_t>(old_value ^ noise));
                }
            }

            cdc::components::block ctu(16, 16, 16, cdc::components::block_type::ctu);

            // run IME to produce ime_result
            cdc::components::ime ime_dut;
            auto ime_info = ime_dut.run(input, reference, ctu, cdc::components::INIT_QP);

            // run FME refine
            cdc::components::fme fme_dut;
            auto fres = fme_dut.run(input, reference, ime_info, cdc::components::INIT_QP);

            std::cerr << "FMEIntegrationRunner: result.valid=" << fres.valid << " best_mv=(" << fres.best_mv.x << "," << fres.best_mv.y << ") cost=" << fres.best_cost << "\n";
        }
    };

    // POSI integration runner: fetch a full frame, run PREI then POSI
    struct POSIIntegrationRunner : sc_core::sc_module {
        FetchFrameLoader* loader;
        FetchWrapper* fetch;
        SC_HAS_PROCESS(POSIIntegrationRunner);
        POSIIntegrationRunner(sc_core::sc_module_name name, FetchFrameLoader* l, FetchWrapper* f)
            : sc_core::sc_module(name), loader(l), fetch(f) {
            SC_THREAD(run);
        }

        void run() {
            while (!loader->got) wait(sc_core::sc_time(1, sc_core::SC_NS));

            const uint32_t W = 64, H = 64;
            std::vector<uint8_t> data;
            bool ok = loader->load_rect(0, 0, 0, W, H, data);
            if (!ok) {
                std::cerr << "POSIIntegrationRunner: load_rect failed\n";
                return;
            }

            cdc::components::frame input(W, H);
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    input.set_luma(x, y, data[y * W + x]);
                }
            }

            // build a reconstructed frame (slightly perturbed)
            cdc::components::frame recon = input;
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    const std::uint8_t old_value = recon.get_luma(x, y);
                    const std::uint8_t noise = static_cast<std::uint8_t>(((x * 7u + y * 13u) & 0x03u));
                    recon.set_luma(x, y, static_cast<std::uint8_t>(old_value ^ noise));
                }
            }

            cdc::components::block region(16, 16, 16, cdc::components::block_type::cu);

            cdc::components::prei prei_dut;
            auto prei_info = prei_dut.run(input, region);

            cdc::components::posi posi_dut;
            auto pres = posi_dut.run(input, recon, region, prei_info, cdc::components::INIT_QP);

            std::cerr << "POSIIntegrationRunner: result.valid=" << pres.valid << " intra_mode=" << static_cast<int>(pres.intra_mode) << "\n";
        }
    };

    // instantiate integration runners (they will wait for the loader and run)
    new IMEIntegrationRunner("ime_runner", loader, fetch_tb);
    new FMEIntegrationRunner("fme_runner", loader, fetch_tb);
    new POSIIntegrationRunner("posi_runner", loader, fetch_tb);

    // preload memory for the LOAD request region (plane 0, x=10..17, y=20..23)
    for (uint32_t r = 0; r < 4; ++r) {
        uint32_t x = 10;
        uint32_t y = 20 + r;
        // Fetch address (for unit monitor) is (plane<<56)|(y<<16)|x
        uint64_t fetch_addr = (static_cast<uint64_t>(0) << 56) | (static_cast<uint64_t>(20 + r) << 16) | (static_cast<uint64_t>(10) & 0xFFFFULL);
        std::vector<uint8_t> v(8, 128);
        if (integration && ext_mem) {
            // The bridge translates fetch-origin addresses into the Rec-packed
            // format per-pixel before forwarding; preload the external memory
            // at each per-pixel Rec-packed address so subsequent per-byte
            // reads return the expected values.
            for (uint32_t c = 0; c < 8; ++c) {
                uint64_t xx = static_cast<uint64_t>(10 + c) & 0x1FFULL;
                uint64_t yy = static_cast<uint64_t>(20 + r) & 0x1FFULL;
                uint64_t pp = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t pd = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t s = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t rec_addr = (pp << (2 + 2 + 9)) | (pd << (2 + 9)) | (s << 9) | (xx << 9) | yy;
                std::vector<uint8_t> one(1, 128);
                ext_mem->load_data(rec_addr, one);
            }
        } else if (mem_mon) {
            mem_mon->load_data(fetch_addr, v);
        }
    }

    // Preload a full 64x64 luma plane so integration runner can fetch an
    // entire frame via the FetchFrameLoader. Uses same gradient as IME unit
    // test so results are comparable.
    const uint32_t W = 64, H = 64;
    for (uint32_t yy = 0; yy < H; ++yy) {
        // build row data
        std::vector<uint8_t> row;
        row.reserve(W);
        for (uint32_t xx = 0; xx < W; ++xx) {
            const uint32_t value = (xx * 9u + yy * 17u + ((xx * yy) % 37u) + ((xx ^ yy) & 0x1fu)) & 0xffu;
            row.push_back(static_cast<uint8_t>(value));
        }
        if (integration && ext_mem) {
            for (uint32_t xx = 0; xx < W; ++xx) {
                uint64_t xxv = static_cast<uint64_t>(xx) & 0x1FFULL;
                uint64_t yyv = static_cast<uint64_t>(yy) & 0x1FFULL;
                uint64_t pp = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t pd = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t s = static_cast<uint64_t>(0) & 0x3ULL;
                uint64_t rec_addr = (pp << (2 + 2 + 9)) | (pd << (2 + 9)) | (s << 9) | (xxv << 9) | yyv;
                std::vector<uint8_t> one(1, row[xx]);
                ext_mem->load_data(rec_addr, one);
            }
        } else if (mem_mon) {
            uint64_t fetch_row_addr = (static_cast<uint64_t>(0) << 56) | (static_cast<uint64_t>(yy) << 16) | (static_cast<uint64_t>(0) & 0xFFFFULL);
            mem_mon->load_data(fetch_row_addr, row);
        }
    }

}

void TestBench::run() {
    // Post-simulation check: by the time sc_start() returns the initiator
    // should have completed and `out` should have the response.
    bool ok = true;
    std::vector<std::string> errors;
    if (!loader->have_first_resp) { errors.push_back("test_fetch_wrapper: no response received"); ok = false; }
    if (loader->have_first_resp && loader->first_resp.cmd != FetchCmd::LOAD_RESP) { errors.push_back("test_fetch_wrapper: unexpected cmd"); ok = false; }
    if (loader->have_first_resp && loader->first_resp.req_id != 0xA1) { errors.push_back("test_fetch_wrapper: req_id mismatch"); ok = false; }
    if (loader->have_first_resp && loader->first_resp.data.size() != static_cast<size_t>(8 * 4)) { errors.push_back("test_fetch_wrapper: data size mismatch"); ok = false; }

    auto encodeAddr = [](uint8_t plane, uint32_t x, uint32_t y) -> uint64_t {
        return (static_cast<uint64_t>(plane) << 56) | (static_cast<uint64_t>(y) << 16) | (static_cast<uint64_t>(x));
    };

    if (ok) {
        // Verify the 4x4 block was written as pixel rows at (12,24)..(15,27)
        for (int r = 0; r < 4; ++r) {
            std::vector<uint8_t> mem_row(4);
            if (mem_mon) {
                uint64_t fetch_a = encodeAddr(0, 12, 24 + r);
                mem_row = mem_mon->read_region(fetch_a, 4);
            } else if (bridge) {
                // read per-pixel via bridge's initiator socket using Rec-packed addresses
                // (multi-byte raw reads are not guaranteed contiguous in the Rec encoding)
                for (int c = 0; c < 4; ++c) {
                    uint64_t xx = static_cast<uint64_t>(12 + c) & 0x1FFULL;
                    uint64_t yy = static_cast<uint64_t>(24 + r) & 0x1FFULL;
                    uint64_t pp = static_cast<uint64_t>(0) & 0x3ULL;
                    uint64_t pd = static_cast<uint64_t>(0) & 0x3ULL;
                    uint64_t s = static_cast<uint64_t>(0) & 0x3ULL;
                    uint64_t rec_addr = (pp << (2 + 2 + 9)) | (pd << (2 + 9)) | (s << 9) | (xx << 9) | yy;

                    tlm::tlm_generic_payload trans;
                    trans.set_command(tlm::TLM_READ_COMMAND);
                    trans.set_address(rec_addr);
                    trans.set_data_ptr(&mem_row[c]);
                    trans.set_data_length(1);
                    trans.set_streaming_width(1);
                    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
                    bridge->socket->b_transport(trans, d);
                    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) { std::cerr << "test_fetch_wrapper: bridge read failed\n"; ok = false; break; }
                }
                if (!ok) break;
            } else {
                errors.push_back("test_fetch_wrapper: no memory to verify");
                ok = false; break;
            }

            // debug: print the row we read back
            std::cerr << "Verify row " << r << ": ";
            for (int c = 0; c < 4; ++c) std::cerr << int(mem_row[c]) << " ";
            std::cerr << "\n";
            for (int c = 0; c < 4; ++c) {
                int idx = r * 4 + c;
                if (mem_row[c] != static_cast<uint8_t>(200 + idx)) {
                    std::ostringstream ss;
                    ss << "test_fetch_wrapper: mem mismatch at (" << r << "," << c << ") got=" << int(mem_row[c]) << " expected=" << (200 + idx);
                    errors.push_back(ss.str());
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
        }
    }

    // print accumulated errors after verification rows to avoid interleaved prints
    if (!errors.empty()) {
        for (const auto &e : errors) std::cerr << e << "\n";
    }

    // record pass/fail in the testbench instance
    this->passed = ok;
}

int sc_main(int argc, char* argv[]) {
    TestBench tb("testbench");
    sc_core::sc_start();
    tb.run();
    return tb.passed ? 0 : 1;
}
