// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.tests.test_scenarios

// Changelog:
//   2026-09-29: Initial version.
//   2026-10-19: REGW: a write while running, response checked.
// =============================================================================
//
// Usage: test_scenarios DIR [first [count]]
// Replay bundled scenarios on a fresh MAC and compare frames, descriptors,
// receive buffers, counters, register values and memory transaction shapes
// with the expected results. See vectors/README.md for record formats.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "eth_regs.h"
#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"
#include "vp/bus_router.h"
#include "vp/memory_tlm.h"

using namespace sc_core;
using Bytes = std::vector<uint8_t>;

namespace {

constexpr uint64_t kDdrSize = 0x100'0000;
constexpr uint32_t kTxRing = 0x1000, kRxRing = 0x3000, kTxBuf = 0x10000, kRxBuf = 0x100000;
constexpr uint32_t kStride = 0x2000, kUnmapped = 0x0800'0000;

Bytes unhex(const std::string& s) {
    Bytes b;
    if (s == "-") return b;
    for (size_t i = 0; i + 1 < s.size(); i += 2) b.push_back(uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
    return b;
}
std::vector<unsigned> list(const std::string& s) {
    std::vector<unsigned> v;
    std::stringstream in(s);
    std::string t;
    while (std::getline(in, t, ',')) v.push_back(unsigned(std::stoul(t)));
    return v;
}

struct Scenario {
    std::string name;
    unsigned ntx = 8, nrx = 8, rx_own = 0;
    std::vector<unsigned> rx_offs;
    std::vector<std::pair<uint32_t, uint32_t>> regs;
    struct RegW {
        uint32_t off, val;
        bool err;
    };
    std::vector<RegW> regw;
    struct Tx {
        Bytes f;
        std::vector<unsigned> segs, leads;
        std::string flag;
    };
    std::vector<Tx> tx;
    struct Rx {
        Bytes w;
        bool rx_er, dribble;
    };
    std::vector<Rx> rx;
    std::vector<Bytes> wire;
    std::vector<std::pair<unsigned, uint32_t>> txd;
    struct Rxd {
        unsigned idx;
        uint32_t des0;
        Bytes data;
    };
    std::vector<Rxd> rxd;
    std::vector<std::pair<uint32_t, uint32_t>> rmon;
    struct Reg {
        uint32_t off, mask, val;
    };
    std::vector<Reg> exp;
};

bool load(const std::string& path, Scenario& s) {
    std::ifstream f(path);
    if (!f) return false;
    s.name = path;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "RING") {
            std::string offs;
            in >> s.ntx >> s.nrx >> s.rx_own >> offs;
            s.rx_offs = list(offs);
        } else if (k == "REG") {
            std::string o, v;
            in >> o >> v;
            s.regs.emplace_back(std::stoul(o, nullptr, 16), std::stoul(v, nullptr, 16));
        } else if (k == "REGW") {
            std::string o, v;
            int e;
            in >> o >> v >> e;
            s.regw.push_back({uint32_t(std::stoul(o, nullptr, 16)), uint32_t(std::stoul(v, nullptr, 16)), e != 0});
        } else if (k == "TX") {
            std::string fr, sg, ld, fl;
            in >> fr >> sg >> ld >> fl;
            s.tx.push_back({unhex(fr), list(sg), list(ld), fl});
        } else if (k == "RX") {
            std::string w;
            int e, d;
            in >> w >> e >> d;
            s.rx.push_back({unhex(w), e != 0, d != 0});
        } else if (k == "WIRE") {
            std::string w;
            in >> w;
            s.wire.push_back(unhex(w));
        } else if (k == "TXD") {
            unsigned i;
            std::string v;
            in >> i >> v;
            s.txd.emplace_back(i, std::stoul(v, nullptr, 16));
        } else if (k == "RXD") {
            unsigned i;
            std::string v, d;
            in >> i >> v >> d;
            s.rxd.push_back({i, uint32_t(std::stoul(v, nullptr, 16)), unhex(d)});
        } else if (k == "RMON") {
            std::string o, v;
            in >> o >> v;
            s.rmon.emplace_back(std::stoul(o, nullptr, 16), std::stoul(v, nullptr, 16));
        } else if (k == "EXPREG") {
            std::string o, m, v;
            in >> o >> m >> v;
            s.exp.push_back({uint32_t(std::stoul(o, nullptr, 16)), uint32_t(std::stoul(m, nullptr, 16)),
                             uint32_t(std::stoul(v, nullptr, 16))});
        }
    }
    return true;
}

