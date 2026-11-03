// =============================================================================
// Module: virtual_platform.eth_mac_tlm.tests.test_eth_drv_lifecycle
// Description: The portable driver's lifecycle against the MAC model (plan M3):
//              configuration check, quiesce/restart, recovery, the ISR split,
//              write-back in flight after DONE, MDIO single owner.
// Changelog:
//   2026-11-02: Initial version (review F02, F03, F08, F09).
//   2026-11-03: Bench moved to eth_drv_fixture.h; ER03 stop accounting.
// =============================================================================
//
//   cfg        F03  rejected configurations write nothing; traffic stays inside
//                   its buffers (canaries)
//   stop       F02  quiesce idle / transmitting / stalled (forced) / receiving,
//                   repeated calls, restart without stale frames or interrupts,
//                   100 stop/restart cycles with traffic; a timeout (fake HAL)
//                   leaves the enables and the rings alone
//   recover    F02  TX and RX halting errors (DECERR), RX_TRUNC (no halt); the
//                   other direction keeps running
//   inflight   F08  DES0 write-back held 5 us after DONE: the ISR clears DONE,
//                   the descriptor still reads OWN=1, eth_*_inflight() reports
//                   it and polling (no new interrupt) completes it
//   irq-mdio   F09  eth_irq_take() masks a level error so the line falls;
//                   MDIO with a stale DONE, a frame still running, no PHY
//
// The manager on top (fw/fx1_soc/drivers/fx1_eth.c) has its own bench:
// platforms/fx1_soc/tests/test_eth_manager.cpp (review ER01-ER04).
#include <map>

#include "eth_drv_fixture.h"

