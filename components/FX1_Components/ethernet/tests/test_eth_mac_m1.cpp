// =============================================================================
// Module: virtual_platform.eth_mac_tlm.tests.test_eth_mac_m1
// Description: Targeted regressions for the review findings F01, F04-F07 and
//              the DMA bus contract (plan M1), each traced to MAS v1.0.7.
// Changelog:
//   2026-11-02: Initial version (FX1 integration plan, milestone M1).
// =============================================================================
//
// One executable, one mode per CTest (a mode may need its own parameter set,
// and SystemC elaborates once per process):
//
//   rx-fifo-reset      F01  MAS Table 6-2 row 8, 6.7, Table 6-31
//   unicast-absent     F04  MAS Table 6-47 row 4, 8.2
//   param-reset        F05  MAS Table 5-1, 8.2: reset values follow parameters
//   invalid:<name>     F05  an illegal EthMacParams fails fast at construction
//   csr-contract       F06  MAS Table 6-47 rows 1, 2, 9, 10
//   collisions         F07  MAS Table 6-47 row 11, Table 6-37, Table 9-3
//   coal-collision     F07  review MR01: an unacknowledged DONE, a W1C, a timer
//                           expiry and a completion on one edge (Table 9-3)
//   dma-bus            M1   MAS 6.10: bursts, 4 KiB, strobes, DES0-only
//                           write-back, ring wrap, data before OWN, on an
//                           annotating and on a blocking memory target
//
// Only the public API of the v1.0.6 import is used, so the same source builds
// against the unmodified baseline to record the fail-before result.
//
// Timing contract used by the collision tests (F07): a CSR access takes
// effect at the simulation time of its b_transport call; the MAC applies a
// hardware event at the time stamp of the process that produces it. "The
// same edge" is the same time stamp, in any delta order.
#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "eth_regs.h"
#include "icsl_eth/eth_core.h"
#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"

using namespace sc_core;
using icsl_eth::EthFrameExt;
using icsl_eth::EthMacParams;
using icsl_eth::EthMacTlm;
using Bytes = std::vector<uint8_t>;
namespace core = icsl_eth::core;

namespace {

int g_fail = 0, g_pass = 0;

void check(bool ok, const std::string& what) {
    ++(ok ? g_pass : g_fail);
    std::printf("[%s] %s @ %s\n", ok ? "PASS" : "FAIL", what.c_str(), sc_time_stamp().to_string().c_str());
}

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

// IP offsets (dram_base = 0)
constexpr uint64_t kMemSize = 0x100'0000;           // 16 MiB; above it: DECERR
constexpr uint32_t kTxRing = 0x1000, kRxRing = 0x2000;
constexpr uint32_t kTxBuf = 0x10000, kRxBuf = 0x20000, kBufStride = 0x800, kRxLead = 2;
constexpr uint32_t kUnmapped = 0x200'0000;
constexpr uint32_t kCtrlAll = 0x3FFF;

const uint8_t kMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t kPeer[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
const uint8_t kOther[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x77};

Bytes frame(size_t len, unsigned seed, const uint8_t* da = kMac) {
    Bytes f(da, da + 6);
    f.insert(f.end(), kPeer, kPeer + 6);
    f.push_back(0x88);                               // local experimental EtherType:
    f.push_back(0xB5);                               // no checksum offload applies
    while (f.size() < len) f.push_back(uint8_t(f.size() * 7 + seed * 13));
    return f;
}

Bytes with_fcs(Bytes f) {
    const auto c = core::fcs(f.data(), f.size());
    f.insert(f.end(), c.begin(), c.end());
    return f;
}

// =============================================================================
// Memory target: annotating (commit at the call, delay annotated) or blocking
// (wait() inside b_transport, then commit).  Records every committed access.
// =============================================================================
class RecMem : public sc_module {
public:
    enum class Mode { Annotate, Blocking };
    struct Op {
        sc_time t;
        bool write;
        uint64_t addr;
        unsigned len;
        Bytes be;                                    // empty: every byte enabled
        Bytes data;
    };
    tlm_utils::simple_target_socket<RecMem> socket;
    Mode mode = Mode::Annotate;
    sc_time latency{50, SC_NS};
    bool logging = false;
    std::vector<Op> log;
    std::function<void(const Op&)> on_commit;

    RecMem(sc_module_name n, uint64_t size) : sc_module(n), socket("socket"), mem_(size, 0) {
        socket.register_b_transport(this, &RecMem::b_transport);
    }
    void read(uint64_t a, void* d, size_t n) const { std::memcpy(d, mem_.data() + a, n); }
    void write(uint64_t a, const void* s, size_t n) { std::memcpy(mem_.data() + a, s, n); }
    void fill(uint64_t a, uint8_t v, size_t n) { std::memset(mem_.data() + a, v, n); }
    uint32_t r32(uint64_t a) const {
        uint32_t v;
        read(a, &v, 4);
        return v;
    }
    void w32(uint64_t a, uint32_t v) { write(a, &v, 4); }
    const uint8_t* ptr(uint64_t a) const { return mem_.data() + a; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
        const uint64_t a = tx.get_address();
        const unsigned len = tx.get_data_length();
        unsigned char* d = tx.get_data_ptr();
        if (a >= mem_.size() || mem_.size() - a < len) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);   // DECERR
            return;
        }
        if (mode == Mode::Blocking) {
            wait(delay + latency);
            delay = SC_ZERO_TIME;
        }
        const unsigned char* be = tx.get_byte_enable_ptr();
        const unsigned bel = tx.get_byte_enable_length();
        Op op{sc_time_stamp(), tx.is_write(), a, len, {}, {}};
        for (unsigned i = 0; i < len; ++i) {
            const bool en = !be || be[i % bel] != TLM_BYTE_DISABLED;
            if (tx.is_write() && en) mem_[a + i] = d[i];
            if (tx.is_read()) d[i] = mem_[a + i];
        }
        if (be)
            for (unsigned i = 0; i < len; ++i) op.be.push_back(be[i % bel]);
        if (tx.is_write()) op.data.assign(d, d + len);
        if (logging) log.push_back(op);
        if (on_commit) on_commit(op);
        if (mode == Mode::Annotate) delay += latency;
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    std::vector<uint8_t> mem_;
};

// One MAC with its memory, PHY (MDIO address 1) and scripted link partner.
struct Rig : sc_module {
    tlm_utils::simple_initiator_socket<Rig> sock;
    sc_signal<bool> rst_n{"rst_n", false};
    sc_signal<bool> rmii_rst_n{"rmii_rst_n", false};
    sc_signal<bool> irq{"irq"};
    RecMem mem;
    EthMacTlm mac;
    icsl_eth::EthPhyTlm phy;
    icsl_eth::EthLinkPartner peer;

    Rig(sc_module_name n, const EthMacParams& p)
        : sc_module(n), sock("sock"), mem("mem", kMemSize), mac("mac", p), phy("phy", 1), peer("peer") {
        sock.bind(mac.csr);
        mac.axi.bind(mem.socket);
        mac.phy_tx.bind(phy.mac_tx);
        phy.mac_rx.bind(mac.phy_rx);
        mac.mdio.bind(phy.mdio);
        phy.line_tx.bind(peer.line_rx);
        peer.line_tx.bind(phy.line_rx);
        mac.rst_n(rst_n);
        mac.rmii_rst_n(rmii_rst_n);
        mac.irq(irq);
    }
};

// =============================================================================
// Software side: register access and bring-up, from the test thread only.
// =============================================================================
struct Ctl {
    Rig& r;
    uint64_t rx_base = 0;                            // frames_received() is not reset by rst_n
    explicit Ctl(Rig& rig) : r(rig) {}
    uint64_t rxf() const { return r.mac.frames_received() - rx_base; }