// issues: bus-aligned address, whole beats, at most
// MAX_BURST beats (a descriptor fetch is one 16-byte burst), no 4 KB crossing,
// and strobes on every write.
struct AxiChecker : sc_module {
    tlm_utils::simple_target_socket<AxiChecker> in;
    tlm_utils::simple_initiator_socket<AxiChecker> out;
    unsigned B = 4, N = 16;
    int violations = 0;
    uint64_t bursts = 0;
    explicit AxiChecker(sc_module_name n) : sc_module(n), in("in"), out("out") {
        in.register_b_transport(this, &AxiChecker::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& t, sc_time& d) {
        const uint64_t a = t.get_address();
        const unsigned len = t.get_data_length();
        ++bursts;
        bool ok = a % B == 0 && len % B == 0 && len / B <= std::max(N, 16u / B) && (a % 4096) + len <= 4096;
        if (t.is_write()) ok = ok && t.get_byte_enable_ptr() && t.get_byte_enable_length() == len;
        if (!ok && ++violations <= 5)
            std::printf("  [AXI] %s addr %llX len %u\n", t.is_write() ? "write" : "read", (unsigned long long)a, len);
        out->b_transport(t, d);
    }
};

struct Tb : sc_module {
    tlm_utils::simple_initiator_socket<Tb> sock;
    tlm_utils::simple_target_socket<Tb> no_phy;         // MDIO: nobody answers
    sc_signal<bool> rst_n{"rst_n", false};
    sc_signal<bool> irq{"irq"};
    vp::MemoryTlm* ddr = nullptr;
    icsl_eth::EthLinkPartner* peer = nullptr;
    AxiChecker* axi = nullptr;
    std::vector<std::string> files;
    int fails = 0, checks = 0, bad_scenarios = 0;

    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_module_name n) : sc_module(n), sock("sock"), no_phy("no_phy") {
        no_phy.register_b_transport(this, &Tb::mdio_none);
        SC_THREAD(run);
    }
    void mdio_none(tlm::tlm_generic_payload& t, sc_time&) { t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); }

    uint32_t xfer(tlm::tlm_command cmd, uint32_t off, uint32_t v, bool* err = nullptr) {
        tlm::tlm_generic_payload t;
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        t.set_command(cmd);
        t.set_address(off);
        t.set_data_ptr(b);
        t.set_data_length(4);
        t.set_streaming_width(4);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = SC_ZERO_TIME;
        sock->b_transport(t, d);
        wait(d);
        if (err) *err = t.is_response_error();
        std::memcpy(&v, b, 4);
        return v;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }
    uint32_t m32(uint32_t a) {
        uint32_t v;
        ddr->read(a, &v, 4);
        return v;
    }
    void w32(uint32_t a, uint32_t v) { ddr->write(a, &v, 4); }

    int miss = 0;
    void expect(bool ok, const Scenario& s, const std::string& what) {
        ++checks;
        if (ok) return;
        ++fails;
        if (++miss <= 6) std::printf("  [FAIL] %s: %s\n", s.name.c_str(), what.c_str());
    }

    void replay(const Scenario& s) {
        miss = 0;
        rst_n.write(false);
        wait(50, SC_NS);
        rst_n.write(true);
        wait(50, SC_NS);
        peer->received().clear();
        const Bytes zero(kRxBuf + kStride * 64, 0);
        ddr->write(0, zero.data(), zero.size());
        for (unsigned i = 0; i < s.nrx; ++i) {
            w32(kRxRing + 16 * i + 8, kRxBuf + kStride * i + s.rx_offs[i]);
            w32(kRxRing + 16 * i, i < s.rx_own ? 0x80000000u : 0);
        }
        for (const auto& r : s.regs) wr(r.first, r.second);
        wr(ETH_DMA_TX_DESC_BASE, kTxRing);
        wr(ETH_DMA_TX_DESC_LEN, s.ntx);
        wr(ETH_DMA_RX_DESC_BASE, kRxRing);
        wr(ETH_DMA_RX_DESC_LEN, s.nrx);
        wr(ETH_RMII_CFG, 1);
        wr(ETH_CTRL, 0x3FFF);
        wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
        wr(ETH_RX_MAC_CFG, 1);
        wr(ETH_TX_MAC_CFG, 1);
        wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
        // writes while running: a closed window answers with an error
        for (const auto& w : s.regw) {
            bool err = false;
            xfer(tlm::TLM_WRITE_COMMAND, w.off, w.val, &err);
            char b[96];
            std::snprintf(b, sizeof b, "write %04X while running: %s want %s", w.off, err ? "error" : "OKAY",
                          w.err ? "error" : "OKAY");
            expect(err == w.err, s, b);
        }
        // the transmit frames, OWN in reverse order within each frame
        unsigned d = 0;
        for (const auto& t : s.tx) {
            size_t pos = 0;
            std::vector<unsigned> idx;
            for (size_t k = 0; k < t.segs.size(); ++k) {
                const uint32_t buf = t.flag == "DECERR" ? kUnmapped : kTxBuf + kStride * d + t.leads[k];
                if (t.flag != "DECERR" && t.segs[k]) ddr->write(buf, t.f.data() + pos, t.segs[k]);
                const bool fs = k == 0 && t.flag != "NOFS", ls = k + 1 == t.segs.size();
                w32(kTxRing + 16 * d + 4, (ls ? 1u << 31 : 0) | (fs ? 1u << 30 : 0) | t.segs[k]);
                w32(kTxRing + 16 * d + 8, buf);
                idx.push_back(d++);
                pos += t.segs[k];
            }
            for (auto it = idx.rbegin(); it != idx.rend(); ++it) w32(kTxRing + 16 * *it, 0x80000000u);
        }
        wr(ETH_DMA_TX_POLL, 1);
        // the receive frames at line rate, a minimum gap apart
        icsl_eth::EthFrameExt x;
        for (const auto& r : s.rx) {
            x.rx_er = r.rx_er;
            x.dribble = r.dribble;
            peer->send(r.w, x);
            wait(icsl_eth::byte_time(true) * 12.0);
        }
        // quiescence: transmit parked (SUSPEND or HALTED) with the line idle
        for (int k = 0; k < 2000; ++k) {
            const uint32_t st = rd(ETH_DMA_TX_STAT);
            if (((st & 0x8) || (st & 0x2)) && !(rd(ETH_STATUS) & ETH_STATUS_TX_BUSY_MASK)) break;
            wait(5, SC_US);
        }
        wait(30, SC_US);

        // ---- compare ---------------------------------------------------------------
        const auto& got = peer->received();
        expect(got.size() == s.wire.size(), s,
               "frames on the line: " + std::to_string(got.size()) + " want " + std::to_string(s.wire.size()));
        for (size_t i = 0; i < std::min(got.size(), s.wire.size()); ++i)
            expect(got[i].bytes == s.wire[i], s, "line frame " + std::to_string(i));
        for (const auto& t : s.txd) {
            const uint32_t v = m32(kTxRing + 16 * t.first);
            char b[96];
            std::snprintf(b, sizeof b, "TX DES0[%u] = %08X want %08X", t.first, v, t.second);
            expect(v == t.second, s, b);
        }
        for (const auto& r : s.rxd) {
            const uint32_t v = m32(kRxRing + 16 * r.idx);
            char b[96];
            std::snprintf(b, sizeof b, "RX DES0[%u] = %08X want %08X", r.idx, v, r.des0);
            expect(v == r.des0, s, b);
            if (!r.data.empty()) {
                Bytes m(r.data.size());
                ddr->read(kRxBuf + kStride * r.idx + s.rx_offs[r.idx], m.data(), m.size());
                expect(m == r.data, s, "RX buffer " + std::to_string(r.idx));
            }
        }
        for (const auto& m : s.rmon) {
            const uint32_t v = rd(m.first);
            char b[96];
            std::snprintf(b, sizeof b, "RMON %04X = %u want %u", m.first, v, m.second);
            expect(v == m.second, s, b);
        }
        for (const auto& e : s.exp) {
            const uint32_t v = rd(e.off) & e.mask;
            char b[96];
            std::snprintf(b, sizeof b, "reg %04X & %X = %X want %X", e.off, e.mask, v, e.val);
            expect(v == e.val, s, b);
        }
        if (miss) ++bad_scenarios;
    }

    void run() {
        for (const auto& f : files) {
            Scenario s;
            if (!load(f, s)) {
                std::printf("  cannot read %s\n", f.c_str());
                ++fails;
                continue;
            }
            replay(s);
        }
        ++checks;
        if (axi->violations) ++fails;
        std::printf("AXI bursts %llu, shape violations %d\n", (unsigned long long)axi->bursts, axi->violations);
        std::printf("=== scenarios: %zu scenarios, %d checks, %d failures (%d scenarios) ===\n", files.size(),
                    checks, fails, bad_scenarios);
        sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s DIR [first [count]]\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    unsigned count = 0;
    {
        std::ifstream c(dir + "/count.txt");
        c >> count;
    }
    const unsigned first = argc > 2 ? unsigned(std::stoul(argv[2])) : 0;
    const unsigned n = argc > 3 ? unsigned(std::stoul(argv[3])) : count - std::min(first, count);
    Tb tb("tb");
    vp::BusRouter bus("bus", 1, {{"ddr", 0, kDdrSize}});
    vp::MemoryTlm ddr("ddr", kDdrSize, sc_time(50, SC_NS), sc_time(10, SC_NS), 4);
    icsl_eth::EthMacTlm mac("mac");
    icsl_eth::EthLinkPartner peer("peer");
    AxiChecker axi("axi_checker");
    tb.sock.bind(mac.csr);
    mac.axi.bind(axi.in);
    axi.out.bind(bus.initiator_port(0));
    tb.axi = &axi;
    bus.target_port("ddr").bind(ddr.socket);
    mac.phy_tx.bind(peer.line_rx);
    peer.line_tx.bind(mac.phy_rx);
    mac.mdio.bind(tb.no_phy);
    mac.rst_n(tb.rst_n);
    mac.rmii_rst_n(tb.rst_n);
    mac.irq(tb.irq);
    tb.ddr = &ddr;
    tb.peer = &peer;
    char name[32];
    for (unsigned i = first; i < first + n && i < count; ++i) {
        std::snprintf(name, sizeof name, "/s%04u.txt", i);
        tb.files.push_back(dir + name);
    }
    std::printf("=== scenarios: %s, %zu files ===\n", dir.c_str(), tb.files.size());
    sc_start();
    return tb.fails == 0 ? 0 : 1;
}
