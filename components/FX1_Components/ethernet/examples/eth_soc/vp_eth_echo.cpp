// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.platforms.eth_soc.vp_eth_echo
// Description: Two nodes on one wire, both running the bare-metal driver.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
//   vp_eth_echo [--frames N] [--burst N] [--10m] [--trace] [--sim-ms N]
//
//   node A (client) --PHY-- wire --PHY-- node B (echo server)
//
// Echo (default): A reads its PHY over MDIO, then sends N UDP frames of
// lengths 60..1514 to B; B returns each with the addresses and ports swapped
// and both checksum fields zeroed, so B's transmit offload must insert them
// and A's receive offload must verify them (DES0 verdict 0x2A).  A checks
// every payload, the verdicts, the order and both nodes' RMON counters.
// Marker: "ECHO PASS".
//
// Burst (--burst N): A sends N minimum-size frames as fast as its ring

// 100 Mbps with no loss (RMON_RX_DROP_EVENTS and ETH_DMA_RX_DROP_CNT stay 0).
// Marker: "BURST PASS".
//
// Both programs are interrupt-driven: they sleep on the IP's level interrupt

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <systemc>

#include "eth_drv.h"
#include "eth_node.h"
#include "eth_regs.h"

using namespace sc_core;
using eth_soc::EthNode;

namespace {

const uint8_t kMacA[6] = {0x02, 0x00, 0x5E, 0x00, 0x00, 0x0A};
const uint8_t kMacB[6] = {0x02, 0x00, 0x5E, 0x00, 0x00, 0x0B};

struct Opts {
    unsigned frames = 64, burst = 0;
    bool speed100 = true, trace = false;
    double sim_ms = 2000;
};

eth_cfg_t node_cfg(const uint8_t* mac, bool speed100, uint32_t ring) {
    eth_cfg_t c;
    std::memset(&c, 0, sizeof c);
    std::memcpy(c.mac, mac, 6);
    c.speed100 = speed100;
    c.phy_addr = 1;
    c.tx_ring = 0x0001'0000;
    c.rx_ring = 0x0002'0000;
    c.tx_buf = 0x0010'0000;
    c.rx_buf = 0x0020'0000;
    c.ring_len = ring;
    c.buf_stride = 0x800;
    c.rx_lead = 2;
    c.coal = 0;
    c.int_en = ETH_INT_RAW_TX_DONE_MASK | ETH_INT_RAW_RX_DONE_MASK | ETH_INT_RAW_TX_ERR_MASK |
               ETH_INT_RAW_RX_ERR_MASK;
    c.filt_ctrl = ETH_FILT_CTRL_BCAST_ACCEPT_MASK;
    return c;
}

// IPv4/UDP, checksum fields zero: the IP inserts them
std::vector<uint8_t> udp_frame(const uint8_t* da, const uint8_t* sa, uint32_t seq, unsigned payload) {
    std::vector<uint8_t> f(da, da + 6);
    f.insert(f.end(), sa, sa + 6);
    const unsigned tot = 20 + 8 + payload;
    const uint8_t ip[20] = {0x45, 0, uint8_t(tot >> 8), uint8_t(tot), uint8_t(seq >> 8), uint8_t(seq),
                            0x40, 0, 64, 17, 0, 0, 192, 168, 1, 10, 192, 168, 1, 11};
    f.push_back(0x08);
    f.push_back(0x00);
    f.insert(f.end(), ip, ip + 20);
    const unsigned ul = 8 + payload;
    const uint8_t udp[8] = {0x30, 0x39, 0x00, 0x07, uint8_t(ul >> 8), uint8_t(ul), 0, 0};   // 12345 -> echo
    f.insert(f.end(), udp, udp + 8);
    for (unsigned i = 0; i < payload; ++i) f.push_back(uint8_t(seq * 31 + i * 7 + (i >> 8)));
    if (payload >= 4) std::memcpy(&f[42], &seq, 4);
    return f;
}

unsigned payload_of(unsigned k) {
    static const unsigned sizes[] = {18, 19, 46, 47, 100, 255, 256, 511, 512, 1000, 1023, 1024, 1472};
    return sizes[k % (sizeof sizes / sizeof sizes[0])];
}

struct Echo : sc_module {
    EthNode& a;
    EthNode& b;
    Opts o;
    bool b_stop = false;
    int result = 1;
    unsigned b_count = 0;