    tlm::tlm_response_status xfer(tlm::tlm_command cmd, uint64_t addr, uint32_t* v,
                                  const uint8_t* be = nullptr, unsigned be_len = 0) {
        tlm::tlm_generic_payload tx;
        uint8_t buf[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        if (cmd == tlm::TLM_WRITE_COMMAND && v) std::memcpy(buf, v, 4);
        tx.set_command(cmd);
        tx.set_address(addr);
        tx.set_data_ptr(buf);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_byte_enable_ptr(const_cast<uint8_t*>(be));
        tx.set_byte_enable_length(be_len);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = SC_ZERO_TIME;
        r.sock->b_transport(tx, d);                  // the access takes effect here
        wait(d);
        if (cmd == tlm::TLM_READ_COMMAND && v) std::memcpy(v, buf, 4);
        return tx.get_response_status();
    }
    uint32_t rd(uint32_t off) {
        uint32_t v = 0;
        if (xfer(tlm::TLM_READ_COMMAND, off, &v) != tlm::TLM_OK_RESPONSE) check(false, "read OK at " + hex(off));
        return v;
    }
    void wr(uint32_t off, uint32_t v) {
        if (xfer(tlm::TLM_WRITE_COMMAND, off, &v) != tlm::TLM_OK_RESPONSE)
            check(false, "write OK at " + hex(off));
    }
    uint32_t peek(uint32_t off) const { return r.mac.peek(off); }
    bool poll(uint32_t off, uint32_t mask, uint32_t val, sc_time timeout) {
        const sc_time end = sc_time_stamp() + timeout;
        while (sc_time_stamp() < end) {
            if ((peek(off) & mask) == val) return true;
            wait(100, SC_NS);
        }
        return (peek(off) & mask) == val;
    }

    void reset() {
        r.rst_n.write(false);
        r.rmii_rst_n.write(false);
        wait(50, SC_NS);
        r.rst_n.write(true);
        r.rmii_rst_n.write(true);
        wait(50, SC_NS);
        r.peer.received().clear();
        r.mem.logging = false;
        r.mem.log.clear();
        r.mem.on_commit = nullptr;
        rx_base = r.mac.frames_received();
    }
    // Clause 22 write through the MAC's MDIO block; ETH_CTRL[3] must be released.
    void mdio_write(unsigned reg, uint16_t v) {
        wr(ETH_MDIO_DATA, v);
        wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | ETH_MDIO_CTRL_OP_MASK | (reg << 2) | (1u << 7) | (40u << 12));
        poll(ETH_MDIO_STAT, ETH_MDIO_STAT_BUSY_MASK, 0, sc_time(100, SC_US));
        wr(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK);
    }
    void phy_speed(bool s100) {
        mdio_write(0, s100 ? (icsl_eth::EthPhyTlm::kBmcrAnEn | icsl_eth::EthPhyTlm::kBmcrSpeed |
                              icsl_eth::EthPhyTlm::kBmcrFdx)
                           : icsl_eth::EthPhyTlm::kBmcrFdx);
    }
    void rx_desc(unsigned i, bool own, uint32_t off) {
        r.mem.w32(kRxRing + 16 * i + 4, 0);
        r.mem.w32(kRxRing + 16 * i + 8, off);
        r.mem.w32(kRxRing + 16 * i + 12, 0xFEED0000 | i);
        r.mem.w32(kRxRing + 16 * i, own ? 0x80000000u : 0);
    }
    static uint32_t rx_buf(unsigned i) { return kRxBuf + kBufStride * i + kRxLead; }

    // Bring-up of MAS 9.1 with every block out of reset.
    void bringup(bool s100 = true, uint32_t coal = 0, unsigned ring = 8, bool rx_own = true) {
        wr(ETH_CTRL, kCtrlAll);
        phy_speed(s100);
        for (unsigned i = 0; i < ring; ++i) {
            for (unsigned w = 0; w < 4; ++w) r.mem.w32(kTxRing + 16 * i + 4 * w, 0);
            rx_desc(i, rx_own, rx_buf(i));
        }
        wr(ETH_RMII_CFG, s100 ? 1 : 0);
        wr(ETH_UC_ADDR_L0, uint32_t(kMac[0]) | kMac[1] << 8 | kMac[2] << 16 | uint32_t(kMac[3]) << 24);
        wr(ETH_UC_ADDR_H0, uint32_t(kMac[4]) | kMac[5] << 8);
        wr(ETH_UC_CTRL0, 1);
        wr(ETH_DMA_TX_DESC_BASE, kTxRing);
        wr(ETH_DMA_TX_DESC_LEN, ring);
        wr(ETH_DMA_RX_DESC_BASE, kRxRing);
        wr(ETH_DMA_RX_DESC_LEN, ring);
        wr(ETH_DMA_TX_COAL, coal);
        wr(ETH_DMA_RX_COAL, coal);
        wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK);
        wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
        wr(ETH_RX_MAC_CFG, 1);
        wr(ETH_TX_MAC_CFG, 1);
        wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
        wait(2, SC_US);                              // the first descriptor fetches
    }
    // The partner starts the frame now; a spawned process waits out its wire
    // time, so the caller keeps running while the frame is on the line.
    void inject(const Bytes& f, bool s100 = true) {
        EthFrameExt x;
        x.speed100 = s100;
        icsl_eth::EthLinkPartner* peer = &r.peer;
        const Bytes wire = with_fcs(f);
        sc_spawn([peer, wire, x]() { peer->send(wire, x); });
    }
    void wait_until(const sc_time& t) {
        if (t > sc_time_stamp()) wait(t - sc_time_stamp());
    }
};

