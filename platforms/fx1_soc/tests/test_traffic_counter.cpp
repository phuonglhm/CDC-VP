// TrafficCounter (fx1_soc --traffic-report, plan DIRECT_CSI_ISP D2a review F3):
// a transaction is counted when it is issued, before it is forwarded, and
// separately when it returns; one still blocked in its target is outstanding.
//
// Three counters named as on the platform. MIPI_DMA issues one write and
// ISP_IDMA one read to targets that block 20 us; ISP_ODMA issues one write
// that returns at once, then one that answers with an error. At 1 us the
// reports are written to <dir>/pending.txt (the forbidden traffic is in
// flight), at the end to <dir>/done.txt; <dir>/clean.txt is a direct-only
// report (ODMA writes only, all returned, no error) for the checker's
// positive control.
//
//   test_fx1_traffic_counter <dir>      Exit 0 when every check holds.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../src/traffic_counter.h"

using cdc::platforms::fx1_soc::TrafficCounter;

namespace {

int failures = 0;
#define CHECK(cond, what)                                                                  \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            std::cerr << "CHECK failed: " << (what) << " @ line " << __LINE__ << '\n';     \
            ++failures;                                                                    \
        }                                                                                  \
    } while (0)

struct Target : sc_core::sc_module {
    tlm_utils::simple_target_socket<Target> socket{"socket"};
    sc_core::sc_time block;
    bool error = false;
    unsigned calls = 0;
    Target(sc_core::sc_module_name n, sc_core::sc_time b) : sc_module(n), block(b)
    {
        socket.register_b_transport(this, &Target::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        ++calls;
        if (block != sc_core::SC_ZERO_TIME) sc_core::wait(block);  // a legal blocking target
        d += sc_core::sc_time(10, sc_core::SC_NS);
        t.set_response_status(error ? tlm::TLM_ADDRESS_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE);
    }
};

struct Initiator : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Initiator> socket{"socket"};
    explicit Initiator(sc_core::sc_module_name n) : sc_module(n) {}
    void access(tlm::tlm_command cmd, std::uint64_t addr, unsigned len)
    {
        unsigned char buf[64] = {};
        tlm::tlm_generic_payload t;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        t.set_command(cmd);
        t.set_address(addr);
        t.set_data_ptr(buf);
        t.set_data_length(len);
        t.set_streaming_width(len);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(t, d);
    }
};

struct Bench : sc_core::sc_module {
    Initiator mipi_i{"mipi_i"}, idma_i{"idma_i"}, odma_i{"odma_i"};
    TrafficCounter mipi{"MIPI_DMA"}, idma{"ISP_IDMA"}, odma{"ISP_ODMA"};
    Target mipi_t{"mipi_t", sc_core::sc_time(20, sc_core::SC_US)};
    Target idma_t{"idma_t", sc_core::sc_time(20, sc_core::SC_US)};
    Target odma_t{"odma_t", sc_core::SC_ZERO_TIME};
    std::string dir;

    Bench(sc_core::sc_module_name n, std::string d) : sc_module(n), dir(std::move(d))
    {
        mipi_i.socket.bind(mipi.target);
        mipi.out.bind(mipi_t.socket);
        idma_i.socket.bind(idma.target);
        idma.out.bind(idma_t.socket);
        odma_i.socket.bind(odma.target);
        odma.out.bind(odma_t.socket);
        SC_THREAD(run_mipi);
        SC_THREAD(run_idma);
        SC_THREAD(run_odma);
        SC_THREAD(observe);
    }
    SC_HAS_PROCESS(Bench);

    void run_mipi() { mipi_i.access(tlm::TLM_WRITE_COMMAND, 0x81000000u, 64); }
    void run_idma() { idma_i.access(tlm::TLM_READ_COMMAND, 0x81000000u, 32); }
    void run_odma()
    {
        odma_i.access(tlm::TLM_WRITE_COMMAND, 0x88000000u, 8);
        sc_core::wait(5, sc_core::SC_US);
        odma_t.error = true;
        odma_i.access(tlm::TLM_WRITE_COMMAND, 0x88000040u, 16);
    }
    void dump(const std::string& file, bool clean)
    {
        std::ofstream o(dir + "/" + file);
        if (clean) {
            o << "MIPI_DMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 "
                 "write_errors=0 outstanding=0\n"
                 "ISP_IDMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 "
                 "write_errors=0 outstanding=0\n";
            o << odma_clean << '\n';
            return;
        }
        o << mipi.report("MIPI_DMA") << '\n' << idma.report("ISP_IDMA") << '\n' << odma.report("ISP_ODMA") << '\n';
    }
    std::string odma_clean;

    void observe()
    {
        sc_core::wait(1, sc_core::SC_US);
        // Issued, not returned: visible and outstanding.
        CHECK(mipi_t.calls == 1 && idma_t.calls == 1, "both blocking targets entered");
        CHECK(mipi.writes().txns == 1 && mipi.writes().bytes == 64 && mipi.writes().done == 0,
              "MIPI write counted at issue, not done");
        CHECK(mipi.writes().lo == 0x81000000u && mipi.writes().hi == 0x8100003Fu, "MIPI write address range at issue");
        CHECK(idma.reads().txns == 1 && idma.reads().bytes == 32 && idma.reads().done == 0,
              "IDMA read counted at issue, not done");
        CHECK(mipi.outstanding() == 1 && idma.outstanding() == 1, "outstanding while blocked");
        CHECK(odma.writes().txns == 1 && odma.writes().done == 1 && odma.outstanding() == 0, "ODMA write returned");
        odma_clean = odma.report("ISP_ODMA");
        dump("pending.txt", false);
        dump("clean.txt", true);
        sc_core::wait(30, sc_core::SC_US);
        CHECK(mipi.writes().done == 1 && idma.reads().done == 1, "blocked transactions returned");
        CHECK(mipi.outstanding() == 0 && idma.outstanding() == 0 && odma.outstanding() == 0, "nothing outstanding");
        CHECK(odma.writes().txns == 2 && odma.writes().done == 2 && odma.writes().errors == 1,
              "ODMA error response counted on return");
        CHECK(odma.writes().hi == 0x8800004Fu, "ODMA highest byte");
        const std::string r = odma.report("ISP_ODMA");
        CHECK(r.find(" writes=2 write_bytes=24 write_done=2 write_errors=1") != std::string::npos &&
                  r.find(" outstanding=0") != std::string::npos,
              "report keys: " + r);
        dump("done.txt", false);
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: test_fx1_traffic_counter <dir>\n";
        return 2;
    }
    Bench b("bench", argv[1]);
    sc_core::sc_start();
    std::cout << (failures ? "FAIL" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
