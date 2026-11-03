// FX1 Ethernet manager (fw/fx1_soc/drivers/fx1_eth.c, built for the host with
// tests/hostshim/fx1_fw.h) on the MAC model, Ethernet review ER01-ER04.
//
// The bench of the component's driver tests (ethernet/tests/eth_drv_fixture.h:
// MAC, PHY, link partner, DDR, a write delayer in front of memory) plus the
// two halves of the FreeRTOS owner (freertos/cmd_eth.c):
//   - an ISR thread: on the level IRQ line, with the hart's interrupts enabled
//     (fx1_irq_save/restore gate it), fx1_eth_isr() in zero simulated time
//     (no owner code interleaves, as on the hart), then a notification;
//   - a service thread: waits for the notification, or for the delay the
//     previous fx1_eth_service() returned, then runs it under the lock.
// Commands take the same lock. MMIO and memory accesses go through TLM and
// wait out their bus time; mtime is sc_time_stamp at 20 MHz.
//
//   error-tx     ER01  TX_DATA_RD_ERR with the DES0 write-back held 100 us: the
//                      error is halting from ERR on (submit refused before and
//                      after HALTED, no soft error), recover then traffic; the
//                      same with auto_recover; HALTED later than the deadline:
//                      FAULTED, then recovered once it came
//   error-rx     ER01  RX_DATA_WR_ERR with the DES0 write-back held: RX halting
//                      from ERR on, TX keeps running, recover then traffic
//   soft         ER01  TX_TOO_LONG and RX_TRUNC: soft errors, nothing halted,
//                      submit accepted, traffic continues (MAS Table 6-32)
//   wb-deadline  ER02  RX DES0 held 1 ms / 5 ms / 60 ms (no commit for a long
//                      time), TX DES0 held 5 ms: no further interrupt, the owner
//                      keeps polling (fast, then slow), never reclaims a
//                      descriptor still owned, takes the frame when it commits
//   stop-flush   ER03  frames reclaimed by the service, then stop without drain:
//                      the flush is reported; an idle stop reports none
//   stats        ER04  an RX descriptor error, recover, clear: counters stay 0
//                      over further services and good traffic, +1 on a new error
//   soft-halt    ER01  (second round) TX_TOO_LONG, then a TX_DATA_RD_ERR before
//                      the owner clears ERR: ERRINFO keeps class 4, no new ERR
//                      or interrupt. HALTED later (DES0 held 100 us) or at once:
//                      found by the watch, submit refused as soon as HALTED;
//                      recover, traffic. A class-4 error alone: watch ends,
//                      nothing halted, the owner back to its idle poll
//   wb-wrap      F08   delayed DES0 write-backs through ring wraps (decisions
//                      review): 48 frames each way through the FX1 ring (16
//                      descriptors, 3 turns), write-backs held 0..2.5 ms in a
//                      pattern that crosses the 2 ms deadline at changing slots;
//                      every frame once and in order, indices/CUR_DESC/OWN at
//                      the end; the last write-backs (slot 15, the DMA's index
//                      wrapped to 0) taken with no further interrupt.
//                      Coalescing off (FX1) and CNT 4 / TIMER 100
//   faulted      N2    stop that cannot halt (DMA writes held 1 ms / 60 ms):
//                      FAULTED, nothing accepted. A recovery while the old
//                      session's writes are in flight fails (ETIMEOUT, twice),
//                      stays FAULTED and leaves the rings alone; once they
//                      landed it succeeds: rings rebuilt, no frame of the old
//                      session delivered, traffic both ways
#include "eth_drv_fixture.h"

#include "fx1_eth.h"
#include "fx1_fw.h"  // tests/hostshim

using namespace ethfix;

namespace {

Node* g_node;
bool g_mie = true;                        // the hart's mstatus.MIE
sc_event* g_mie_on;

}  // namespace

extern "C" uint32_t fx1_read32(uintptr_t a) { return g_node->rd(uint32_t(a - FX1_ETH_CSR_BASE)); }
extern "C" void fx1_write32(uintptr_t a, uint32_t v) { g_node->wr(uint32_t(a - FX1_ETH_CSR_BASE), v); }
extern "C" uint64_t fx1_mtime(void) { return uint64_t(sc_time_stamp().to_seconds() * double(FX1_MTIME_HZ)); }
extern "C" uint32_t fx1_irq_save(void)
{
    const bool was = g_mie;
    g_mie = false;
    return was ? 1u : 0u;
}
extern "C" void fx1_irq_restore(uint32_t saved)
{
    if (!saved) return;
    g_mie = true;
    g_mie_on->notify(SC_ZERO_TIME);
}