// =============================================================================
// F01: RX FIFO reset (ETH_CTRL[6] or [13]) around one incoming frame.
// MAS Table 6-2 row 8: any time; the frame being written by the Rx MAC is lost
// and the FIFO discards up to the next SOF; stored frames not yet delivered
// are lost; the frame open at the DMA is voided with no write-back; every
// other block (the Rx MAC and its RMON event included) keeps running and the
// overflow pulses stop.
// =============================================================================
void test_rx_fifo_reset(Ctl& c) {
    const char* when_name[] = {"before SOF, released mid-frame", "mid-frame", "after EOF, before delivery",
                               "during DMA WRITE"};
    unsigned seed = 1;
    for (bool s100 : {true, false}) {
        for (unsigned bit : {6u, 13u}) {
            for (unsigned when = 0; when < 4; ++when) {
                const std::string tag = std::string(s100 ? "100M" : "10M") + " bit " + std::to_string(bit) +
                                        " " + when_name[when];
                c.reset();
                c.bringup(s100);
                const Bytes f1 = frame(1514, seed++), f2 = frame(200, seed++);
                const sc_time wire = icsl_eth::frame_time(f1.size() + 4, s100);
                const uint32_t held = kCtrlAll & ~(1u << bit);
                switch (when) {
                case 0:
                    c.wr(ETH_CTRL, held);
                    wait(1, SC_US);
                    c.inject(f1, s100);
                    wait(wire / 2);
                    c.wr(ETH_CTRL, kCtrlAll);
                    break;
                case 1:
                    c.inject(f1, s100);
                    wait(wire / 2);
                    check(c.peek(ETH_STATUS) & ETH_STATUS_RX_BUSY_MASK, "F01 " + tag + ": RX_BUSY mid-frame");
                    c.wr(ETH_CTRL, held);
                    wait(1, SC_US);
                    c.wr(ETH_CTRL, kCtrlAll);
                    break;
                case 2:
                    c.inject(f1, s100);
                    wait(wire + sc_time(20, SC_NS));
                    c.wr(ETH_CTRL, held);
                    c.wr(ETH_CTRL, kCtrlAll);
                    break;
                case 3:
                    c.inject(f1, s100);
                    wait(wire + sc_time(1, SC_US));
                    check(c.peek(ETH_STATUS) & ETH_STATUS_DMA_RX_BUSY_MASK, "F01 " + tag + ": DMA RX busy");
                    c.wr(ETH_CTRL, held);
                    c.wr(ETH_CTRL, kCtrlAll);
                    break;
                }
                wait(wire + sc_time(20, SC_US));
                const uint32_t des0 = c.r.mem.r32(kRxRing);
                check(des0 == 0x80000000u && !(c.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK) &&
                          c.rxf() == 0,
                      "F01 " + tag + ": interrupted frame has no write-back/DONE (DES0 " + hex(des0) + ")");
                check(c.peek(RMON_RX_PKTS) == 1 && c.peek(RMON_RX_DROP_EVENTS) == 0,
                      "F01 " + tag + ": Rx MAC counted the frame once, no drop event (RX_PKTS " +
                          std::to_string(c.peek(RMON_RX_PKTS)) + ")");
                // the next frame is delivered intact into the same descriptor
                c.inject(f2, s100);
                wait(icsl_eth::frame_time(f2.size() + 4, s100) + sc_time(20, SC_US));
                const uint32_t d2 = c.r.mem.r32(kRxRing);
                Bytes got(f2.size());
                c.r.mem.read(Ctl::rx_buf(0), got.data(), got.size());
                check(!(d2 >> 31) && (d2 & 0xFFFF) == f2.size() && got == f2 && c.rxf() == 1 &&
                          c.peek(RMON_RX_PKTS) == 2,
                      "F01 " + tag + ": next frame received intact (DES0 " + hex(d2) + ")");
            }
        }
    }
}

// =============================================================================
// F04: unicast entries at or above PARA_RX_UNICAST_NUM read 0, ignore writes,
// answer OKAY, whatever the RX_EN window (MAS Table 6-47 row 4, 8.2).
// =============================================================================
void test_unicast_absent(std::vector<Ctl>& rigs) {
    for (auto& c : rigs) {
        const unsigned num = c.r.mac.params().rx_unicast_num;
        const std::string tag = "F04 UC_NUM=" + std::to_string(num);
        c.reset();
        for (unsigned n = 0; n < 4; ++n) {
            const uint32_t L = ETH_UC_ADDR_L0 + 0x10 * n;
            const bool present = n < num;
            uint32_t vl = 0x11111111u * (n + 1), vh = 0xAB00u | n, vc = 1;
            const bool ok = c.xfer(tlm::TLM_WRITE_COMMAND, L, &vl) == tlm::TLM_OK_RESPONSE &&
                            c.xfer(tlm::TLM_WRITE_COMMAND, L + 4, &vh) == tlm::TLM_OK_RESPONSE &&
                            c.xfer(tlm::TLM_WRITE_COMMAND, L + 8, &vc) == tlm::TLM_OK_RESPONSE;
            uint32_t rl = 0xEE, rh = 0xEE, rc = 0xEE;
            const bool rok = c.xfer(tlm::TLM_READ_COMMAND, L, &rl) == tlm::TLM_OK_RESPONSE &&
                             c.xfer(tlm::TLM_READ_COMMAND, L + 4, &rh) == tlm::TLM_OK_RESPONSE &&
                             c.xfer(tlm::TLM_READ_COMMAND, L + 8, &rc) == tlm::TLM_OK_RESPONSE;
            if (present)
                check(ok && rok && rl == vl && rh == vh && rc == 1,
                      tag + ": entry " + std::to_string(n) + " present, written and read back");
            else
                check(ok && rok && rl == 0 && rh == 0 && rc == 0,
                      tag + ": entry " + std::to_string(n) + " absent, OKAY and reads 0 (L " + hex(rl) + ")");
            // a partial-strobe write
            uint32_t ff = 0xFFFFFFFF;
            const uint8_t lane0[4] = {0xFF, 0, 0, 0};
            const bool pok = c.xfer(tlm::TLM_WRITE_COMMAND, L, &ff, lane0, 4) == tlm::TLM_OK_RESPONSE;
            const uint32_t after = c.rd(L);
            check(pok && after == (present ? ((vl & ~0xFFu) | 0xFF) : 0u),
                  tag + ": entry " + std::to_string(n) + " partial strobes " + (present ? "merge" : "ignored"));
        }
        // with the Rx MAC enabled: absent entries still OKAY + ignored, present ones refused
        c.wr(ETH_CTRL, kCtrlAll);
        c.wr(ETH_RX_MAC_CFG, 1);
        for (unsigned n = 0; n < 4; ++n) {
            const uint32_t L = ETH_UC_ADDR_L0 + 0x10 * n;
            const uint32_t before = c.rd(L);
            uint32_t v = 0x5A5A5A5A;
            const auto resp = c.xfer(tlm::TLM_WRITE_COMMAND, L, &v);
            if (n < num)
                check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE && c.rd(L) == before,
                      tag + ": RX_EN=1, present entry " + std::to_string(n) + " refused (window)");
            else
                check(resp == tlm::TLM_OK_RESPONSE && c.rd(L) == 0,
                      tag + ": RX_EN=1, absent entry " + std::to_string(n) + " OKAY, ignored");
        }
        c.wr(ETH_RX_MAC_CFG, 0);
        if (num < 4) {
            // the absent entry `num` programmed with kOther never matches
            c.reset();
            const uint32_t L = ETH_UC_ADDR_L0 + 0x10 * num;
            c.wr(L, uint32_t(kOther[0]) | kOther[1] << 8 | kOther[2] << 16 | uint32_t(kOther[3]) << 24);
            c.wr(L + 4, uint32_t(kOther[4]) | kOther[5] << 8);
            c.wr(L + 8, 1);
            c.bringup(true);
            c.inject(frame(100, 3, kOther));
            wait(30, SC_US);
            c.inject(frame(100, 4, kMac));
            wait(30, SC_US);
            check(c.rxf() == 1 && c.peek(RMON_RX_FILTER_DROP) == 1,
                  tag + ": absent entry does not take part in filtering");
        }
    }
}

