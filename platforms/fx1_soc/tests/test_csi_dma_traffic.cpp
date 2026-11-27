// MIPI_DMA traffic proof with the RAW dump (plan DIRECT_CSI_ISP D2b review
// F1 / F2), on the product classes TrafficCounter and CsiDmaFault.
//
// Chains, each initiator -> ... -> RAM (0x8100_0000, 128 KiB), 8-byte writes:
//   fault  initiator -> counter (2 side windows) -> CsiDmaFault(2) -> RAM, the
//          platform topology: 3 writes in the sides, the 2nd answered SLVERR:
//          the counter has 3 issued / 3 returned / 1 error, the shim saw 3
//   lost   initiator -> CsiDmaFault(2) -> counter -> RAM, the D2b topology the
//          review refused (bench only): the counter misses the injected write
//   clean  3 writes: side 0, side 0 + 0x80, side 1: outside 0
//   gap    side 0, 0x8100_8000 (between the sides), side 1: outside 1, and the
//          RAM in the gap was written (the counter only reports, it forwards)
//   cross  one 16-byte write at 0x8100_0FF8 (past the end of side 0): outside 1
// For each chain <dir>/<name>.txt is a console as fx1_soc prints it (traffic
// lines, and the shim's report line where there is a shim), for the host
// checker's fixtures (platforms/fx1_soc/CMakeLists.txt).
//
//   test_fx1_csi_dma_traffic <dir>      Exit 0 when every check holds.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../src/csi_dma_fault.h"
#include "../src/traffic_counter.h"

using cdc::platforms::fx1_soc::CsiDmaFault;
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

constexpr std::uint64_t kBase = 0x81000000u, kSide1 = 0x81010000u, kSize = 0x20000u;