namespace {

constexpr uint32_t kHalted = ETH_DMA_TX_STAT_HALTED_MASK, kErr = ETH_DMA_TX_STAT_ERR_MASK;

struct Bench : sc_module {
    Node node{"node"};
    fx1_eth e;
    sc_mutex lock;                        // eth_lock
    sc_event wake;                        // the task notification ...
    bool notified = false;                // ... which, as in FreeRTOS, is not lost while the task runs
    sc_event mie_on;
    uint32_t services = 0, isr_runs = 0;
    std::vector<uint32_t> rx_tags;
    std::string mode;

    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n, std::string m) : sc_module(n), mode(std::move(m))
    {
        g_node = &node;
        g_mie_on = &mie_on;
        fx1_eth_setup(&e);
        SC_THREAD(isr_thread);
        SC_THREAD(service_thread);
        SC_THREAD(run);
    }

    void isr_thread()
    {
        for (;;) {
            if (!node.irq.read()) wait(node.irq.posedge_event());
            if (!g_mie) {
                wait(mie_on);
                continue;
            }
            node.atomic = true;
            fx1_eth_isr(&e);
            node.atomic = false;
            ++isr_runs;
            notify();
            wait(100, SC_NS);             // the line falls with the ISR's CSR writes
        }
    }

    void service_thread()
    {
        int again_ms = 0;
        for (;;) {
            if (!notified) {
                if (again_ms > 0) wait(sc_time(again_ms, SC_MS), wake);
                else wait(wake);
            }
            notified = false;
            lock.lock();
            again_ms = fx1_eth_service(&e);
            ++services;
            lock.unlock();
        }
    }

    void notify()
    {
        notified = true;
        wake.notify(SC_ZERO_TIME);
    }

    template <class F>
    auto cmd(F f)
    {
        lock.lock();
        auto r = f();
        lock.unlock();
        notify();                         // unlock(): pick up what arrived meanwhile
        return r;
    }