// =============================================================================
// F05: reset values that follow parameters (MAS Table 5-1, 8.2), default and
// non-default; the consuming block uses them.
// =============================================================================
void test_param_reset(Ctl& dflt, Ctl& c) {
    auto expect = [](Ctl& k, uint32_t mx, uint32_t div, const std::string& tag) {
        const uint32_t rxm = k.rd(ETH_RX_MAX_FRAME), txm = k.rd(ETH_TX_MAX_FRAME), bs = k.rd(ETH_DMA_RX_BUF_SIZE);
        const uint32_t d = (k.rd(ETH_MDIO_CTRL) & ETH_MDIO_CTRL_CLK_DIV_MASK) >> 12;
        check(rxm == mx && txm == mx && bs == mx - 4 && d == div,
              tag + ": RX/TX_MAX_FRAME " + std::to_string(rxm) + "/" + std::to_string(txm) + ", RX_BUF_SIZE " +
                  std::to_string(bs) + ", CLK_DIV " + std::to_string(d));
    };
    auto mdio_time = [](Ctl& k) {
        k.wr(ETH_CTRL, kCtrlAll);
        const uint32_t ctl = k.rd(ETH_MDIO_CTRL) & ETH_MDIO_CTRL_CLK_DIV_MASK;
        const sc_time t0 = sc_time_stamp();
        k.wr(ETH_MDIO_CTRL, ctl | ETH_MDIO_CTRL_START_MASK | (2u << 2) | (1u << 7));
        while (k.peek(ETH_MDIO_STAT) & ETH_MDIO_STAT_BUSY_MASK) wait(100, SC_NS);
        k.wr(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK);
        return sc_time_stamp() - t0;
    };
    dflt.reset();
    expect(dflt, 1518, 40, "F05 default parameters after reset");
    const sc_time td = mdio_time(dflt);
    check(td >= sc_time(26, SC_US) && td < sc_time(27, SC_US), "F05 default: MDIO frame 26 us (" + td.to_string() + ")");

    c.reset();
    expect(c, 1024, 80, "F05 max_frame_len=1024 mdio_clk_div=80 after power-on reset");
    c.wr(ETH_RX_MAX_FRAME, 2000);
    check(c.rd(ETH_RX_MAX_FRAME) == 1024, "F05 RX_MAX_FRAME above the parameter stores the parameter");
    c.wr(ETH_DMA_RX_BUF_SIZE, 500);
    c.wr(ETH_TX_MAX_FRAME, 600);
    c.wr(ETH_MDIO_CTRL, 50u << 12);
    c.reset();
    expect(c, 1024, 80, "F05 non-default parameters after a second i_eth_rst_n");
    const sc_time t80 = mdio_time(c);
    check(t80 >= sc_time(52, SC_US) && t80 < sc_time(53, SC_US),
          "F05 MDIO frame with the reset divider 80 lasts 52 us (" + t80.to_string() + ")");
    // the Rx MAC consumes the reset RX_MAX_FRAME: an 1100-byte frame is oversize
    c.reset();
    c.bringup(true);
    c.inject(frame(1096, 9));
    wait(150, SC_US);
    check(c.rxf() == 0 && c.peek(RMON_RX_OVERSIZE) == 1,
          "F05 an 1100-byte frame is oversize under the reset RX_MAX_FRAME 1024");
}

// =============================================================================
// F06: the core CSR decode of MAS Table 6-47.
// =============================================================================
void test_csr_contract(Ctl& c, Ctl& alias) {
    c.reset();
    uint32_t v = 0;
    auto resp = c.xfer(tlm::TLM_READ_COMMAND, 0x0024, &v);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0, "F06 row 1: hole 0x0024 read: SLVERR, data 0 (" + hex(v) + ")");
    resp = c.xfer(tlm::TLM_READ_COMMAND, 0x9000, &v);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0, "F06 row 1: hole 0x9000 read: SLVERR, data 0");
    v = 0xFFFFFFFF;
    resp = c.xfer(tlm::TLM_WRITE_COMMAND, 0x0024, &v);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE, "F06 row 2: hole 0x0024 write: SLVERR");
    resp = c.xfer(tlm::TLM_WRITE_COMMAND, 0x9000, &v);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE && c.rd(ETH_FIFO_STAT) == 0 && c.rd(ETH_MDIO_CTRL) == 40u << 12,
          "F06 row 2: hole 0x9000 write: SLVERR, no field changes");
    const uint8_t none[4] = {0, 0, 0, 0};
    resp = c.xfer(tlm::TLM_WRITE_COMMAND, 0x0024, &v, none, 4);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE, "F06 hole write with no strobe: SLVERR (decode precedes strobes)");
    // row 9: address bits [1:0] ignored, the word that contains the address
    for (uint32_t lo = 1; lo < 4; ++lo) {
        v = 0;
        resp = c.xfer(tlm::TLM_READ_COMMAND, ETH_ID + lo, &v);
        check(resp == tlm::TLM_OK_RESPONSE && v == 0x45544801u,
              "F06 row 9: read at ETH_ID+" + std::to_string(lo) + " returns ETH_ID (" + hex(v) + ")");
    }
    v = 0xAABBCCDD;
    resp = c.xfer(tlm::TLM_WRITE_COMMAND, ETH_HASH_L + 2, &v);
    check(resp == tlm::TLM_OK_RESPONSE && c.rd(ETH_HASH_L) == 0xAABBCCDD, "F06 row 9: write at HASH_L+2 writes HASH_L");
    const uint8_t lane1[4] = {0, 0xFF, 0, 0};
    v = 0x00005500;
    c.xfer(tlm::TLM_WRITE_COMMAND, ETH_HASH_L + 1, &v, lane1, 4);
    check(c.rd(ETH_HASH_L) == 0xAABB55DD, "F06 row 9: lanes stay fixed to the word, strobes select bytes");
    // the default 16-bit window: an address beyond it is outside the IP port
    resp = c.xfer(tlm::TLM_READ_COMMAND, 0x10000, &v);
    check(resp == tlm::TLM_ADDRESS_ERROR_RESPONSE, "F06 default window: 0x10000 is not decoded (ADDRESS_ERROR)");
    // debug path follows the same decode
    tlm::tlm_generic_payload t;
    uint8_t buf[4] = {};
    t.set_command(tlm::TLM_READ_COMMAND);
    t.set_address(ETH_ID + 2);
    t.set_data_ptr(buf);
    t.set_data_length(4);
    t.set_streaming_width(4);
    uint32_t id = 0;
    const unsigned n = c.r.sock->transport_dbg(t);
    std::memcpy(&id, buf, 4);
    check(n == 4 && id == 0x45544801u, "F06 transport_dbg at ETH_ID+2 reads ETH_ID");

    // row 10: PARA_AXIL_ADDR_WIDTH = 17, the space repeats every 64 KiB
    alias.reset();
    v = 0;
    resp = alias.xfer(tlm::TLM_READ_COMMAND, 0x10000 + ETH_ID, &v);
    check(resp == tlm::TLM_OK_RESPONSE && v == 0x45544801u, "F06 row 10: 0x10000 aliases ETH_ID (" + hex(v) + ")");
    v = 0x55;
    resp = alias.xfer(tlm::TLM_WRITE_COMMAND, 0x10000 + ETH_HASH_H, &v);
    check(resp == tlm::TLM_OK_RESPONSE && alias.rd(ETH_HASH_H) == 0x55, "F06 row 10: write through the alias");
    resp = alias.xfer(tlm::TLM_READ_COMMAND, 0x10024, &v);
    check(resp == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0, "F06 row 10: alias of a hole is a hole");
    resp = alias.xfer(tlm::TLM_READ_COMMAND, 0x20000, &v);
    check(resp == tlm::TLM_ADDRESS_ERROR_RESPONSE, "F06 row 10: beyond the 17-bit window: ADDRESS_ERROR");
}

