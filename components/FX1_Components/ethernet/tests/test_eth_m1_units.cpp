// =============================================================================
// Module: virtual_platform.eth_mac_tlm.tests.test_eth_m1_units
// Description: Same-edge rules of CsrFile and core::Rmon on their own, with a
//              test-driven edge stamp, in every call order (plan M1, F07).
// Changelog:
//   2026-11-02: Initial version (review MR01 and C01).
// =============================================================================
//
//   csr-edge   CsrFile: hardware set/store/add against W1C/WC/RW on one edge,
//              and hw_retract() of a set the edge takes back (review MR01).
//   rmon-wrap  core::Rmon: an octet counter wrapping on the edge of an
//              overflow clear, a CLEAR_ALL or a read-clear (MAS Table 6-37).
//
// The MAC-level collisions are in test_eth_mac_m1 (collisions, coal-collision).
#include <cstdio>
#include <string>

#include <systemc>

#include "eth_regs.h"
#include "icsl_eth/csr_file.h"
#include "icsl_eth/eth_core.h"

namespace {

int g_fail = 0, g_pass = 0;

void check(bool ok, const std::string& what) {
    ++(ok ? g_pass : g_fail);
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

uint64_t g_edge = 1;

// -----------------------------------------------------------------------------
// One edge of a coalescing collision on ETH_DMA_RX_STAT.DONE, in a given order.
//   prior     DONE before the edge (an unacknowledged earlier report)
//   w1c       where the W1C of DONE lands: 0 none, 1 before the expiry's set,
//             2 between the set and the retract, 3 after the retract (and
//             after the completion's own set, if any)
//   reached   the completion reaches COAL_CNT and sets DONE again
// Table 9-3: DONE at the end = reached || (prior && no W1C).
bool coal_edge(bool prior, int w1c, bool reached) {
    icsl_eth::CsrFile r;
    r.now = [] { return g_edge; };
    const uint32_t off = ETH_DMA_RX_STAT, done = ETH_DMA_RX_STAT_DONE_MASK;
    ++g_edge;
    if (prior) r.hw_set(off, done);
    ++g_edge;                                                // the collision edge
    if (w1c == 1) r.write(off, done, 0xFFFFFFFF);
    r.hw_set(off, done);                                     // timer expiry
    if (w1c == 2) r.write(off, done, 0xFFFFFFFF);
    r.hw_retract(off, done);                                 // completion cancels it
    if (reached) r.hw_set(off, done);                        // ... and reports
    if (w1c == 3) r.write(off, done, 0xFFFFFFFF);
    const bool end = r.value(off) & done;
    // the next edge: a W1C clears whatever is left
    ++g_edge;
    r.write(off, done, 0xFFFFFFFF);
    return end == (reached || (prior && w1c == 0)) && !(r.value(off) & done);
}

void test_csr_edge() {
    const char* where[] = {"no W1C", "W1C before the expiry", "W1C between expiry and completion",
                           "W1C after the completion"};
    for (bool prior : {false, true})
        for (int w = 0; w < 4; ++w)
            for (bool reached : {false, true})
                check(coal_edge(prior, w, reached), std::string("MR01 retract: old DONE ") + (prior ? "1" : "0") +
                                                        ", " + where[w] + (reached ? ", COAL_CNT reached" : ", below COAL_CNT"));

    icsl_eth::CsrFile r;
    r.now = [] { return g_edge; };
    // sticky set + W1C, both orders, and the next edge
    ++g_edge;
    r.hw_set(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    r.write(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK, 0xFFFFFFFF);
    const bool kept = r.value(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_DONE_MASK;
    ++g_edge;
    r.write(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK, 0xFFFFFFFF);
    r.hw_set(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    const bool reset_then_set = r.value(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_DONE_MASK;
    ++g_edge;
    r.write(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK, 0xFFFFFFFF);
    check(kept && reset_then_set && !(r.value(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_DONE_MASK),
          "set + W1C on one edge keeps the bit in both orders; a later W1C clears it");
    // WC + increment: restarts at the edge's increment, both orders
    ++g_edge;
    r.hw_add(ETH_DMA_RX_DROP_CNT, 5);
    ++g_edge;
    r.hw_add(ETH_DMA_RX_DROP_CNT, 1);
    r.write(ETH_DMA_RX_DROP_CNT, 0, 0xFF);
    const uint32_t a = r.value(ETH_DMA_RX_DROP_CNT);
    ++g_edge;
    r.write(ETH_DMA_RX_DROP_CNT, 0, 0xFF);
    r.hw_add(ETH_DMA_RX_DROP_CNT, 1);
    check(a == 1 && r.value(ETH_DMA_RX_DROP_CNT) == 1, "WC + increment on one edge leaves 1 in both orders");
    // RW + hardware store: the hardware value, both orders; software wins later
    ++g_edge;
    r.hw_store(ETH_MDIO_DATA, 0xFFFF, 0x0007);
    r.write(ETH_MDIO_DATA, 0xBEEF, 0xFFFFFFFF);
    const uint32_t h1 = r.value(ETH_MDIO_DATA);
    ++g_edge;
    r.write(ETH_MDIO_DATA, 0xBEEF, 0xFFFFFFFF);
    r.hw_store(ETH_MDIO_DATA, 0xFFFF, 0x0007);
    const uint32_t h2 = r.value(ETH_MDIO_DATA);
    ++g_edge;
    r.write(ETH_MDIO_DATA, 0xBEEF, 0xFFFFFFFF);
    check(h1 == 7 && h2 == 7 && r.value(ETH_MDIO_DATA) == 0xBEEF, "RW + hardware store: hardware wins on its edge only");
    // without an edge stamp nothing is protected (v1.0.6 behaviour)
    icsl_eth::CsrFile plain;
    plain.hw_set(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK);
    plain.write(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK, 0xFFFFFFFF);
    check(!(plain.value(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK), "no edge stamp: a W1C always clears");
}

// -----------------------------------------------------------------------------
// TX_OCTETS adds the frame length per event: 1518-byte frames wrap it after
// ceil(2^32 / 1518) = 2,829,423 events.
namespace core = icsl_eth::core;
constexpr uint32_t kTxOvfBit = 1u << (core::TX_OCTETS - core::kRmonRx);   // TX_OCTETS in ovf_tx()

core::Rmon near_wrap() {
    core::Rmon m;
    m.now = [] { return g_edge; };
    m.en = true;
    while (m.peek(core::TX_OCTETS) <= 0xFFFFFFFFu - 1518u) {
        ++g_edge;
        m.tx_frame(1518, false, false);
    }
    ++g_edge;                                                    // the next event wraps
    return m;
}

void test_rmon_wrap() {
    const core::Rmon base = near_wrap();
    const uint32_t before = base.peek(core::TX_OCTETS);
    const uint32_t pkts = base.peek(core::TX_PKTS);
    check(uint64_t(before) + 1518 > 0xFFFFFFFFull && (base.ovf_tx() & kTxOvfBit) == 0,
          "TX_OCTETS one 1518-byte event short of its wrap after " + std::to_string(pkts) + " events");
    const uint32_t wrapped = uint32_t(uint64_t(before) + 1518);
    {
        core::Rmon m = base;                                     // wrap, then W1C on the edge
        m.tx_frame(1518, false, false);
        m.clear_ovf(0, kTxOvfBit);
        const bool kept = m.ovf_tx() & kTxOvfBit;
        ++g_edge;
        m.clear_ovf(0, kTxOvfBit);
        check(kept && !(m.ovf_tx() & kTxOvfBit) && m.peek(core::TX_OCTETS) == wrapped,
              "wrap + W1C of its flag on one edge: the flag stays 1; a later W1C clears it");
    }
    {
        core::Rmon m = base;                                     // W1C, then wrap on the edge
        m.clear_ovf(0, kTxOvfBit);
        m.tx_frame(1518, false, false);
        check((m.ovf_tx() & kTxOvfBit) != 0, "W1C then wrap on one edge: the flag is 1");
    }
    {
        core::Rmon m = base;                                     // wrap + CLEAR_ALL, both orders
        m.tx_frame(1518, false, false);
        m.clear_all();
        core::Rmon n = base;
        n.clear_all();
        n.tx_frame(1518, false, false);
        check(m.peek(core::TX_OCTETS) == 1518 && m.peek(core::TX_PKTS) == 1 && m.ovf_tx() == 0 &&
                  n.peek(core::TX_OCTETS) == 1518 && n.peek(core::TX_PKTS) == 1 && n.ovf_tx() == 0,
              "wrap + CLEAR_ALL on one edge: counters restart at the edge's increment, flags 0, both orders");
    }
    {
        core::Rmon m = base;                                     // wrap + read-clear, both orders
        m.tx_frame(1518, false, false);
        const uint32_t v1 = m.read(core::TX_OCTETS, true);
        core::Rmon n = base;
        const uint32_t v2 = n.read(core::TX_OCTETS, true);
        n.tx_frame(1518, false, false);
        const bool m_ok = v1 == before && m.peek(core::TX_OCTETS) == 1518 && !(m.ovf_tx() & kTxOvfBit);
        // read first: nothing wrapped yet when the counter restarts, so the
        // edge's event only adds 1518 and sets no flag
        const bool n_ok = v2 == before && n.peek(core::TX_OCTETS) == 1518 && !(n.ovf_tx() & kTxOvfBit);
        check(m_ok && n_ok, "wrap + read-clear on one edge: returns the value before the edge, counter holds the "
                            "edge's increment, no flag, both orders");
    }
}

}  // namespace

int sc_main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "csr-edge") test_csr_edge();
    else if (mode == "rmon-wrap") test_rmon_wrap();
    else {
        std::printf("usage: %s csr-edge|rmon-wrap\n", argv[0]);
        return 2;
    }
    std::printf("=== eth_m1 %s: %d checks, %d failures ===\n", mode.c_str(), g_pass + g_fail, g_fail);
    return g_fail == 0 ? 0 : 1;
}