struct Ram : sc_core::sc_module {
    tlm_utils::simple_target_socket<Ram> socket{"socket"};
    std::vector<unsigned char> mem = std::vector<unsigned char>(kSize, 0xA5);
    explicit Ram(sc_core::sc_module_name n) : sc_module(n) { socket.register_b_transport(this, &Ram::b_transport); }
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        const std::uint64_t a = t.get_address() - kBase;
        if (t.get_address() < kBase || a + t.get_data_length() > kSize) {
            t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (t.is_write()) std::memcpy(&mem[a], t.get_data_ptr(), t.get_data_length());
        d += sc_core::sc_time(10, sc_core::SC_NS);
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct Initiator : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Initiator> socket{"socket"};
    unsigned errors = 0;
    explicit Initiator(sc_core::sc_module_name n) : sc_module(n) {}
    void write(std::uint64_t addr, unsigned len)
    {
        unsigned char buf[64];
        std::memset(buf, 0x5A, sizeof buf);
        tlm::tlm_generic_payload t;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        t.set_command(tlm::TLM_WRITE_COMMAND);
        t.set_address(addr);
        t.set_data_ptr(buf);
        t.set_data_length(len);
        t.set_streaming_width(len);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(t, d);
        if (!t.is_response_ok()) ++errors;
    }
};

// One chain: initiator, counter with the two side windows, optional shim, RAM.
struct Chain {
    std::string name;
    std::unique_ptr<Initiator> ini;
    std::unique_ptr<TrafficCounter> cnt;
    std::unique_ptr<CsiDmaFault> shim;
    std::unique_ptr<Ram> ram;
    std::vector<std::pair<std::uint64_t, unsigned>> writes;
    Chain(const std::string& n, bool with_shim, bool shim_first, std::vector<std::pair<std::uint64_t, unsigned>> w)
        : name(n), writes(std::move(w))
    {
        ini = std::make_unique<Initiator>((n + "_ini").c_str());
        cnt = std::make_unique<TrafficCounter>((n + "_cnt").c_str());
        cnt->add_window(kBase, kBase + 0xFFF);
        cnt->add_window(kSide1, kSide1 + 0xFFF);
        ram = std::make_unique<Ram>((n + "_ram").c_str());
        if (!with_shim) {
            ini->socket.bind(cnt->target);
            cnt->out.bind(ram->socket);
            return;
        }
        shim = std::make_unique<CsiDmaFault>((n + "_shim").c_str(), 2);
        if (shim_first) {
            ini->socket.bind(shim->target);
            shim->out.bind(cnt->target);
            cnt->out.bind(ram->socket);
        } else {
            ini->socket.bind(cnt->target);
            cnt->out.bind(shim->target);
            shim->out.bind(ram->socket);
        }
    }
    void run()
    {
        for (const auto& [a, n] : writes) ini->write(a, n);
    }
    // A console as fx1_soc prints it: the three traffic lines, then the shim's report.
    void dump(const std::string& dir) const
    {
        std::ofstream o(dir + "/" + name + ".txt");
        o << "[fx1_soc] traffic:\n"
          << cnt->report("MIPI_DMA") << '\n'
          << "ISP_IDMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=0 write_bytes=0 write_done=0 "
             "write_errors=0 outstanding=0\n"
          << "ISP_ODMA reads=0 read_bytes=0 read_done=0 read_errors=0 writes=1 write_bytes=8 write_done=1 "
             "write_errors=0 write_lo=0x88000000 write_hi=0x88000007 outstanding=0\n";
        if (shim) o << "[fx1_soc] camera fault (injection): csi dma " << shim->report() << '\n';
    }
};

struct Bench : sc_core::sc_module {
    std::string dir;
    std::vector<std::unique_ptr<Chain>> chains;
    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n, std::string d) : sc_module(n), dir(std::move(d))
    {
        const std::vector<std::pair<std::uint64_t, unsigned>> three = {{kBase, 8}, {kBase + 0x80, 8}, {kSide1, 8}};
        chains.push_back(std::make_unique<Chain>("fault", true, false, three));
        chains.push_back(std::make_unique<Chain>("lost", true, true, three));
        chains.push_back(std::make_unique<Chain>("clean", false, false, three));
        chains.push_back(std::make_unique<Chain>(
            "gap", false, false, std::vector<std::pair<std::uint64_t, unsigned>>{{kBase, 8}, {kBase + 0x8000, 8}, {kSide1, 8}}));
        chains.push_back(std::make_unique<Chain>("cross", false, false,
                                                 std::vector<std::pair<std::uint64_t, unsigned>>{{kBase + 0xFF8, 16}}));
        SC_THREAD(run);
    }
    Chain& c(const char* n)
    {
        for (auto& ch : chains)
            if (ch->name == n) return *ch;
        throw std::runtime_error(n);
    }
    void run()
    {
        for (auto& ch : chains) ch->run();
        // F1: counter before the shim: the injected write is issued traffic with an error.
        auto& f = c("fault");
        CHECK(f.ini->errors == 1, "fault: the initiator got one error");
        CHECK(f.cnt->writes().txns == 3 && f.cnt->writes().done == 3 && f.cnt->writes().errors == 1,
              "fault: counter 3 issued / 3 returned / 1 error");
        CHECK(f.cnt->outstanding() == 0 && f.cnt->outside() == 0, "fault: nothing outstanding or outside");
        CHECK(f.shim->report().find(", 3 MIPI_DMA write(s) seen, fired at #2 address 0x81000080") != std::string::npos,
              "fault: shim saw 3, fired at #2: " + f.shim->report());
        CHECK(f.ram->mem[0x80] == 0xA5 && f.ram->mem[0] == 0x5A, "fault: the failed write reached no memory");
        // The refused topology: the counter misses the injected write (control for the checker).
        auto& l = c("lost");
        CHECK(l.ini->errors == 1 && l.cnt->writes().txns == 2 && l.cnt->writes().errors == 0,
              "lost: counter after the shim sees 2 writes, 0 errors");
        // F2: per-transaction windows.
        CHECK(c("clean").cnt->outside() == 0 && c("clean").cnt->writes().txns == 3, "clean: outside 0");
        auto& g = c("gap");
        CHECK(g.cnt->outside() == 1, "gap: the write between the sides is outside");
        CHECK(g.cnt->writes().lo == kBase && g.cnt->writes().hi == kSide1 + 7, "gap: global lo/hi as clean");
        CHECK(g.ram->mem[0x8000] == 0x5A, "gap: the write was forwarded (the counter only reports)");
        CHECK(c("cross").cnt->outside() == 1, "cross: a burst past the end of side 0 is outside");
        CHECK(c("clean").cnt->report("MIPI_DMA").find(" windows=0x81000000-0x81000fff,0x81010000-0x81010fff outside=0") !=
                  std::string::npos,
              "report lists the windows: " + c("clean").cnt->report("MIPI_DMA"));
        for (auto& ch : chains) ch->dump(dir);
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: test_fx1_csi_dma_traffic <dir>\n";
        return 2;
    }
    Bench b("bench", argv[1]);
    sc_core::sc_start();
    std::cout << (failures ? "FAIL" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