// =============================================================================
// F07: a hardware event and a software clear on the same edge.
// A measure pass finds the time stamp T of the event after the trigger; each
// replay lands the software access at T after 0..3 delta cycles (before or
// after the event's process), plus two controls at T - 1 ns and T + 1 ns.
// =============================================================================
struct Collision {
    std::string name;
    std::function<void(Ctl&)> setup;                 // after reset
    std::function<void(Ctl&)> trigger;
    std::function<bool(Ctl&)> seen;                  // the event happened (peek only)
    std::function<void(Ctl&)> action;                // the software access
    std::function<bool(Ctl&)> same_edge;             // MAS result for the same edge
    std::function<bool(Ctl&)> after;                 // result when the access is 1 ns late
    sc_time settle{5, SC_US};
};

sc_time find_event(Ctl& c, const std::function<bool(Ctl&)>& seen, sc_time limit) {
    const sc_time end = sc_time_stamp() + limit;
    while (sc_time_stamp() < end) {
        for (int d = 0; d < 6; ++d) {
            if (seen(c)) return sc_time_stamp();
            wait(SC_ZERO_TIME);
        }
        wait(1, SC_NS);
    }
    return SC_ZERO_TIME;
}

void run_collision(Ctl& c, const Collision& k) {
    c.reset();
    k.setup(c);
    sc_time t0 = sc_time_stamp();
    k.trigger(c);
    const sc_time at = find_event(c, k.seen, sc_time(3, SC_MS));
    if (at == SC_ZERO_TIME) {
        check(false, "F07 " + k.name + ": event not observed");
        return;
    }
    const sc_time lat = at - t0;
    struct Try {
        sc_time off;
        int deltas;
        bool late;
        std::string label;
    };
    std::vector<Try> tries = {{lat - sc_time(1, SC_NS), 0, false, "T-1ns"}};
    for (int d = 0; d < 4; ++d) tries.push_back({lat, d, false, "T+" + std::to_string(d) + "delta"});
    tries.push_back({lat + sc_time(1, SC_NS), 0, true, "T+1ns (control)"});
    for (const auto& t : tries) {
        c.reset();
        k.setup(c);
        t0 = sc_time_stamp();
        k.trigger(c);
        c.wait_until(t0 + t.off);
        for (int d = 0; d < t.deltas; ++d) wait(SC_ZERO_TIME);
        k.action(c);
        wait(k.settle);
        check(t.late ? k.after(c) : k.same_edge(c), "F07 " + k.name + " @ " + t.label);
    }
}

void test_collisions(Ctl& c) {
    const Bytes small = frame(60, 5);
    std::vector<Collision> cases;
    // DONE + W1C(DONE): the sticky bit stays 1
    cases.push_back({"RX DONE + W1C DONE",
                     [](Ctl& k) { k.bringup(true); },
                     [&](Ctl& k) { k.inject(small); },
                     [](Ctl& k) { return k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK; },
                     [](Ctl& k) { k.wr(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK); },
                     [](Ctl& k) { return (k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK) != 0; },
                     [](Ctl& k) { return (k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK) == 0; }});
    // ERR already set by an earlier error; a new error (RX_TRUNC, class 4) on the
    // edge of the W1C of ERR: ERR stays 1 and ERRINFO loads the new error
    cases.push_back({"RX ERR(set) + new RX_TRUNC + W1C ERR",
                     [](Ctl& k) {
                         k.bringup(true);
                         // re-fetch descriptor 0 pointing outside memory:
                         // RX_DATA_WR_ERR with DECERR, ERRINFO 0x13, RX halts
                         k.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
                         wait(2, SC_US);
                         k.rx_desc(0, true, kUnmapped);
                         k.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
                         wait(2, SC_US);
                         k.inject(frame(60, 6));
                         wait(20, SC_US);
                         k.rx_desc(0, true, Ctl::rx_buf(0));
                         k.wr(ETH_DMA_RX_BUF_SIZE, 100);
                         k.wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
                         wait(2, SC_US);
                         k.wr(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK);
                     },
                     [](Ctl& k) { k.inject(frame(300, 7)); },
                     [](Ctl& k) { return k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK; },
                     [](Ctl& k) { k.wr(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_ERR_MASK); },
                     [](Ctl& k) {
                         return (k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_ERR_MASK) && k.peek(ETH_DMA_RX_ERRINFO) == 4;
                     },
                     [](Ctl& k) {
                         return !(k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_ERR_MASK) && k.peek(ETH_DMA_RX_ERRINFO) == 0;
                     }});
    // ETH_DMA_RX_DROP_CNT increment + WC clear: becomes 1
    cases.push_back({"RX_DROP_CNT increment + WC",
                     [](Ctl& k) { k.bringup(true, 0, 8, false); },
                     [&](Ctl& k) { k.inject(small); },
                     [](Ctl& k) { return k.peek(ETH_DMA_RX_DROP_CNT) != 0; },
                     [](Ctl& k) { k.wr(ETH_DMA_RX_DROP_CNT, 0); },
                     [](Ctl& k) { return k.peek(ETH_DMA_RX_DROP_CNT) == 1; },
                     [](Ctl& k) { return k.peek(ETH_DMA_RX_DROP_CNT) == 0; }});
    // RMON CLEAR_ALL + Rx MAC event: the counter keeps the event of its cycle
    cases.push_back({"RMON CLEAR_ALL + RX event",
                     [](Ctl& k) { k.bringup(true); },
                     [&](Ctl& k) { k.inject(small); },
                     [](Ctl& k) { return k.peek(RMON_RX_PKTS) != 0; },
                     [](Ctl& k) { k.wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_CLEAR_ALL_MASK); },
                     [](Ctl& k) { return k.peek(RMON_RX_PKTS) == 1; },
                     [](Ctl& k) { return k.peek(RMON_RX_PKTS) == 0; }});
    // MDIO completion + software write of ETH_MDIO_DATA: the hardware value wins
    cases.push_back({"MDIO read completion + write MDIO_DATA",
                     [](Ctl& k) { k.wr(ETH_CTRL, kCtrlAll); },
                     [](Ctl& k) {
                         k.wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (2u << 2) | (1u << 7) | (40u << 12));
                     },
                     [](Ctl& k) { return k.peek(ETH_MDIO_STAT) & ETH_MDIO_STAT_DONE_MASK; },
                     [](Ctl& k) { k.wr(ETH_MDIO_DATA, 0xBEEF); },
                     [](Ctl& k) { return k.peek(ETH_MDIO_DATA) == 0x0007; },
                     [](Ctl& k) { return k.peek(ETH_MDIO_DATA) == 0xBEEF; }});
    for (const auto& k : cases) run_collision(c, k);

    // RMON read-clear + event on the same edge: no event is lost, and the event
    // counts after the clear (Table 6-37 row 2)
    {
        Collision k{"RMON read-clear + RX event",
                    [&](Ctl& q) {
                        q.bringup(true);
                        q.wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_READ_CLEAR_EN_MASK);
                        q.inject(small);
                        wait(20, SC_US);
                        q.inject(small);
                        wait(20, SC_US);
                    },
                    [&](Ctl& q) { q.inject(small); },
                    [](Ctl& q) { return q.peek(RMON_RX_PKTS) == 3; },
                    nullptr, nullptr, nullptr};
        static uint32_t got = 0;
        k.action = [](Ctl& q) { got = q.rd(RMON_RX_PKTS); };
        // the sample is the value before the edge's update; the counter keeps
        // the event (Table 6-37 row 2); 1 ns late, the read takes all three
        k.same_edge = [](Ctl& q) { return got == 2 && q.peek(RMON_RX_PKTS) == 1; };
        k.after = [](Ctl& q) { return got == 3 && q.peek(RMON_RX_PKTS) == 0; };
        run_collision(c, k);
    }

    // Coalescing (Table 9-3 "Collisions"): COAL_CNT 4, COAL_TIMER 10 (12.8 us).
    const uint32_t coal = 4 | (10u << 8);
    const sc_time D = sc_time(10.0 * 128 * 10, SC_NS);
    auto rx_done = [&](Ctl& k) { return (k.peek(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK) != 0; };
    // a frame completed in the cycle the timer expires: no pulse in that cycle,
    // the count advances and the timer restarts
    c.reset();
    c.bringup(true, coal);
    sc_time t0 = sc_time_stamp();
    c.inject(small);
    const sc_time t1 = find_event(c, [](Ctl& k) { return k.rxf() >= 1; }, sc_time(1, SC_MS));
    const sc_time lat = t1 - t0;
    wait(D + sc_time(1, SC_US));
    check(rx_done(c), "F07 coalescing reference: timer expiry with count 1 reports DONE after 12.8 us");
    for (int extra = 0; extra < 2; ++extra) {
        c.reset();
        c.bringup(true, coal);
        t0 = sc_time_stamp();
        c.inject(small);                             // completes at t0 + lat
        c.wait_until(t0 + D);
        for (int d = 0; d < extra; ++d) wait(SC_ZERO_TIME);
        c.inject(small);                             // completes at t0 + lat + D: the expiry
        c.wait_until(t0 + lat + D + sc_time(100, SC_NS));
        const bool none = !rx_done(c) && c.rxf() == 2;
        c.wait_until(t0 + lat + 2 * D - sc_time(100, SC_NS));
        const bool still_none = !rx_done(c);
        c.wait_until(t0 + lat + 2 * D + sc_time(100, SC_NS));
        check(none && still_none && rx_done(c),
              "F07 completion on the timer-expiry edge: no pulse, timer restarts (variant " + std::to_string(extra) + ")");
    }
    // COAL_TIMER written 0 while frames are counted stops the timer; a later
    // nonzero COAL_TIMER does not restart it until the next completion
    c.reset();
    c.bringup(true, coal);
    c.inject(small);
    find_event(c, [](Ctl& k) { return k.rxf() >= 1; }, sc_time(1, SC_MS));
    const sc_time t_a = sc_time_stamp();
    wait(2, SC_US);
    c.wr(ETH_DMA_RX_COAL, 4);
    wait(2, SC_US);
    c.wr(ETH_DMA_RX_COAL, coal);
    c.wait_until(t_a + D + sc_time(5, SC_US));
    check(!rx_done(c), "F07 COAL_TIMER 0 then nonzero: the stopped timer does not fire");
    c.inject(small);
    find_event(c, [](Ctl& k) { return k.rxf() >= 2; }, sc_time(1, SC_MS));
    const sc_time t_b = sc_time_stamp();
    c.wait_until(t_b + D - sc_time(100, SC_NS));
    const bool quiet = !rx_done(c);
    c.wait_until(t_b + D + sc_time(100, SC_NS));
    check(quiet && rx_done(c), "F07 the next completion restarts the timer: DONE 12.8 us later");
    // COAL_TIMER 0 only: the frames are reported when the count reaches COAL_CNT
    c.reset();
    c.bringup(true, coal);
    c.inject(small);
    find_event(c, [](Ctl& k) { return k.rxf() >= 1; }, sc_time(1, SC_MS));
    c.wr(ETH_DMA_RX_COAL, 4);
    wait(D + sc_time(5, SC_US));
    const bool held = !rx_done(c);
    for (int i = 0; i < 3; ++i) {
        c.inject(small);
        wait(10, SC_US);
    }
    check(held && rx_done(c) && c.rxf() == 4, "F07 COAL_TIMER 0: reported at COAL_CNT");
}