    SC_HAS_PROCESS(Echo);
    Echo(sc_module_name n, EthNode& a_, EthNode& b_, const Opts& o_) : sc_module(n), a(a_), b(b_), o(o_) {
        SC_THREAD(client);
        SC_THREAD(server);
    }

    bool fail(const char* what) {
        std::printf("FAIL: %s @ %s\n", what, sc_time_stamp().to_string().c_str());
        return false;
    }

    // node B: echo (or count, in burst mode) every frame
    void server() {
        wait(1, SC_US);
        eth_dev_t d;
        eth_hal_t h = b.hal();
        const eth_cfg_t cfg = node_cfg(kMacB, o.speed100, 64);
        if (eth_init(&d, &h, &cfg) != ETH_OK) {
            std::printf("FAIL: node B init\n");
            return;
        }
        std::vector<uint8_t> buf(1600);
        while (!b_stop) {
            h.wait_irq(h.ctx, 200);
            eth_irq_ack(&d);
            uint32_t des0;
            int n;
            while ((n = eth_recv(&d, buf.data(), uint32_t(buf.size()), &des0)) > 0) {
                ++b_count;
                if (o.burst) continue;
                std::vector<uint8_t> e(buf.begin(), buf.begin() + n);
                for (int k = 0; k < 6; ++k) std::swap(e[k], e[6 + k]);
                for (int k = 0; k < 4; ++k) std::swap(e[26 + k], e[30 + k]);     // IPv4 src <-> dst
                for (int k = 0; k < 2; ++k) std::swap(e[34 + k], e[36 + k]);     // ports
                e[24] = e[25] = 0;                                                // IPv4 checksum
                e[40] = e[41] = 0;                                                // UDP checksum
                while (eth_send(&d, e.data(), uint32_t(e.size())) == ETH_E_FULL) wait(1, SC_US);
            }
            eth_tx_reclaim(&d);
        }
    }

    bool run_echo(eth_dev_t& d, eth_hal_t& h) {
        std::vector<uint8_t> buf(1600);
        unsigned sent = 0, got = 0;
        const sc_time t0 = sc_time_stamp();
        while (got < o.frames) {
            // keep up to 16 frames in flight
            while (sent < o.frames && sent - got < 16) {
                const auto f = udp_frame(kMacB, kMacA, sent, payload_of(sent));
                if (eth_send(&d, f.data(), uint32_t(f.size())) != ETH_OK) break;
                ++sent;
            }
            if (!h.wait_irq(h.ctx, 5000)) return fail("no interrupt in 5 ms");
            const uint32_t st = eth_irq_ack(&d);
            if (st & (ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK)) return fail("DMA error interrupt");
            eth_tx_reclaim(&d);
            uint32_t des0;
            int n;
            while ((n = eth_recv(&d, buf.data(), uint32_t(buf.size()), &des0)) > 0) {
                const auto exp = udp_frame(kMacA, kMacB, got, payload_of(got));
                // the echo: same payload, addresses swapped
                if (unsigned(n) != exp.size()) return fail("echo length");
                if (std::memcmp(buf.data(), kMacA, 6) || std::memcmp(buf.data() + 6, kMacB, 6))
                    return fail("echo MAC addresses");
                if (std::memcmp(buf.data() + 42, exp.data() + 42, n - 42)) return fail("echo payload / order");
                if (((des0 >> 20) & 0x3F) != 0x2A) {
                    std::printf("  frame %u DES0 %08X\n", got, des0);
                    return fail("checksum verdict (node B's insertion, node A's check)");
                }
                ++got;
            }
        }
        const double us = (sc_time_stamp() - t0).to_seconds() * 1e6;
        std::printf("echo: %u frames of 60..1514 bytes round trip in %.1f us simulated\n", got, us);
        return true;
    }

    bool run_burst(eth_dev_t& d, eth_hal_t& h) {
        const auto f = udp_frame(kMacB, kMacA, 0, 18);          // 60 bytes + FCS = 64 on the wire
        const sc_time t0 = sc_time_stamp();
        unsigned sent = 0;
        while (sent < o.burst) {
            if (eth_send(&d, f.data(), uint32_t(f.size())) == ETH_OK) {
                ++sent;
            } else {
                wait(500, SC_NS);
            }
        }
        // wait for the line and node B
        const sc_time end = sc_time_stamp() + sc_time(2, SC_MS);
        while (b_count < o.burst && sc_time_stamp() < end) wait(10, SC_US);
        const double sec = (sc_time_stamp() - t0).to_seconds();
        const uint32_t rx = b.reg_rd(RMON_RX_PKTS), drops = b.reg_rd(RMON_RX_DROP_EVENTS);
        const uint32_t dcnt = b.reg_rd(ETH_DMA_RX_DROP_CNT);
        std::printf("burst: %u sent, node B received %u (RMON %u), drop events %u, no-descriptor drops %u\n",
                    sent, b_count, rx, drops, dcnt);
        std::printf("burst: %.1f kframes/s over %.1f us (line rate 148.8)\n", b_count / sec / 1e3, sec * 1e6);
        if (b_count != o.burst || rx != o.burst || drops || dcnt) return fail("line-rate burst lost frames");
        return true;
    }