namespace {

using namespace ethfix;

// =============================================================================
void test_cfg(Node& n) {
    n.reset();
    eth_dev_t d;
    const eth_hal_t h = n.hal();
    struct Bad {
        const char* what;
        std::function<void(eth_cfg_t&)> f;
    } bad[] = {
        {"stride 16 (overlapping RX buffers, probe bad-stride)", [](eth_cfg_t& c) { c.buf_stride = 16; }},
        {"stride too small for rx_lead + 1514", [](eth_cfg_t& c) { c.buf_stride = 1514; }},
        {"ring length 0", [](eth_cfg_t& c) { c.ring_len = 0; }},
        {"ring length 257", [](eth_cfg_t& c) { c.ring_len = 257; }},
        {"TX ring not 16-byte aligned", [](eth_cfg_t& c) { c.tx_ring += 8; }},
        {"RX ring overlaps TX ring", [](eth_cfg_t& c) { c.rx_ring = c.tx_ring + 16; }},
        {"RX ring inside the TX buffers", [](eth_cfg_t& c) { c.rx_ring = c.tx_buf + 0x100; }},
        {"RX buffers overlap TX buffers", [](eth_cfg_t& c) { c.rx_buf = c.tx_buf + kStride; }},
        {"RX buffers past the end of the DMA window", [](eth_cfg_t& c) { c.rx_buf = uint32_t(kDdrSize) - kStride; }},
        {"RX buffers wrap past 4 GiB", [](eth_cfg_t& c) { c.dma_size = 0; c.rx_buf = 0xFFFFF000u; }},
        {"window past 4 GiB", [](eth_cfg_t& c) { c.dma_base = 0xF0000000u; c.dma_size = 0x20000000u; }},
        {"PHY address 32", [](eth_cfg_t& c) { c.phy_addr = 32; }},
        {"INT_EN bit 4 (TX_BUF_UNAVAIL)", [](eth_cfg_t& c) { c.int_en |= ETH_INT_RAW_TX_BUF_UNAVAIL_MASK; }},
    };
    for (const auto& b : bad) {
        eth_cfg_t c = base_cfg();
        b.f(c);
        const uint64_t m0 = n.mmio, w0 = n.mem_writes;
        const int rc = eth_init(&d, &h, &c);
        check(rc == ETH_E_CFG && n.mmio == m0 && n.mem_writes == w0,
              std::string("F03 rejected without a register or memory access: ") + b.what);
    }
    eth_hal_t nh = h;
    nh.mem_rd = nullptr;
    eth_cfg_t c = base_cfg();
    check(eth_init(&d, &nh, &c) == ETH_E_CFG, "F03 a NULL HAL hook is rejected");

    // canaries around every RX buffer slot; valid traffic never touches them
    std::vector<uint8_t> can(kStride * kRing, 0xA5);
    n.ddr.write(kRxBuf, can.data(), can.size());
    check(eth_init(&d, &h, &c) == ETH_OK, "F03 the valid configuration starts");
    std::vector<uint32_t> got;
    for (uint32_t k = 0; k < 12; ++k) {
        n.inject(frame(60 + 130 * k, 0x100 + k));
        wait(icsl_eth::frame_time(60 + 130 * k + 4, true) + sc_time(20, SC_US));
        for (uint32_t t : drain_rx(d)) got.push_back(t);
    }
    bool can_ok = true;
    for (uint32_t i = 0; i < kRing; ++i) {
        uint8_t pre[kLead], post[kStride - kLead - 1514];
        n.ddr.read(kRxBuf + kStride * i, pre, sizeof pre);
        n.ddr.read(kRxBuf + kStride * i + kLead + 1514, post, sizeof post);
        for (uint8_t b : pre) can_ok = can_ok && b == 0xA5;
        for (uint8_t b : post) can_ok = can_ok && b == 0xA5;
    }
    check(got.size() == 12 && can_ok, "F03 12 frames received through an 8-entry ring; slot canaries intact");
    eth_quiesce(&d, 0, 100, nullptr);
}

// Timeout path, on a register file where HALTED never sets.
void test_stop_timeout() {
    struct Fake {
        std::map<uint32_t, uint32_t> regs;
        std::vector<uint32_t> writes;
    } f;
    eth_dev_t d;
    std::memset(&d, 0, sizeof d);
    d.hal.ctx = &f;
    d.hal.rd = [](void* c, uint32_t off) {
        Fake* x = static_cast<Fake*>(c);
        if (off == ETH_DMA_TX_STAT || off == ETH_DMA_RX_STAT) return ETH_DMA_TX_STAT_RUNNING_MASK;
        return x->regs[off];
    };
    d.hal.wr = [](void* c, uint32_t off, uint32_t v) {
        static_cast<Fake*>(c)->regs[off] = v;
        static_cast<Fake*>(c)->writes.push_back(off);
    };
    d.hal.mem_wr = [](void*, uint32_t, const void*, uint32_t) {};
    d.hal.mem_rd = [](void*, uint32_t, void* dst, uint32_t k) { std::memset(dst, 0xFF, k); };   // OWN=1
    d.hal.delay_us = [](void*, uint32_t) {};
    d.cfg = base_cfg();
    d.tx_head = 3;
    eth_stop_report_t r;
    const int rc = eth_quiesce(&d, 5, 10, &r);
    int halt_reqs = 0;
    bool touched = false;
    for (uint32_t off : f.writes) {
        if (off == ETH_DMA_TX_CTRL) ++halt_reqs;
        touched = touched || off == ETH_TX_MAC_CFG || off == ETH_RX_MAC_CFG || off == ETH_CTRL || off == ETH_INT_EN;
    }
    check(rc == ETH_E_TIMEOUT && r.forced && halt_reqs == 2 && !touched && d.tx_tail == 0 && d.tx_head == 3,
          "F02 HALTED never reached: ETH_E_TIMEOUT after a forced HALT_REQ; enables, ETH_CTRL, INT_EN and "
          "the ring untouched (caller goes FAULTED)");
}

void test_stop(Node& n) {
    test_stop_timeout();
    eth_dev_t d;
    const eth_hal_t h = n.hal();
    eth_stop_report_t r;
    // idle, then a second call
    n.reset();
    eth_cfg_t c = base_cfg();
    eth_init(&d, &h, &c);
    int rc = eth_quiesce(&d, 100, 100, &r);
    check(rc == ETH_OK && !r.forced && r.tx_dropped == 0 && quiescent(n) && !n.irq.read(), "F02 stop when idle: quiescent");
    rc = eth_quiesce(&d, 100, 100, &r);
    check(rc == ETH_OK && quiescent(n), "F02 stop again: still OK");
    check(eth_restart(&d) == ETH_OK && eth_restart(&d) == ETH_E_STATE, "F02 restart from stopped; a second restart while running is refused");
    eth_quiesce(&d, 0, 100, nullptr);

    // transmitting at 10 Mbps, no drain: the frames still queued are dropped
    n.reset();
    c = base_cfg(false);
    eth_init(&d, &h, &c);
    for (uint32_t k = 0; k < 6; ++k) eth_send(&d, frame(1514, 0x200 + k).data(), 1514);
    wait(200, SC_US);
    rc = eth_quiesce(&d, 0, 2000, &r);
    const size_t sent_before = n.peer.received().size();
    check(rc == ETH_OK && quiescent(n) && r.tx_dropped > 0 && r.tx_dropped + r.tx_drained == 6 && d.tx_tail == d.tx_head &&
              r.tx_fifo_flushed,
          "F02 stop while transmitting: quiescent, " + std::to_string(r.tx_dropped) + " queued frames dropped, " +
              std::to_string(r.tx_drained) + " completed");
    // restart: only the new session's frames reach the line, no stale interrupt
    check(!n.irq.read(), "F02 no interrupt pending after stop");
    eth_restart(&d);
    wait(20, SC_US);
    const bool no_stale = n.peer.received().size() == sent_before && !n.irq.read();
    eth_send(&d, frame(100, 0x2FF).data(), 100);
    wait_for([&] { return n.peer.received().size() > sent_before; }, sc_time(2, SC_MS));
    wait(500, SC_US);
    bool only_new = n.peer.received().size() == sent_before + 1 &&
                    tag_of(Bytes(n.peer.received().back().bytes)) == 0x2FF;
    check(no_stale && only_new, "F02 after restart: no frame of the old session on the line, the new frame sent");
    eth_quiesce(&d, 0, 2000, nullptr);

    // with a drain long enough, nothing is dropped
    n.reset();
    eth_init(&d, &h, &c);
    for (uint32_t k = 0; k < 4; ++k) eth_send(&d, frame(800, 0x300 + k).data(), 800);
    rc = eth_quiesce(&d, 20000, 2000, &r);
    wait(2, SC_MS);
    check(rc == ETH_OK && r.tx_dropped == 0 && r.tx_drained == 4 && !r.tx_fifo_flushed && n.peer.received().size() == 4,
          "F02 stop with a drain: every queued frame sent, none dropped (rc " + std::to_string(rc) + " dropped " +
              std::to_string(r.tx_dropped) + " drained " + std::to_string(r.tx_drained) + " on line " +
              std::to_string(n.peer.received().size()) + ")");

    // stalled: TX_EN cleared by hand, the TX FIFO fills, the DMA stream cannot
    // move; the first HALT_REQ is not honoured, the second aborts (9.3.4)
    n.reset();
    c = base_cfg();
    eth_init(&d, &h, &c);
    n.wr(ETH_TX_MAC_CFG, 0);
    for (uint32_t k = 0; k < 6; ++k) eth_send(&d, frame(1514, 0x400 + k).data(), 1514);
    wait(300, SC_US);
    rc = eth_quiesce(&d, 0, 50, &r);
    check(rc == ETH_OK && r.forced && quiescent(n), "F02 stop with the TX FIFO full and TX_EN 0: forced abort, quiescent");
    eth_restart(&d);
    wait(500, SC_US);
    check(n.peer.received().empty(), "F02 the frames left in the TX FIFO were flushed, never sent after restart");
    eth_quiesce(&d, 0, 100, nullptr);

    // receiving: a frame on the line during the stop is not delivered later
    n.reset();
    eth_init(&d, &h, &c);
    n.inject(frame(1514, 0x500));
    wait(40, SC_US);
    rc = eth_quiesce(&d, 0, 100, &r);
    check(rc == ETH_OK && quiescent(n), "F02 stop while a frame is received: quiescent");
    eth_restart(&d);
    n.inject(frame(100, 0x501));
    wait(50, SC_US);
    const auto tags = drain_rx(d);
    check(tags.size() == 1 && tags[0] == 0x501, "F02 after restart only the new frame is received");
    eth_quiesce(&d, 0, 100, nullptr);

    // review ER03: a written-back frame is in the TX FIFO, not yet on the line.
    // Whoever reclaimed it (the owner before the stop, or the stop itself), a
    // stop that flushes it must say so; an idle stop must not. Pre-reclaim
    // all / part / none, no drain or a drain deadline too short, both rates.
    struct Er03 {
        bool s100;
        int pre;              // 0: none, 1: once early, 2: all just before the stop
        uint32_t drain;
    } er03[] = {{false, 2, 0}, {false, 1, 0}, {false, 0, 0}, {false, 2, 50}, {true, 2, 0}, {true, 1, 0}, {true, 2, 5}};
    for (const auto& k : er03) {
        n.reset();
        c = base_cfg(k.s100);
        eth_init(&d, &h, &c);
        const uint32_t len = k.s100 ? 1514 : 800;
        for (uint32_t i = 0; i < 3; ++i) eth_send(&d, frame(len, 0x580 + i).data(), len);
        int pre = 0;
        if (k.pre == 1) {
            wait(k.s100 ? 2 : 5, SC_US);
            pre = eth_tx_reclaim(&d);
        }
        wait(k.s100 ? 20 : 100, SC_US);
        if (k.pre == 2) pre += eth_tx_reclaim(&d);
        rc = eth_quiesce(&d, k.drain, 2000, &r);
        wait(3, SC_MS);
        const uint32_t written = uint32_t(pre) + r.tx_drained, on_line = uint32_t(n.peer.received().size());
        const std::string what = std::string(k.s100 ? "100" : "10") + " Mbps, pre-reclaimed " +
                                 std::to_string(pre) + ", drain " + std::to_string(k.drain) + " us: on line " +
                                 std::to_string(on_line) + ", written back " + std::to_string(written) +
                                 ", dropped " + std::to_string(r.tx_dropped) + ", fifo_flushed " +
                                 std::to_string(r.tx_fifo_flushed);
        check(rc == ETH_OK && written + r.tx_dropped == 3 && on_line <= written && (on_line == written || r.tx_fifo_flushed),
              "ER03 stop with frames in the TX FIFO: every written-back frame either on the line or reported "
              "flushed (" + what + ")");
        check(on_line < written, "ER03 setup: the stop did cut frames out of the TX FIFO (" + what + ")");
        // idle: everything sent long ago, reclaimed by the owner, then a stop without drain
        eth_restart(&d);
        for (uint32_t i = 0; i < 2; ++i) eth_send(&d, frame(len, 0x590 + i).data(), len);
        wait(3, SC_MS);
        eth_tx_reclaim(&d);
        const size_t before = n.peer.received().size();
        rc = eth_quiesce(&d, 0, 2000, &r);
        check(rc == ETH_OK && n.peer.received().size() == before && !r.tx_fifo_flushed && r.tx_dropped == 0 &&
                  before == on_line + 2,
              std::string("ER03 idle stop (") + (k.s100 ? "100" : "10") + " Mbps, frames reclaimed earlier and "
              "already sent): no flush reported");
    }

    // review ER03 (second round): no frame written back before the stop; its
    // DES0 commits while the stop waits for HALTED. With the Tx MAC disabled
    // (a stalled FIFO) or enabled, both rates, no drain or a drain deadline:
    // a frame flushed out of the FIFO is reported
    struct Commit {
        bool s100, tx_en;
        uint32_t drain;
    } commit[] = {{false, false, 0}, {true, false, 0}, {false, false, 30}, {false, true, 0}, {true, true, 0}};
    for (const auto& k : commit) {
        n.reset();
        c = base_cfg(k.s100);
        eth_init(&d, &h, &c);
        if (!k.tx_en) {
            // halt both, disable the Tx MAC, start the DMA again
            n.wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
            n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
            wait(2, SC_US);
            n.wr(ETH_TX_MAC_CFG, 0);
            n.wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
            n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
        }
        n.slow.lo = kTxRing;
        n.slow.hi = kTxRing + 16 * kRing;
        n.slow.delay = sc_time(100, SC_US);
        eth_send(&d, frame(800, 0x5A0).data(), 800);
        wait(5, SC_US);
        uint32_t own = 0;
        n.ddr.read(kTxRing, &own, 4);
        const bool setup = (own >> 31) && d.tx_unconfirmed == 0;
        rc = eth_quiesce(&d, k.drain, 2000, &r);
        n.slow.delay = SC_ZERO_TIME;
        wait(3, SC_MS);
        const uint32_t on_line = uint32_t(n.peer.received().size());
        const std::string what = std::string(k.s100 ? "100" : "10") + " Mbps, Tx MAC " + (k.tx_en ? "on" : "off") +
                                 ", drain " + std::to_string(k.drain) + " us: on line " + std::to_string(on_line) +
                                 ", drained " + std::to_string(r.tx_drained) + ", fifo_flushed " +
                                 std::to_string(r.tx_fifo_flushed);
        check(setup && rc == ETH_OK && r.tx_drained == 1 && r.tx_dropped == 0 && (on_line == 1 || r.tx_fifo_flushed) &&
                  (k.tx_en || (on_line == 0 && r.tx_fifo_flushed)),
              "ER03 DES0 committed while the stop waited for HALTED: a frame cut out of the FIFO is reported (" +
                  what + "; review probe stop-commit)");
    }

    // 100 cycles with traffic both ways
    n.reset();
    eth_init(&d, &h, &c);
    bool ok = true;
    uint32_t rx_total = 0;
    for (uint32_t cyc = 0; cyc < 100 && ok; ++cyc) {
        if (cyc) ok = eth_restart(&d) == ETH_OK;
        const size_t before = n.peer.received().size();
        eth_send(&d, frame(64, 0x10000 + cyc).data(), 64);
        n.inject(frame(64, 0x20000 + cyc));
        wait(30, SC_US);
        const auto t = drain_rx(d);
        ok = ok && t.size() == 1 && t[0] == 0x20000 + cyc && n.peer.received().size() == before + 1 &&
             tag_of(Bytes(n.peer.received().back().bytes)) == 0x10000 + cyc;
        rx_total += uint32_t(t.size());
        ok = ok && eth_quiesce(&d, 1000, 100, &r) == ETH_OK && r.tx_dropped == 0 && quiescent(n) && !n.irq.read();
    }
    check(ok && rx_total == 100 && !n.bus_error, "F02 100 stop/restart cycles with one frame each way: no loss, no stale frame or IRQ");
}

// =============================================================================
void test_recover(Node& n) {
    eth_dev_t d;
    const eth_hal_t h = n.hal();
    eth_cfg_t c = base_cfg();
    eth_err_snap_t s;
    uint32_t dropped = 0;
    // TX: a buffer outside memory -> TX_DATA_RD_ERR with DECERR, TX halts
    n.reset();
    eth_init(&d, &h, &c);
    eth_send(&d, frame(100, 0x600).data(), 100);
    wait(50, SC_US);
    const uint32_t bad = kTxRing + 16 * (d.tx_head % kRing);
    uint32_t des[3] = {0, (3u << 30) | 100u, kUnmapped};
    n.access(tlm::TLM_WRITE_COMMAND, bad + 4, &des[1], 4);
    n.access(tlm::TLM_WRITE_COMMAND, bad + 8, &des[2], 4);
    des[0] = 0x80000000u;
    n.access(tlm::TLM_WRITE_COMMAND, bad, &des[0], 4);
    d.tx_head++;
    eth_send(&d, frame(100, 0x601).data(), 100);              // queued behind the bad one
    wait(50, SC_US);
    const bool tx_halted = n.rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_HALTED_MASK;
    n.inject(frame(80, 0x602));                               // RX keeps running meanwhile
    wait(30, SC_US);
    const auto rx_during = drain_rx(d);
    eth_recover(&d, 1, &s, &dropped);
    // the faulting descriptor is written back OWN=0 with DES0[18:17] (an error
    // completion, counted in tx_errors); the frame queued behind it is dropped
    check(tx_halted && s.halted && s.errinfo == 0x13 && dropped == 1 && d.tx_errors == 1 && rx_during.size() == 1,
          "F02 TX halting error: snapshot ERRINFO 0x13 (DATA, DECERR) before the clear, the bad frame an error "
          "completion, 1 queued frame dropped, RX unaffected"
          " (halted " + std::to_string(tx_halted) + " errinfo " + std::to_string(s.errinfo) + " dropped " +
              std::to_string(dropped) + " rx " + std::to_string(rx_during.size()) + ")");
    const size_t before = n.peer.received().size();
    eth_send(&d, frame(100, 0x603).data(), 100);
    wait(50, SC_US);
    check(n.peer.received().size() == before + 1 && !(n.rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_ERR_MASK) &&
              n.rd(ETH_DMA_TX_ERRINFO) == 0,
          "F02 TX recovered: ERR and ERRINFO clear, the next frame is sent");
    eth_quiesce(&d, 0, 100, nullptr);

    // RX: the held descriptor points outside memory -> RX_DATA_WR_ERR, RX halts
    n.reset();
    eth_init(&d, &h, &c);
    n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    wait(2, SC_US);
    uint32_t v = kUnmapped;
    n.access(tlm::TLM_WRITE_COMMAND, kRxRing + 8, &v, 4);
    n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    wait(2, SC_US);
    n.inject(frame(80, 0x700));
    wait(30, SC_US);
    const bool rx_halted = n.rd(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_HALTED_MASK;
    const size_t tx_before = n.peer.received().size();
    eth_send(&d, frame(90, 0x701).data(), 90);                // TX keeps running
    wait(30, SC_US);
    eth_recover(&d, 0, &s, nullptr);
    n.inject(frame(80, 0x702));
    wait(30, SC_US);
    const auto rx_after = drain_rx(d);
    check(rx_halted && s.halted && s.errinfo == 0x13 && n.peer.received().size() == tx_before + 1 &&
              rx_after.size() == 1 && rx_after[0] == 0x702,
          "F02 RX halting error: ERRINFO 0x13 snapshot, TX unaffected, RX recovered with a rebuilt ring");
    eth_quiesce(&d, 0, 100, nullptr);

    // RX_TRUNC does not halt: ERR cleared, the direction never stopped
    n.reset();
    eth_init(&d, &h, &c);
    n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    wait(2, SC_US);
    n.wr(ETH_DMA_RX_BUF_SIZE, 100);
    n.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    n.inject(frame(300, 0x800));
    wait(40, SC_US);
    eth_recover(&d, 0, &s, nullptr);
    check(!s.halted && (s.errinfo & 0xF) == 4 && !(n.rd(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_ERR_MASK) &&
              (n.rd(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_RUNNING_MASK),
          "F02 RX_TRUNC: left-hand branch, ERR cleared, RX still running");
    eth_quiesce(&d, 0, 100, nullptr);

    // review ER01: ERR comes before HALTED (the faulting descriptor's DES0
    // write-back, held here, and the drain). eth_recover() called at ERR
    // classifies by ERRINFO, waits for HALTED, then recovers.
    auto queue_bad_tx = [&](uint32_t des1, uint32_t des2) {
        const uint32_t at = kTxRing + 16 * (d.tx_head % kRing);
        uint32_t w[3] = {0x80000000u, des1, des2};
        n.access(tlm::TLM_WRITE_COMMAND, at + 4, &w[1], 4);
        n.access(tlm::TLM_WRITE_COMMAND, at + 8, &w[2], 4);
        n.access(tlm::TLM_WRITE_COMMAND, at, &w[0], 4);
        d.tx_head++;
        n.wr(ETH_DMA_TX_POLL, 1);
    };
    auto tx_err = [&] { return wait_for([&] { return (n.rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_ERR_MASK) != 0; }, sc_time(500, SC_US)); };
    const uint32_t HALTED = ETH_DMA_TX_STAT_HALTED_MASK;
    for (const sc_time hold : {sc_time(100, SC_US), sc_time(1500, SC_US)}) {
        n.reset();
        eth_init(&d, &h, &c);
        n.slow.lo = kTxRing;
        n.slow.hi = kTxRing + 16 * kRing;
        n.slow.delay = hold;
        queue_bad_tx((3u << 30) | 100u, kUnmapped);
        const bool err_seen = tx_err();
        const uint32_t st0 = n.rd(ETH_DMA_TX_STAT);
        const sc_time t0 = sc_time_stamp();
        const int rc = eth_recover(&d, 1, &s, &dropped);
        const sc_time took = sc_time_stamp() - t0;
        n.slow.delay = SC_ZERO_TIME;
        const size_t before_line = n.peer.received().size();
        eth_send(&d, frame(100, 0x610).data(), 100);
        wait(50, SC_US);
        check(err_seen && !(st0 & HALTED) && rc == ETH_OK && s.halted && s.errinfo == 0x13 && took >= hold / 2 &&
                  d.tx_errors >= 1 && n.peer.received().size() == before_line + 1 && !(n.rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_ERR_MASK),
              "ER01 TX_DATA_RD_ERR seen before HALTED (DES0 held " + hold.to_string() +
                  "): classified halting by ERRINFO 0x13, recover waited " + took.to_string() +
                  " for HALTED, then SRST; the next frame is sent");
        eth_quiesce(&d, 0, 100, nullptr);
    }
    // HALTED later than the deadline: ETH_E_TIMEOUT, nothing touched
    n.reset();
    eth_init(&d, &h, &c);
    n.slow.lo = kTxRing;
    n.slow.hi = kTxRing + 16 * kRing;
    n.slow.delay = sc_time(5, SC_MS);
    queue_bad_tx((3u << 30) | 100u, kUnmapped);
    tx_err();
    const uint32_t head0 = d.tx_head, tail0 = d.tx_tail;
    int rc = eth_recover(&d, 1, &s, &dropped);
    const uint32_t st1 = n.rd(ETH_DMA_TX_STAT);
    check(rc == ETH_E_TIMEOUT && s.halted && (st1 & ETH_DMA_TX_STAT_ERR_MASK) && !(st1 & HALTED) && d.tx_head == head0 &&
              d.tx_tail == tail0,
          "ER01 HALTED not within ETH_DRV_HALT_WAIT_US: ETH_E_TIMEOUT, ERR kept, ring indices untouched (caller faults)");
    wait(5, SC_MS);
    rc = eth_recover(&d, 1, &s, &dropped);
    check(rc == ETH_OK && s.halted && !(n.rd(ETH_DMA_TX_STAT) & (HALTED | ETH_DMA_TX_STAT_ERR_MASK)),
          "ER01 once HALTED came, the same recovery succeeds");
    n.slow.delay = SC_ZERO_TIME;
    eth_quiesce(&d, 0, 100, nullptr);

    // a DES0 write-back answered SLVERR: TX_DESC_WR_ERR (class 2) halts
    n.reset();
    eth_init(&d, &h, &c);
    n.slow.err_lo = kTxRing;
    n.slow.err_hi = kTxRing + 16 * kRing;
    n.slow.err_left = 1;
    eth_send(&d, frame(100, 0x620).data(), 100);
    tx_err();
    rc = eth_recover(&d, 1, &s, &dropped);
    eth_send(&d, frame(100, 0x621).data(), 100);
    wait(50, SC_US);
    check(rc == ETH_OK && s.halted && (s.errinfo & 0xF) == 2 && !(s.errinfo & 0x10) && n.peer.received().size() == 2,
          "ER01 DES0 write-back SLVERR: TX_DESC_WR_ERR (ERRINFO class 2, no DECERR) halts; recovered, traffic again");
    eth_quiesce(&d, 0, 100, nullptr);

    // TX_TOO_LONG (class 4) does not halt: ERR cleared, TX runs on
    n.reset();
    eth_init(&d, &h, &c);
    queue_bad_tx((3u << 30) | 1600u, kTxBuf);
    tx_err();
    wait(20, SC_US);
    rc = eth_recover(&d, 1, &s, &dropped);
    eth_tx_reclaim(&d);
    eth_send(&d, frame(100, 0x630).data(), 100);
    wait(50, SC_US);
    const uint32_t st2 = n.rd(ETH_DMA_TX_STAT);
    check(rc == ETH_OK && !s.halted && (s.errinfo & 0xF) == 4 && !(st2 & (HALTED | ETH_DMA_TX_STAT_ERR_MASK)) &&
              dropped == 0 && n.peer.received().size() == 1 && tag_of(Bytes(n.peer.received().back().bytes)) == 0x630,
          "ER01 TX_TOO_LONG: class 4, not halting, ERR cleared without SRST, the next frame sent");
    eth_quiesce(&d, 0, 100, nullptr);
}

// =============================================================================
void test_inflight(Node& n) {
    eth_dev_t d;
    const eth_hal_t h = n.hal();
    eth_cfg_t c = base_cfg();
    c.int_en = ETH_INT_RAW_RX_DONE_MASK;
    n.reset();
    eth_init(&d, &h, &c);
    n.slow.lo = kRxRing;
    n.slow.hi = kRxRing + 16 * kRing;
    n.slow.delay = sc_time(5, SC_US);
    n.inject(frame(100, 0x900));
    const bool irq = h.wait_irq(h.ctx, 100);
    const uint32_t st = eth_irq_take(&d);                     // ISR: DONE cleared first
    uint8_t buf[1600];
    uint32_t des0;
    const int first = eth_recv(&d, buf, sizeof buf, &des0);   // OWN still 1
    const uint32_t inflight = eth_rx_inflight(&d);
    int got = 0;
    for (int poll = 0; poll < 100 && got <= 0; ++poll) {      // bounded poll, no IRQ wait
        wait(1, SC_US);
        got = eth_recv(&d, buf, sizeof buf, &des0);
    }
    wait(20, SC_US);
    check(irq && (st & ETH_INT_RAW_RX_DONE_MASK) && first == 0 && inflight == 1 && got == 100 &&
              tag_of(Bytes(buf, buf + got)) == 0x900 && !n.irq.read(),
          "F08 DONE before the DES0 write-back: OWN=1 after the ISR, inflight 1, polled to completion with no "
          "second interrupt");
    // transmit: the same with the TX ring
    n.slow.lo = kTxRing;
    n.slow.hi = kTxRing + 16 * kRing;
    eth_irq_unmask(&d, 0);
    n.wr(ETH_INT_EN, ETH_INT_RAW_TX_DONE_MASK);
    d.cfg.int_en = ETH_INT_RAW_TX_DONE_MASK;
    d.int_en = d.cfg.int_en;
    eth_send(&d, frame(100, 0x901).data(), 100);
    h.wait_irq(h.ctx, 100);
    eth_irq_take(&d);
    const uint32_t tin = eth_tx_inflight(&d);
    const bool done = wait_for([&] { return eth_tx_inflight(&d) == 0 && d.tx_tail == d.tx_head; }, sc_time(100, SC_US));
    check(tin == 1 && done && d.tx_done == 1, "F08 TX: write-back in flight after DONE seen as 1, reclaimed by polling");
    n.slow.delay = SC_ZERO_TIME;
    eth_quiesce(&d, 0, 100, nullptr);
}

// =============================================================================
void test_irq_mdio(Node& n) {
    eth_dev_t d;
    const eth_hal_t h = n.hal();
    eth_cfg_t c = base_cfg();
    n.reset();
    eth_init(&d, &h, &c);
    // a level error: the ISR masks it, the line falls before the PLIC completion
    eth_send(&d, frame(100, 0xA00).data(), 100);
    wait(30, SC_US);
    const uint32_t bad = kTxRing + 16 * (d.tx_head % kRing);
    uint32_t des[3] = {0x80000000u, (3u << 30) | 100u, kUnmapped};
    n.access(tlm::TLM_WRITE_COMMAND, bad + 4, &des[1], 4);
    n.access(tlm::TLM_WRITE_COMMAND, bad + 8, &des[2], 4);
    n.access(tlm::TLM_WRITE_COMMAND, bad, &des[0], 4);
    d.tx_head++;
    n.wr(ETH_DMA_TX_POLL, 1);
    wait(30, SC_US);
    const bool high = n.irq.read();
    const uint32_t st = eth_irq_take(&d);
    wait(100, SC_NS);
    const bool low = !n.irq.read();
    const bool masked = !(n.rd(ETH_INT_EN) & ETH_INT_RAW_TX_ERR_MASK) && (n.rd(ETH_INT_RAW) & ETH_INT_RAW_TX_ERR_MASK);
    eth_recover(&d, 1, nullptr, nullptr);
    eth_irq_unmask(&d, ETH_INT_RAW_TX_ERR_MASK);
    wait(100, SC_NS);
    check(high && (st & ETH_INT_RAW_TX_ERR_MASK) && low && masked && !n.irq.read() &&
              (n.rd(ETH_INT_EN) & ETH_INT_RAW_TX_ERR_MASK),
          "F09 TX_ERR level: eth_irq_take masks it (line low), recover then unmask: no re-entry");

    // MDIO: a stale DONE from an earlier frame does not satisfy the next one
    n.wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (3u << 2) | (1u << 7) | (40u << 12));   // ID2
    wait(40, SC_US);
    uint16_t v = 0;
    int rc = eth_mdio_read(&d, 1, 2, &v);
    check(rc == ETH_OK && v == 0x0007, "F09 MDIO read with a stale DONE pending returns its own value (ID1 0x0007)");
    // a frame still running: the driver waits for it, no START_WHILE_BUSY
    n.wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (3u << 2) | (1u << 7) | (40u << 12));
    rc = eth_mdio_read(&d, 1, 3, &v);
    check(rc == ETH_OK && v == 0xC0F1 && !(n.rd(ETH_MDIO_STAT) & ETH_MDIO_STAT_START_WHILE_BUSY_MASK),
          "F09 MDIO started while a frame runs: waits for it, ID2 0xC0F1, no START_WHILE_BUSY");
    rc = eth_mdio_read(&d, 9, 2, &v);
    check(rc == ETH_E_MDIO, "F09 MDIO to an absent PHY: ETH_E_MDIO (NO_RESPONSE)");
    rc = eth_mdio_read(&d, 1, 1, &v);
    check(rc == ETH_OK && (v & 0x0004), "F09 MDIO works again after the absent PHY (BMSR link up)");
    eth_quiesce(&d, 0, 100, nullptr);
}

struct Top : sc_module {
    std::string mode;
    Node node{"node"};
    SC_HAS_PROCESS(Top);
    Top(sc_module_name nm, std::string m) : sc_module(nm), mode(std::move(m)) { SC_THREAD(run); }
    void run() {
        wait(10, SC_NS);
        if (mode == "cfg") test_cfg(node);
        else if (mode == "stop") test_stop(node);
        else if (mode == "recover") test_recover(node);
        else if (mode == "inflight") test_inflight(node);
        else if (mode == "irq-mdio") test_irq_mdio(node);
        std::printf("=== eth_drv %s: %d checks, %d failures ===\n", mode.c_str(), g_pass + g_fail, g_fail);
        sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode != "cfg" && mode != "stop" && mode != "recover" && mode != "inflight" && mode != "irq-mdio") {
        std::printf("usage: %s cfg|stop|recover|inflight|irq-mdio\n", argv[0]);
        return 2;
    }
    Top top("top", mode);
    sc_start();
    return g_fail == 0 ? 0 : 1;
}