// =============================================================================
// F07 / review MR01: DONE from an earlier report still pending, a coalescing
// timer expiry and a completion on the same edge T, and a W1C of DONE around
// T. MAS Table 9-3: the completion cancels the expiry's pulse; only a pulse
// that reaches COAL_CNT survives, and wins over a W1C of the same edge. So at
// the end of T, DONE = new pulse || (old DONE && no W1C). Checked for both
// directions, through ETH_DMA_*_STAT and the irq output.
// =============================================================================
void test_coal_collision(Ctl& c) {
    const sc_time D = sc_time(10.0 * 128 * 10, SC_NS);         // COAL_TIMER 10
    const Bytes small = frame(60, 5);
    for (bool tx : {false, true}) {
        const uint32_t stat = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
        const uint32_t done = ETH_DMA_TX_STAT_DONE_MASK;
        const uint32_t int_bit = tx ? ETH_INT_RAW_TX_DONE_MASK : ETH_INT_RAW_RX_DONE_MASK;
        unsigned sent = 0;
        auto send = [&](Ctl& k) {
            if (!tx) {
                k.inject(small);
                return;
            }
            const unsigned i = sent++ % 8;
            const uint32_t off = kTxBuf + kBufStride * i;
            k.r.mem.write(off, small.data(), small.size());
            k.r.mem.w32(kTxRing + 16 * i + 4, (3u << 30) | uint32_t(small.size()));
            k.r.mem.w32(kTxRing + 16 * i + 8, off);
            k.r.mem.w32(kTxRing + 16 * i, 0x80000000u);
            k.wr(ETH_DMA_TX_POLL, ETH_DMA_TX_POLL_POLL_MASK);
        };
        auto completions = [tx](Ctl& k) -> uint64_t { return tx ? k.peek(ETH_DMA_TX_CUR_DESC) : k.rxf(); };
        auto is_done = [&](Ctl& k) { return (k.peek(stat) & done) != 0; };
        // One scenario up to frame B: P reported by its timer (old DONE), A at
        // tA, B queued `off_b` after tA. Returns tA + D, the expiry of A's count.
        auto prologue = [&](Ctl& k, bool thr, bool old_done, sc_time off_b, sc_time& ta, sc_time& tb) {
            k.reset();
            sent = 0;
            k.bringup(true, (thr ? 2u : 4u) | (10u << 8));
            k.wr(ETH_INT_EN, int_bit);
            send(k);                                             // P
            wait(D + sc_time(20, SC_US));
            const bool p_reported = is_done(k);
            if (!old_done) k.wr(stat, done);
            wait(1, SC_US);
            const sc_time t0 = sc_time_stamp();
            send(k);                                             // A
            ta = find_event(k, [&](Ctl& q) { return completions(q) >= 2; }, sc_time(200, SC_US));
            k.wait_until(t0 + off_b);
            send(k);                                             // B
            tb = SC_ZERO_TIME;
            return p_reported && is_done(k) == old_done;
        };
        for (bool thr : {false, true}) {
            // calibrate B so that it completes on the expiry edge of A's count
            sc_time off_b = D, ta, tb;
            bool aligned = false;
            for (int it = 0; it < 5 && !aligned; ++it) {
                prologue(c, thr, true, off_b, ta, tb);
                tb = find_event(c, [&](Ctl& q) { return completions(q) >= 3; }, sc_time(200, SC_US));
                aligned = tb == ta + D;
                if (!aligned) off_b = off_b + (ta + D) - tb;
            }
            const std::string dir = std::string(tx ? "TX" : "RX") + (thr ? " COAL_CNT=2 (B reaches it)" : " COAL_CNT=4");
            check(aligned, "MR01 " + dir + ": B completes on the timer-expiry edge (" + (tb - ta).to_string() + " after A)");
            if (!aligned) continue;
            struct Pos {
                const char* name;
                int kind;                                        // 0 none, 1 T-1ns, 2 T+deltas, 3 T+1ns
                int deltas;
            };
            const Pos pos[] = {{"no W1C", 0, 0},   {"W1C at T-1ns", 1, 0}, {"W1C at T+0delta", 2, 0},
                               {"W1C at T+1delta", 2, 1}, {"W1C at T+2delta", 2, 2}, {"W1C at T+3delta", 2, 3},
                               {"W1C at T+1ns", 3, 0}};
            for (bool old_done : {false, true}) {
                for (const auto& p : pos) {
                    const bool pre = prologue(c, thr, old_done, off_b, ta, tb);
                    const sc_time T = ta + D;
                    if (p.kind == 1) c.wait_until(T - sc_time(1, SC_NS));
                    if (p.kind == 2) c.wait_until(T);
                    if (p.kind == 3) c.wait_until(T + sc_time(1, SC_NS));
                    for (int d = 0; d < p.deltas; ++d) wait(SC_ZERO_TIME);
                    if (p.kind != 0) c.wr(stat, done);
                    c.wait_until(T + sc_time(200, SC_NS));
                    const bool want = p.kind == 3 ? false : (thr || (old_done && p.kind == 0));
                    const bool got = is_done(c), irq = c.r.irq.read();
                    const bool three = completions(c) == 3;
                    bool restart = true;
                    if (!thr) {
                        c.wait_until(T + D - sc_time(100, SC_NS));
                        restart = is_done(c) == want;            // no pulse before the restarted timer
                        c.wait_until(T + D + sc_time(100, SC_NS));
                        restart = restart && is_done(c) && c.r.irq.read();
                    }
                    check(pre && three && got == want && irq == want && restart,
                          "MR01 " + dir + ", old DONE " + std::to_string(old_done) + ", " + p.name + ": DONE " +
                              std::to_string(got) + " irq " + std::to_string(irq) + " (want " + std::to_string(want) +
                              ")" + (thr ? "" : restart ? ", restarted timer reports" : ", restarted timer WRONG"));
                }
            }
        }
    }
}