    // node A: the test
    void client() {
        wait(1, SC_US);
        eth_dev_t d;
        eth_hal_t h = a.hal();
        a.trace = o.trace;
        bool ok = true;
        const eth_cfg_t cfg = node_cfg(kMacA, o.speed100, 64);
        if (eth_init(&d, &h, &cfg) != ETH_OK) ok = fail("node A init");
        uint16_t id1 = 0, id2 = 0, bmsr = 0;
        if (ok && (eth_mdio_read(&d, 1, 2, &id1) || eth_mdio_read(&d, 1, 3, &id2) ||
                   eth_mdio_read(&d, 1, 1, &bmsr)))
            ok = fail("MDIO");
        if (ok) std::printf("node A: PHY %04X:%04X, BMSR %04X (link %s), %s\n", id1, id2, bmsr,
                            (bmsr & 4) ? "up" : "down", o.speed100 ? "100 Mbps" : "10 Mbps");
        if (ok && !(bmsr & 4)) ok = fail("PHY link down");
        uint16_t none;
        if (ok && eth_mdio_read(&d, 7, 2, &none) != ETH_E_MDIO) ok = fail("MDIO NO_RESPONSE at an empty address");
        wait(20, SC_US);                                        // node B up
        if (ok) ok = o.burst ? run_burst(d, h) : run_echo(d, h);
        if (ok && !o.burst) {
            wait(50, SC_US);
            const uint32_t atx = a.reg_rd(RMON_TX_PKTS), arx = a.reg_rd(RMON_RX_PKTS);
            const uint32_t btx = b.reg_rd(RMON_TX_PKTS), brx = b.reg_rd(RMON_RX_PKTS);
            std::printf("RMON: A tx %u rx %u, B tx %u rx %u; A MMIO accesses %llu\n", atx, arx, btx, brx,
                        (unsigned long long)a.mmio);
            if (atx != o.frames || arx != o.frames || btx != o.frames || brx != o.frames)
                ok = fail("RMON packet counters");
            if (d.tx_errors || d.rx_errors) ok = fail("DES0 error status");
        }
        b_stop = true;
        if (ok && eth_stop(&d) != ETH_OK) ok = fail("HALT");
        std::printf("%s\n", ok ? (o.burst ? "BURST PASS" : "ECHO PASS") : (o.burst ? "BURST FAIL" : "ECHO FAIL"));
        result = ok ? 0 : 1;
        sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    Opts o;
    for (int i = 1; i < argc; ++i) {
        const std::string s = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", s.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (s == "--frames") o.frames = unsigned(std::atoi(next()));
        else if (s == "--burst") o.burst = unsigned(std::atoi(next()));
        else if (s == "--10m") o.speed100 = false;
        else if (s == "--trace") o.trace = true;
        else if (s == "--sim-ms") o.sim_ms = std::atof(next());
        else {
            std::fprintf(stderr, "usage: %s [--frames N] [--burst N] [--10m] [--trace] [--sim-ms N]\n", argv[0]);
            return 2;
        }
    }
    EthNode a("node_a"), b("node_b");
    a.phy.line_tx.bind(b.phy.line_rx);              // the wire
    b.phy.line_tx.bind(a.phy.line_rx);
    Echo echo("echo", a, b, o);
    sc_start(sc_time(o.sim_ms, SC_MS));
    if (sc_get_status() != SC_STOPPED) {
        std::printf("SIM: TIMEOUT after %.0f ms simulated\n", o.sim_ms);
        return 124;
    }
    std::printf("SIM: %s simulated, A sent %llu, B sent %llu\n", sc_time_stamp().to_string().c_str(),
                (unsigned long long)a.mac.frames_sent(), (unsigned long long)b.mac.frames_sent());
    return echo.result;
}