    // the manager's state, appended to check messages
    std::string st()
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      " [%s halted %u halts tx %u rx %u soft %u rec %u rto %u wbin %u wbto %u txstat %x rxstat %x info %x/%x rx %zu]",
                      fx1_eth_state_name(e.state), e.err_halted, e.c.tx_halts, e.c.rx_halts, e.c.soft_errors,
                      e.c.recoveries, e.c.recover_timeouts, e.c.wb_inflight, e.c.wb_timeouts,
                      node.rd(ETH_DMA_TX_STAT), node.rd(ETH_DMA_RX_STAT), e.last_err[1].errinfo, e.last_err[0].errinfo,
                      rx_tags.size());
        return b;
    }

    static void on_rx(void* ctx, const uint8_t* f, uint32_t len, uint32_t)
    {
        static_cast<Bench*>(ctx)->rx_tags.push_back(tag_of(Bytes(f, f + len)));
    }

    void start(bool s100 = true, uint32_t ring_len = kRing, uint32_t coal = 0)
    {
        node.reset();
        fx1_eth_setup(&e);
        e.hal = node.hal();
        e.dev.hal = e.hal;
        e.cfg = base_cfg(s100);
        e.cfg.ring_len = ring_len;
        e.cfg.coal = coal;
        e.speed100 = s100;
        e.rx_cb = on_rx;
        e.rx_ctx = this;
        rx_tags.clear();
        check(cmd([&] { return fx1_eth_start(&e); }) == ETH_OK && e.state == FX1_ETH_RUNNING, "start");
    }

    int send(uint32_t len, uint32_t tag)
    {
        const Bytes f = frame(len, tag);
        return cmd([&] { return fx1_eth_send(&e, f.data(), uint32_t(f.size())); });
    }

    // a transmit descriptor written by hand at the head (bad buffer, bad length)
    void queue_tx(uint32_t des1, uint32_t des2)
    {
        cmd([&] {
            const uint32_t at = kTxRing + 16 * (e.dev.tx_head % kRing);
            uint32_t w[3] = {0x80000000u, des1, des2};
            node.access(tlm::TLM_WRITE_COMMAND, at + 4, &w[1], 4);
            node.access(tlm::TLM_WRITE_COMMAND, at + 8, &w[2], 4);
            node.access(tlm::TLM_WRITE_COMMAND, at, &w[0], 4);
            e.dev.tx_head++;
            node.wr(ETH_DMA_TX_POLL, 1);
            return 0;
        });
    }

    // the descriptor the receive DMA holds: its buffer past the end of DDR
    void bad_rx_buffer()
    {
        cmd([&] {
            node.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
            wait(2, SC_US);
            uint32_t v = kUnmapped;
            node.access(tlm::TLM_WRITE_COMMAND, kRxRing + 16 * (e.dev.rx_head % kRing) + 8, &v, 4);
            node.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
            wait(2, SC_US);
            return 0;
        });
    }

    void hold_writes(uint32_t lo, sc_time t)
    {
        node.slow.lo = lo;
        node.slow.hi = lo + 16 * kRing;
        node.slow.delay = t;
    }

    bool traffic(uint32_t tag)
    {
        const size_t line0 = node.peer.received().size(), rx0 = rx_tags.size();
        const bool sent = send(100, tag) == ETH_OK;
        node.inject(frame(90, tag + 1));
        wait_for([&] { return rx_tags.size() > rx0 && node.peer.received().size() > line0; }, sc_time(500, SC_US));
        return sent && node.peer.received().size() == line0 + 1 &&
               tag_of(Bytes(node.peer.received().back().bytes)) == tag && rx_tags.size() == rx0 + 1 &&
               rx_tags.back() == tag + 1;
    }

    // ---------------------------------------------------------------------------------
    void error_tx()
    {
        // halting error seen before HALTED (review probe "error")
        start();
        hold_writes(kTxRing, sc_time(100, SC_US));
        queue_tx((3u << 30) | 100u, kUnmapped);
        const bool taken = wait_for([&] { return (e.err_halted & 2u) != 0; }, sc_time(90, SC_US));
        const uint32_t st0 = node.rd(ETH_DMA_TX_STAT);
        const int early = send(100, 0x1001);
        check(taken && (st0 & kErr) && !(st0 & kHalted) && early == ETH_E_STATE && e.c.tx_halts == 1 &&
                  e.c.soft_errors == 0 && e.last_err[1].errinfo == 0x13 && e.last_err[1].halted,
              "ER01 TX_DATA_RD_ERR before HALTED: halting from ERR on (ERRINFO 0x13), submit refused, no soft error" + st());
        wait(200, SC_US);
        node.slow.delay = SC_ZERO_TIME;
        const uint32_t st1 = node.rd(ETH_DMA_TX_STAT);
        const int late = send(100, 0x1002);
        check((st1 & kHalted) && late == ETH_E_STATE && (e.err_halted & 2u) && e.c.tx_halts == 1 &&
                  e.state == FX1_ETH_RUNNING,
              "ER01 after HALTED: still tracked as one halt, submit refused (review probe: 3/5 failed before)" + st());
        const int rc = cmd([&] { return fx1_eth_recover(&e); });
        check(rc == ETH_OK && e.err_halted == 0 && e.c.recoveries == 1 && e.state == FX1_ETH_RUNNING &&
                  traffic(0x1010),
              "ER01 recover after HALTED: SRST, ring rebuilt, traffic both ways" + st());

        // the same, the service recovering by itself
        e.auto_recover = 1;
        hold_writes(kTxRing, sc_time(100, SC_US));
        queue_tx((3u << 30) | 100u, kUnmapped);
        wait_for([&] { return e.c.recoveries == 2; }, sc_time(2, SC_MS));
        node.slow.delay = SC_ZERO_TIME;
        check(e.c.recoveries == 2 && e.c.tx_halts == 2 && e.err_halted == 0 && e.c.soft_errors == 0 &&
                  e.state == FX1_ETH_RUNNING && traffic(0x1020),
              "ER01 auto_recover: the service waits for HALTED, then recovers; traffic both ways" + st());
        e.auto_recover = 0;

        // HALTED later than ETH_DRV_HALT_WAIT_US: the ring stays with the DMA
        hold_writes(kTxRing, sc_time(5, SC_MS));
        queue_tx((3u << 30) | 100u, kUnmapped);
        wait_for([&] { return (e.err_halted & 2u) != 0; }, sc_time(90, SC_US));
        const int rc2 = cmd([&] { return fx1_eth_recover(&e); });
        check(rc2 == ETH_E_TIMEOUT && e.state == FX1_ETH_FAULTED && e.c.recover_timeouts == 1 &&
                  send(100, 0x1030) == ETH_E_STATE && cmd([&] { return fx1_eth_start(&e); }) == ETH_E_STATE,
              "ER01 HALTED not within the deadline: FAULTED, no submit, no start" + st());
        wait(5, SC_MS);
        node.slow.delay = SC_ZERO_TIME;
        const int rc3 = cmd([&] { return fx1_eth_recover(&e); });
        check(rc3 == ETH_OK && e.state == FX1_ETH_RUNNING && e.err_halted == 0 && traffic(0x1040),
              "ER01 FAULTED recovered once HALTED came (quiesce, restart); traffic both ways" + st());
    }

    void error_rx()
    {
        start();
        bad_rx_buffer();
        hold_writes(kRxRing, sc_time(100, SC_US));
        node.inject(frame(80, 0x2000));
        const bool taken = wait_for([&] { return (e.err_halted & 1u) != 0; }, sc_time(150, SC_US));
        const uint32_t st0 = node.rd(ETH_DMA_RX_STAT);
        const size_t line0 = node.peer.received().size();
        const int sent = send(100, 0x2001);
        wait(50, SC_US);
        check(taken && (st0 & kErr) && !(st0 & kHalted) && e.c.rx_halts == 1 && e.c.soft_errors == 0 &&
                  (e.last_err[0].errinfo & 0xF) == 3 && sent == ETH_OK && node.peer.received().size() == line0 + 1,
              "ER01 RX_DATA_WR_ERR before HALTED: RX halting from ERR on, TX still accepted and sent" + st());
        wait(200, SC_US);
        node.slow.delay = SC_ZERO_TIME;
        check((node.rd(ETH_DMA_RX_STAT) & kHalted) && e.c.rx_halts == 1, "ER01 RX HALTED later: one halt");
        const int rc = cmd([&] { return fx1_eth_recover(&e); });
        check(rc == ETH_OK && e.err_halted == 0 && traffic(0x2010), "ER01 RX recovered; traffic both ways");
    }

    void soft()
    {
        start();
        queue_tx((3u << 30) | 1600u, kTxBuf);                   // TX_TOO_LONG
        wait_for([&] { return e.c.soft_errors == 1; }, sc_time(200, SC_US));
        check(e.c.soft_errors == 1 && e.err_halted == 0 && e.c.tx_halts == 0 &&
                  (e.last_err[1].errinfo & 0xF) == 4 && !e.last_err[1].halted && traffic(0x3000),
              "ER01 TX_TOO_LONG: a soft error, TX not halted, submit accepted, traffic continues" + st());
        cmd([&] {
            node.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
            wait(2, SC_US);
            node.wr(ETH_DMA_RX_BUF_SIZE, 100);
            node.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
            return 0;
        });
        node.inject(frame(300, 0x3010));
        wait_for([&] { return e.c.soft_errors == 2; }, sc_time(200, SC_US));
        wait(20, SC_US);
        node.inject(frame(80, 0x3011));
        wait(50, SC_US);
        check(e.c.soft_errors == 2 && e.c.rx_trunc == 1 && e.c.rx_halts == 0 && e.err_halted == 0 &&
                  rx_tags.size() == 3 && rx_tags.back() == 0x3011,
              "ER01 RX_TRUNC: a soft error, the prefix delivered, RX not halted, the next frame received" + st());
    }

    void wb_deadline()
    {
        // below / above the 2 ms deadline: no further interrupt, the frame is taken
        for (const int hold_ms : {1, 5}) {
            start();
            hold_writes(kRxRing, sc_time(hold_ms, SC_MS));
            const uint32_t isr_before = isr_runs;
            node.inject(frame(100, 0x4000 + hold_ms));
            wait_for([&] { return isr_runs > isr_before; }, sc_time(100, SC_US));
            const uint32_t isr0 = isr_runs;
            const bool got = wait_for([&] { return rx_tags.size() == 1; }, sc_time(hold_ms + 15, SC_MS));
            check(got && isr_runs == isr0 && e.state == FX1_ETH_RUNNING && e.c.wb_inflight >= 1 &&
                      e.c.wb_timeouts == (hold_ms > 2 ? 1u : 0u),
                  "ER02 RX DES0 held " + std::to_string(hold_ms) + " ms after DONE: taken with no new interrupt, " +
                      "wb_timeouts " + std::to_string(e.c.wb_timeouts) + " (review probe: stranded before)" + st() + " isr " + std::to_string(isr_runs - isr0));
            node.slow.delay = SC_ZERO_TIME;
        }
        // a write-back that does not commit for a long time: polled slowly,
        // never reclaimed while OWN reads 1, taken when it commits
        start();
        hold_writes(kRxRing, sc_time(60, SC_MS));
        node.inject(frame(100, 0x4100));
        wait(5, SC_MS);
        const uint32_t s0 = services, head0 = e.dev.rx_head;
        wait(45, SC_MS);
        uint32_t own = 0;
        node.ddr.read(kRxRing, &own, 4);
        const uint32_t polls = services - s0;
        check(polls >= 4 && polls <= 10 && e.dev.rx_head == head0 && (own >> 31) && rx_tags.empty() &&
                  e.state == FX1_ETH_RUNNING && e.c.wb_timeouts == 1,
              "ER02 no commit for 50 ms: the owner polls every 10 ms (" + std::to_string(polls) +
                  " services), the descriptor stays with the DMA, one deadline counted");
        wait_for([&] { return rx_tags.size() == 1; }, sc_time(30, SC_MS));
        check(rx_tags.size() == 1 && rx_tags[0] == 0x4100, "ER02 the frame is taken once the write-back commits");
        node.slow.delay = SC_ZERO_TIME;
        // transmit: the descriptor is reclaimed when its write-back commits
        start();
        hold_writes(kTxRing, sc_time(5, SC_MS));
        send(100, 0x4200);
        wait(1, SC_MS);
        const bool owned = e.dev.tx_tail != e.dev.tx_head;
        const bool done = wait_for([&] { return e.dev.tx_tail == e.dev.tx_head; }, sc_time(20, SC_MS));
        check(owned && done && e.c.wb_timeouts == 1 && e.state == FX1_ETH_RUNNING,
              "ER02 TX DES0 held 5 ms: reclaimed after the commit with no new interrupt" + st());
        node.slow.delay = SC_ZERO_TIME;
    }

    void wb_wrap()
    {
        constexpr uint32_t kN = 48, kFx1Ring = FX1_ETH_RING;          // 3 turns; the last frame in slot 15
        static const int kHoldUs[5] = {0, 100, 2500, 0, 700};    // the n-th write-back of a ring: [n % 5]
        for (const uint32_t coal : {0u, 4u | (100u << 8)}) {
            const std::string cfg = coal ? "coalescing CNT 4 TIMER 100" : "coalescing off (FX1)";
            start(true, kFx1Ring, coal);
            uint32_t wbs[2] = {0, 0};                               // [0] TX ring, [1] RX ring
            bool last[2] = {false, false};
            node.slow.lo = kTxRing;
            node.slow.hi = kRxRing + 16 * kFx1Ring;
            node.slow.delay_of = [&](uint64_t a) {
                const int rx = a >= kRxRing;
                const uint32_t n = wbs[rx]++;
                if (n + 1 == kN) last[rx] = true;
                return sc_time(kHoldUs[n % 5], SC_US);
            };
            // TX as fast as the ring takes it, RX at most 4 frames ahead of delivery
            uint32_t sent = 0, injected = 0, full = 0;
            const sc_time end = sc_time_stamp() + sc_time(500, SC_MS);
            while ((sent < kN || injected < kN) && sc_time_stamp() < end) {
                if (sent < kN) {
                    const int rc = send(100, 0x9000 + sent);
                    if (rc == ETH_OK) ++sent;
                    else if (rc == ETH_E_FULL) ++full;
                }
                if (injected < kN && injected < rx_tags.size() + 4) node.inject(frame(90, 0xA000 + injected++));
                wait(20, SC_US);
            }
            // the last write-back of each ring (slot 15, the DMA's index back at 0)
            // is held 2.5 ms: nothing else is in flight, so the owner must take
            // those by polling. Interrupts of
            // those frames come before the window: DONE at once, or with
            // coalescing at the timer expiry (100 x 128 clocks = 128 us)
            wait_for([&] { return last[0] && last[1]; }, sc_time(100, SC_MS));
            wait(300, SC_US);
            const uint32_t isr0 = isr_runs;
            const bool done = wait_for([&] {
                return rx_tags.size() == kN && e.dev.tx_tail == kN && node.peer.received().size() == kN;
            }, sc_time(50, SC_MS));
            const uint32_t isr_late = isr_runs - isr0;
            wait(1, SC_MS);
            bool rx_order = rx_tags.size() == kN, tx_order = node.peer.received().size() == kN;
            for (uint32_t i = 0; rx_order && i < kN; ++i) rx_order = rx_tags[i] == 0xA000 + i;
            for (uint32_t i = 0; tx_order && i < kN; ++i)
                tx_order = tag_of(Bytes(node.peer.received()[i].bytes)) == 0x9000 + i;
            uint32_t rx_own = 0x80000000u, tx_own = 0;
            for (uint32_t i = 0; i < kFx1Ring; ++i) {
                uint32_t t = 0, r = 0;
                node.ddr.read(kTxRing + 16 * i, &t, 4);
                node.ddr.read(kRxRing + 16 * i, &r, 4);
                tx_own |= t;
                rx_own &= r;
            }
            const uint32_t tx_cur = node.rd(ETH_DMA_TX_CUR_DESC), rx_cur = node.rd(ETH_DMA_RX_CUR_DESC);
            check(sent == kN && injected == kN && full > 0 && done && rx_order && tx_order && e.dev.tx_head == kN &&
                      e.dev.rx_head == kN && tx_cur == kN % kFx1Ring && rx_cur == kN % kFx1Ring &&
                      !(tx_own >> 31) && (rx_own >> 31) && e.c.rx_desc_errors == 0 && e.c.tx_desc_errors == 0 &&
                      e.err_halted == 0 && e.state == FX1_ETH_RUNNING,
                  "F08 " + cfg + ": 48 frames each way through the 16-descriptor ring (3 turns, TX ring full " +
                      std::to_string(full) + " times), write-backs held 0..2.5 ms: every frame once and in order, " +
                      "heads 48, CUR_DESC " + std::to_string(tx_cur) + "/" + std::to_string(rx_cur) +
                      ", TX OWN 0 and RX OWN 1 on every descriptor" + st());
            check(done && isr_late == 0 && e.c.wb_timeouts >= 2 && e.c.wb_inflight > 0,
                  "F08 " + cfg + ": the last write-back of each ring (2.5 ms) taken with no further interrupt; " +
                      "wb_inflight " + std::to_string(e.c.wb_inflight) + ", wb_timeouts " +
                      std::to_string(e.c.wb_timeouts) + st());
            node.slow.delay_of = nullptr;
            check(cmd([&] { return fx1_eth_stop(&e, 2000); }) == ETH_OK && !e.last_stop.tx_fifo_flushed,
                  "F08 " + cfg + ": stop, nothing flushed");
        }
    }

    void stop_flush()
    {
        start(false);
        for (uint32_t i = 0; i < 3; ++i) send(800, 0x5000 + i);
        wait(100, SC_US);
        const bool reclaimed = e.dev.tx_tail == e.dev.tx_head && e.dev.tx_tail == 3;
        const int rc = cmd([&] { return fx1_eth_stop(&e, 0); });
        wait(3, SC_MS);
        const size_t on_line = node.peer.received().size();
        check(reclaimed && rc == ETH_OK && on_line < 3 && e.last_stop.tx_fifo_flushed && e.c.tx_fifo_flushes == 1,
              "ER03 3 frames reclaimed by the service, stop without drain: " + std::to_string(on_line) +
                  " on the line, the flush reported (review probe: not reported before)");
        cmd([&] { return fx1_eth_start(&e); });
        for (uint32_t i = 0; i < 2; ++i) send(800, 0x5010 + i);
        wait(3, SC_MS);
        const int rc2 = cmd([&] { return fx1_eth_stop(&e, 0); });
        check(rc2 == ETH_OK && node.peer.received().size() == on_line + 2 && !e.last_stop.tx_fifo_flushed &&
                  e.c.tx_fifo_flushes == 1,
              "ER03 idle stop without drain after the frames left: no flush reported");
    }

    void stats()
    {
        start();
        bad_rx_buffer();
        node.inject(frame(80, 0x6000));
        wait_for([&] { return (e.err_halted & 1u) != 0; }, sc_time(200, SC_US));
        wait(5, SC_MS);
        check(e.c.wb_inflight == 0 && e.c.wb_timeouts == 0,
              "RX halted by an error, 5 ms before the recovery: CUR_DESC behind the ring head is no write-back "
              "in flight" + st());
        cmd([&] { return fx1_eth_recover(&e); });
        const uint32_t before = e.c.rx_desc_errors;
        cmd([&] {
            fx1_eth_stats_clear(&e);
            return 0;
        });
        bool zero = e.c.rx_desc_errors == 0;
        for (int i = 0; i < 5; ++i) {
            notify();
            wait(1, SC_MS);
            zero = zero && e.c.rx_desc_errors == 0;
        }
        const bool good = traffic(0x6010) && traffic(0x6020);
        check(before == 1 && zero && good && e.c.rx_desc_errors == 0 && e.c.rx_frames == 2,
              "ER04 RX descriptor error, recover, clear: 0 over 5 services and 2 good frames (was " +
                  std::to_string(before) + "; review: it came back to 1)");
        bad_rx_buffer();
        node.inject(frame(80, 0x6030));
        wait_for([&] { return (e.err_halted & 1u) != 0; }, sc_time(200, SC_US));
        wait(50, SC_US);
        cmd([&] { return fx1_eth_recover(&e); });
        check(e.c.rx_desc_errors == 1 && e.c.rx_halts == 1, "ER04 a new error after the clear counts 1");
    }

    void soft_halt()
    {
        // a: 1600 bytes (TX_TOO_LONG, class 4), b: 100 bytes from past the end
        // of memory (TX_DATA_RD_ERR); b's DES0 write-back held `hold`
        for (const sc_time& hold : {sc_time(100, SC_US), SC_ZERO_TIME}) {
            start();
            node.slow.lo = kTxRing + 16;
            node.slow.hi = kTxRing + 32;
            node.slow.delay = hold;
            const uint32_t isr0 = isr_runs;
            cmd([&] {
                uint32_t a[4] = {0x80000000u, 0xC0000000u | 1600u, kTxBuf, 0};
                uint32_t b[4] = {0x80000000u, 0xC0000000u | 100u, kUnmapped, 0};
                node.access(tlm::TLM_WRITE_COMMAND, kTxRing + 16, b, 16);
                node.access(tlm::TLM_WRITE_COMMAND, kTxRing, a, 16);
                e.dev.tx_head = 2;
                node.wr(ETH_DMA_TX_POLL, 1);
                return 0;
            });
            const bool soft_seen = wait_for([&] { return e.c.soft_errors + e.c.tx_halts > 0; }, sc_time(50, SC_US));
            const uint32_t isr1 = isr_runs;
            int early = ETH_OK;
            if (hold != SC_ZERO_TIME) {
                // HALTED came, the watch has not polled yet: the submit itself sees it
                wait_for([&] { return (node.rd(ETH_DMA_TX_STAT) & kHalted) != 0; }, sc_time(150, SC_US));
                early = send(100, 0x8001);
            }
            const bool found = wait_for([&] { return (e.err_halted & 2u) != 0; }, sc_time(3, SC_MS));
            const int late = send(100, 0x8002);
            const std::string what = hold == SC_ZERO_TIME ? "HALTED at once" : "HALTED 100 us later";
            check(soft_seen && found && e.c.tx_halts == 1 && e.c.soft_errors <= 1 && late == ETH_E_STATE &&
                      (hold == SC_ZERO_TIME || early != ETH_OK) && isr_runs == isr1 && isr1 > isr0,
                  "ER01 TX_TOO_LONG then TX_DATA_RD_ERR before the clear (" + what +
                      "): no further interrupt, the halt found, submit refused (review probe soft-halt)" + st());
            node.slow.delay = SC_ZERO_TIME;
            const int rc = cmd([&] { return fx1_eth_recover(&e); });
            check(rc == ETH_OK && e.err_halted == 0 && e.err_watch == 0 && traffic(0x8010),
                  "ER01 recovered after the hidden halt (" + what + "); traffic both ways" + st());
        }
        // a class-4 error alone: the watch ends (TX suspended), nothing halted,
        // the owner falls back to the idle poll
        start();
        queue_tx((3u << 30) | 1600u, kTxBuf);
        wait_for([&] { return e.c.soft_errors == 1; }, sc_time(200, SC_US));
        const bool watched = e.err_watch == 2u;
        wait(5, SC_MS);
        const uint32_t s0 = services;
        wait(250, SC_MS);
        const uint32_t idle_polls = services - s0;
        check(watched && e.err_watch == 0 && e.err_halted == 0 && e.c.tx_halts == 0 && idle_polls >= 2 &&
                  idle_polls <= 3 && traffic(0x8020),
              "ER01 a class-4 error alone: watched, then released; " + std::to_string(idle_polls) +
                  " idle polls in 250 ms (FX1_ETH_POLL_IDLE_MS 100); traffic" + st());
    }

    void faulted()
    {
        for (const int hold_ms : {1, 60}) {
            start();
            node.slow.lo = kTxRing;                             // both rings: TX and RX DES0
            node.slow.hi = kRxRing + 16 * kRing;
            node.slow.delay = sc_time(hold_ms, SC_MS);
            send(100, 0x7000);
            node.inject(frame(90, 0x7001));                     // its RX DES0 held as well
            wait(20, SC_US);
            const int rc = cmd([&] { return fx1_eth_stop(&e, 0); });
            wait(200, SC_US);
            check(rc == ETH_E_TIMEOUT && e.state == FX1_ETH_FAULTED && e.c.failed_stops == 1 &&
                      send(100, 0x7002) == ETH_E_STATE && cmd([&] { return fx1_eth_start(&e); }) == ETH_E_STATE &&
                      cmd([&] { return fx1_eth_service(&e); }) == 0,
                  "faulted: stop with DMA writes held " + std::to_string(hold_ms) +
                      " ms: FAULTED; submit, start and service refused" + st());
            node.bus_error = false;
            std::vector<uint8_t> rings0(kRxRing + 16 * kRing - kTxRing), rings1(rings0.size());
            node.ddr.read(kTxRing, rings0.data(), uint32_t(rings0.size()));
            int tries = 0, rc2 = ETH_E_TIMEOUT;
            bool untouched = true;
            while (rc2 != ETH_OK && tries < 10) {
                if (tries) wait(20, SC_MS);
                ++tries;
                rc2 = cmd([&] { return fx1_eth_recover(&e); });
                if (rc2 != ETH_OK) {
                    // the held writes have not landed: the rings are still the
                    // DMA's; nothing of the manager may have changed them
                    node.ddr.read(kTxRing, rings1.data(), uint32_t(rings1.size()));
                    untouched = untouched && e.state == FX1_ETH_FAULTED && rings1 == rings0;
                }
            }
            node.slow.delay = SC_ZERO_TIME;
            uint32_t own_all = 0x80000000u;
            for (uint32_t i = 0; i < kRing; ++i) {
                uint32_t d0 = 0;
                node.ddr.read(kRxRing + 16 * i, &d0, 4);
                own_all &= d0;
            }
            const size_t rx_before = rx_tags.size();
            wait(2, SC_MS);                                     // nothing of the old session comes
            const bool isolated = rx_tags.size() == rx_before && own_all == 0x80000000u;
            const bool expect_retry = hold_ms > 20;
            check(rc2 == ETH_OK && !node.bus_error && untouched && isolated && e.state == FX1_ETH_RUNNING &&
                      e.c.recoveries == 1 && (expect_retry ? tries >= 3 : tries == 1) &&
                      e.c.recover_timeouts == uint32_t(tries - 1) && traffic(0x7010) && traffic(0x7020) &&
                      rx_tags.size() == rx_before + 2,
                  "faulted (" + std::to_string(hold_ms) + " ms): " + std::to_string(tries) +
                      " recover calls, the failed ones left FAULTED and the rings untouched; once the old writes "
                      "landed: running, every RX descriptor owned again, no frame of the old session, traffic both "
                      "ways (review N2)" + st());
        }
    }

    void run()
    {
        wait(10, SC_NS);
        if (mode == "error-tx") error_tx();
        else if (mode == "error-rx") error_rx();
        else if (mode == "soft") soft();
        else if (mode == "soft-halt") soft_halt();
        else if (mode == "wb-deadline") wb_deadline();
        else if (mode == "wb-wrap") wb_wrap();
        else if (mode == "stop-flush") stop_flush();
        else if (mode == "stats") stats();
        else if (mode == "faulted") faulted();
        std::printf("=== fx1_eth manager %s: %d checks, %d failures ===\n", mode.c_str(), g_pass + g_fail, g_fail);
        sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "";
    static const char* modes[] = {"error-tx", "error-rx", "soft", "soft-halt", "wb-deadline", "wb-wrap", "stop-flush", "stats", "faulted"};
    bool known = false;
    for (const char* m : modes) known = known || mode == m;
    if (!known) {
        std::printf("usage: %s error-tx|error-rx|soft|soft-halt|wb-deadline|wb-wrap|stop-flush|stats|faulted\n", argv[0]);
        return 2;
    }
    Bench bench("bench", mode);
    sc_start();
    return g_fail == 0 ? 0 : 1;
}