// =============================================================================
// DMA bus contract (MAS 6.10) on an annotating and on a blocking target.
// =============================================================================
void test_dma_bus(Ctl& c) {
    constexpr unsigned kRing = 4, kFrames = 10;
    for (auto mode : {RecMem::Mode::Annotate, RecMem::Mode::Blocking}) {
        const std::string tag = mode == RecMem::Mode::Annotate ? "annotating target" : "blocking target";
        c.reset();
        c.r.mem.mode = mode;
        c.bringup(true, 0, kRing, false);
        c.r.mem.logging = true;

        // ---- TX: two descriptors per frame, the first crossing 4 KiB at an odd lead
        std::vector<Bytes> sent;
        unsigned head = 0;
        bool tx_ok = true;
        for (unsigned f = 0; f < kFrames; f += 2) {
            for (unsigned j = 0; j < 2; ++j) {
                const unsigned k = f + j;
                const Bytes fr = frame(120 + 23 * k, 20 + k);
                const size_t s1 = 37 + 5 * k;
                const uint32_t o1 = kTxBuf + 0x1000 * (k % 4) + 0xFF0 + (k % 4);
                const uint32_t o2 = kTxBuf + 0x8000 + 0x200 * (k % 4) + 3;
                c.r.mem.write(o1, fr.data(), s1);
                c.r.mem.write(o2, fr.data() + s1, fr.size() - s1);
                const unsigned i1 = head++ % kRing, i2 = head++ % kRing;
                c.r.mem.w32(kTxRing + 16 * i1 + 4, (1u << 30) | uint32_t(s1));
                c.r.mem.w32(kTxRing + 16 * i1 + 8, o1);
                c.r.mem.w32(kTxRing + 16 * i2 + 4, (1u << 31) | uint32_t(fr.size() - s1));
                c.r.mem.w32(kTxRing + 16 * i2 + 8, o2);
                c.r.mem.w32(kTxRing + 16 * i2, 0x80000000u);
                c.r.mem.w32(kTxRing + 16 * i1, 0x80000000u);
                sent.push_back(fr);
            }
            c.wr(ETH_DMA_TX_POLL, ETH_DMA_TX_POLL_POLL_MASK);
            const sc_time end = sc_time_stamp() + sc_time(500, SC_US);
            while (c.r.peer.received().size() < f + 2 && sc_time_stamp() < end) wait(1, SC_US);
            tx_ok = tx_ok && c.r.peer.received().size() == f + 2;
        }
        for (unsigned k = 0; k < kFrames && tx_ok; ++k)
            tx_ok = c.r.peer.received()[k].bytes == core::tx_wire_frame(sent[k], 64);
        check(tx_ok, "DMA " + tag + ": 10 two-descriptor TX frames through a 4-entry ring, wire bytes exact");

        // ---- RX: buffers crossing 4 KiB at leads 2..5; software re-arms each descriptor
        std::vector<Bytes> expect;
        std::vector<uint32_t> bufs(kRing);
        bool data_before_own = true;
        unsigned rx_seen = 0;
        c.r.mem.on_commit = [&](const RecMem::Op& op) {
            if (!op.write || op.addr < kRxRing || op.addr >= kRxRing + 16 * kRing) return;
            const unsigned i = unsigned(op.addr - kRxRing) / 16;
            uint32_t des0;
            std::memcpy(&des0, op.data.data(), 4);
            if (des0 >> 31 || rx_seen >= expect.size()) return;
            const Bytes& e = expect[rx_seen++];
            data_before_own = data_before_own && std::memcmp(c.r.mem.ptr(bufs[i]), e.data(), e.size()) == 0;
        };
        bool rx_ok = true, canary_ok = true;
        for (unsigned k = 0; k < kFrames; ++k) {
            const unsigned i = k % kRing;
            bufs[i] = kRxBuf + 0x1000 * i + 0xFF0 + 2 + (k % 4);
            const Bytes fr = frame(60 + 31 * k, 40 + k);
            c.r.mem.fill(bufs[i] - 32, 0xA5, fr.size() + 64);
            c.rx_desc(i, true, bufs[i]);
            c.wr(ETH_DMA_RX_POLL, ETH_DMA_RX_POLL_POLL_MASK);
            expect.push_back(fr);
            c.inject(fr);
            const sc_time end = sc_time_stamp() + sc_time(200, SC_US);
            while ((c.r.mem.r32(kRxRing + 16 * i) >> 31) && sc_time_stamp() < end) wait(1, SC_US);
            const uint32_t des0 = c.r.mem.r32(kRxRing + 16 * i);
            rx_ok = rx_ok && !(des0 >> 31) && (des0 & 0xFFFF) == fr.size() &&
                    std::memcmp(c.r.mem.ptr(bufs[i]), fr.data(), fr.size()) == 0;
            for (unsigned b = 0; b < 32; ++b)
                canary_ok = canary_ok && c.r.mem.ptr(bufs[i] - 32)[b] == 0xA5 &&
                            c.r.mem.ptr(bufs[i] + fr.size())[b] == 0xA5;
        }
        check(rx_ok, "DMA " + tag + ": 10 RX frames through a 4-entry ring into 4 KiB-crossing buffers");
        check(canary_ok, "DMA " + tag + ": RX writes never touch bytes outside [buffer, buffer + length)");
        check(data_before_own && rx_seen == kFrames, "DMA " + tag + ": every payload byte is in memory when OWN=0 commits");

        // ---- the transaction log
        c.r.mem.logging = false;
        c.r.mem.on_commit = nullptr;
        bool shape = true, des_only = true, be_ok = true;
        std::vector<unsigned> tx_wb, rx_wb;
        for (const auto& op : c.r.mem.log) {
            shape = shape && op.addr % 4 == 0 && op.len % 4 == 0 && op.len <= 64 && op.addr % 4096 + op.len <= 4096;
            const bool in_tx = op.addr >= kTxRing && op.addr < kTxRing + 16 * kRing;
            const bool in_rx = op.addr >= kRxRing && op.addr < kRxRing + 16 * kRing;
            if (in_tx || in_rx) {
                if (op.write) {
                    des_only = des_only && op.addr % 16 == 0 && op.len == 4 &&
                               std::all_of(op.be.begin(), op.be.end(), [](uint8_t b) { return b == 0xFF; });
                    (in_tx ? tx_wb : rx_wb).push_back(unsigned(op.addr - (in_tx ? kTxRing : kRxRing)) / 16);
                } else {
                    des_only = des_only && op.addr % 16 == 0 && op.len == 16;
                }
            } else if (op.write) {
                // an RX payload write: enabled bytes inside one buffer's frame
                for (unsigned b = 0; b < op.len; ++b) {
                    if (!op.be.empty() && op.be[b] == 0) continue;
                    const uint64_t a = op.addr + b;
                    bool inside = false;
                    for (unsigned k = 0; k < kFrames && !inside; ++k) {
                        const uint32_t base = kRxBuf + 0x1000 * (k % kRing) + 0xFF0 + 2 + (k % 4);
                        inside = a >= base && a < base + expect[k].size();
                    }
                    be_ok = be_ok && inside;
                }
            }
        }
        bool wrap = tx_wb.size() == 2 * kFrames && rx_wb.size() == kFrames;
        for (size_t n = 0; n < tx_wb.size() && wrap; ++n) wrap = tx_wb[n] == n % kRing;
        for (size_t n = 0; n < rx_wb.size() && wrap; ++n) wrap = rx_wb[n] == n % kRing;
        check(shape, "DMA " + tag + ": bus-aligned beats, at most 16 beats, never across 4 KiB");
        check(des_only, "DMA " + tag + ": 16-byte descriptor reads, DES0-only write-backs with full strobes");
        check(be_ok, "DMA " + tag + ": RX write strobes mask the lead and the tail");
        check(wrap, "DMA " + tag + ": write-backs follow the ring 0,1,2,3,0,... (TX " + std::to_string(tx_wb.size()) +
                        ", RX " + std::to_string(rx_wb.size()) + ")");
        c.r.mem.mode = RecMem::Mode::Annotate;
    }
}

// =============================================================================
struct Top : sc_module {
    std::string mode;
    std::vector<Rig*> rigs;
    SC_HAS_PROCESS(Top);
    Top(sc_module_name n, std::string m, std::vector<Rig*> r) : sc_module(n), mode(std::move(m)), rigs(std::move(r)) {
        SC_THREAD(run);
    }
    void run() {
        wait(20, SC_NS);
        std::vector<Ctl> c;
        for (auto* r : rigs) c.emplace_back(*r);
        if (mode == "rx-fifo-reset") test_rx_fifo_reset(c[0]);
        else if (mode == "unicast-absent") test_unicast_absent(c);
        else if (mode == "param-reset") test_param_reset(c[0], c[1]);
        else if (mode == "csr-contract") test_csr_contract(c[0], c[1]);
        else if (mode == "collisions") test_collisions(c[0]);
        else if (mode == "coal-collision") test_coal_collision(c[0]);
        else if (mode == "dma-bus") test_dma_bus(c[0]);
        std::printf("=== eth_m1 %s: %d checks, %d failures ===\n", mode.c_str(), g_pass + g_fail, g_fail);
        sc_stop();
    }
};

// invalid:<name>: an illegal parameter set must fail fast at construction
bool invalid_params(const std::string& name, EthMacParams& p) {
    if (name == "axi_data_bytes") p.axi_data_bytes = 2;
    else if (name == "axi_max_burst_0") p.axi_max_burst = 0;
    else if (name == "axi_max_burst_17") p.axi_max_burst = 17;
    else if (name == "rx_unicast_num_0") p.rx_unicast_num = 0;
    else if (name == "rx_unicast_num_5") p.rx_unicast_num = 5;
    else if (name == "mdio_clk_div") p.mdio_clk_div = 20;
    else if (name == "poll_cycles") p.poll_cycles = 1000;
    else if (name == "fifo_pow2") p.rx_fifo_bytes = 6000;
    else if (name == "fifo_capacity") p.tx_fifo_bytes = 2048;   // < 2 x 1514: checked by v1.0.6 too
    else if (name == "tx_meta_depth") p.tx_meta_depth = 4;
    else if (name == "dram_base") p.dram_base = 0x800;
    else if (name == "clk_period") p.clk_period = SC_ZERO_TIME;
    else if (name == "csr_window") p.csr_window = 0x8000;
    else if (name == "max_frame_len") p.max_frame_len = 40;
    else return false;
    return true;
}

}  // namespace

int sc_main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode.rfind("invalid:", 0) == 0) {
        EthMacParams p;
        if (!invalid_params(mode.substr(8), p)) {
            std::printf("unknown case %s\n", mode.c_str());
            return 2;
        }
        sc_report_handler::set_actions(SC_FATAL, SC_DISPLAY | SC_THROW);
        sc_report_handler::set_actions(SC_ERROR, SC_DISPLAY | SC_THROW);
        try {
            EthMacTlm mac("mac", p);
            std::printf("ACCEPTED %s: the illegal parameter set was not rejected\n", mode.c_str());
            return 1;
        } catch (const sc_report& r) {
            std::printf("FAILFAST %s: %s\n", mode.c_str(), r.get_msg());
            return 0;
        }
    }
    std::vector<Rig*> rigs;
    EthMacParams p;
    if (mode == "unicast-absent") {
        for (unsigned n = 1; n <= 4; ++n) {
            p.rx_unicast_num = n;
            rigs.push_back(new Rig(("rig" + std::to_string(n)).c_str(), p));
        }
    } else if (mode == "param-reset") {
        rigs.push_back(new Rig("dflt", p));
        p.max_frame_len = 1024;
        p.mdio_clk_div = 80;
        rigs.push_back(new Rig("rig", p));
    } else if (mode == "csr-contract") {
        rigs.push_back(new Rig("dflt", p));
        p.csr_window = 0x20000;
        rigs.push_back(new Rig("alias", p));
    } else if (mode == "rx-fifo-reset" || mode == "collisions" || mode == "coal-collision" || mode == "dma-bus") {
        rigs.push_back(new Rig("rig", p));
    } else {
        std::printf("usage: %s rx-fifo-reset|unicast-absent|param-reset|csr-contract|collisions|coal-collision|dma-bus|invalid:<case>\n",
                    argv[0]);
        return 2;
    }
    Top top("top", mode, rigs);
    sc_start();
    return g_fail == 0 ? 0 : 1;
}
