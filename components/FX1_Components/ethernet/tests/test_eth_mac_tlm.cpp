// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.tests.test_eth_mac_tlm
// Description: Unit test of the Ethernet MAC TLM component, CPU-independent.
// Changelog:
//   2026-09-29: Initial version.

// =============================================================================
//
// A SystemC testbench with its own initiator socket drives the register
// window, owns both resets and observes the interrupt.  The MAC's AXI master
// reaches a DDR model through a router (an unmapped address is a DECERR, the
// DDR's error window an SLVERR); its PHY side goes through the PHY model to a
// scripted link partner.  Every register transaction starts INCOMPLETE and
// its response, data and returned delay are checked.  The process ends with
// sc_stop(); the exit status is 0 only when nothing failed.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "eth_regs.h"
#include "icsl_eth/eth_core.h"
#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"
#include "vp/bus_router.h"
#include "vp/memory_tlm.h"

using namespace sc_core;
using icsl_eth::EthFrameExt;
using icsl_eth::EthMacTlm;
using Bytes = std::vector<uint8_t>;
namespace core = icsl_eth::core;

namespace {

int g_fail = 0, g_pass = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_pass;
        std::printf("[PASS] %s @ %s\n", what.c_str(), sc_time_stamp().to_string().c_str());
    } else {
        ++g_fail;
        std::printf("[FAIL] %s @ %s\n", what.c_str(), sc_time_stamp().to_string().c_str());
    }
}

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

// DDR layout (IP offsets; dram_base = 0)
constexpr uint64_t kDdrSize = 0x100'0000;          // 16 MiB; above it: DECERR
constexpr uint32_t kTxRing = 0x1000, kRxRing = 0x2000, kRingLen = 8;
constexpr uint32_t kTxBuf = 0x10000, kRxBuf = 0x20000, kBufStride = 0x800;
constexpr uint32_t kRxLead = 2;                     // RX buffers offset by two bytes
constexpr uint32_t kErrBase = 0x80000, kErrSize = 0x1000;   // SLVERR window
constexpr uint32_t kUnmapped = 0x200'0000;          // DECERR

const uint8_t kMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t kPeer[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

// ---- frame builders -------------------------------------------------------------
Bytes l2(const uint8_t* da, const uint8_t* sa, uint16_t type, const Bytes& payload) {
    Bytes f(da, da + 6);
    f.insert(f.end(), sa, sa + 6);
    f.push_back(uint8_t(type >> 8));
    f.push_back(uint8_t(type));
    f.insert(f.end(), payload.begin(), payload.end());
    return f;
}

Bytes pattern(size_t n, unsigned seed) {
    Bytes b(n);
    for (size_t i = 0; i < n; ++i) b[i] = uint8_t(i * 7 + seed * 13 + (i >> 8));
    return b;
}

// IPv4 + UDP (proto 17) or TCP (6) datagram; checksum fields left as given
Bytes ipv4(uint8_t proto, const Bytes& l4payload, uint16_t csum_fill = 0) {
    const size_t l4h = proto == 17 ? 8 : 20;
    const uint16_t tot = uint16_t(20 + l4h + l4payload.size());
    Bytes ip = {0x45, 0x00, uint8_t(tot >> 8), uint8_t(tot), 0x12, 0x34, 0x40, 0x00, 64, proto,
                uint8_t(csum_fill >> 8), uint8_t(csum_fill), 10, 0, 0, 1, 10, 0, 0, 2};
    Bytes l4(l4h, 0);
    l4[0] = 0x04; l4[1] = 0xD2; l4[2] = 0x16; l4[3] = 0x2E;          // ports 1234 -> 5678
    if (proto == 17) {
        l4[4] = uint8_t((8 + l4payload.size()) >> 8);
        l4[5] = uint8_t(8 + l4payload.size());
        l4[6] = uint8_t(csum_fill >> 8);
        l4[7] = uint8_t(csum_fill);
    } else {
        l4[12] = 0x50;                                              // data offset 5
        l4[13] = 0x18;
        l4[16] = uint8_t(csum_fill >> 8);
        l4[17] = uint8_t(csum_fill);
    }
    ip.insert(ip.end(), l4.begin(), l4.end());
    ip.insert(ip.end(), l4payload.begin(), l4payload.end());
    return ip;
}

Bytes with_fcs(Bytes f) {
    const auto c = core::fcs(f.data(), f.size());
    f.insert(f.end(), c.begin(), c.end());
    return f;
}

struct Tb : sc_module {
    tlm_utils::simple_initiator_socket<Tb> sock;
    sc_signal<bool> rst_n{"rst_n", false};
    sc_signal<bool> rmii_rst_n{"rmii_rst_n", false};
    sc_signal<bool> irq{"irq"};
    EthMacTlm* mac = nullptr;
    vp::MemoryTlm* ddr = nullptr;
    icsl_eth::EthPhyTlm* phy = nullptr;
    icsl_eth::EthLinkPartner* peer = nullptr;
    unsigned tx_head = 0, rx_head = 0;

    SC_HAS_PROCESS(Tb);
    explicit Tb(sc_module_name n) : sc_module(n), sock("sock") { SC_THREAD(run); }

    // ---- bus helpers ------------------------------------------------------------
    tlm::tlm_response_status xfer(tlm::tlm_command cmd, uint32_t off, uint32_t* v,
                                  sc_time* d_out = nullptr, const uint8_t* be = nullptr,
                                  unsigned be_len = 0, unsigned len = 4, unsigned sw = 4,
                                  sc_time d0 = SC_ZERO_TIME) {
        tlm::tlm_generic_payload tx;
        uint8_t buf[8] = {};
        if (cmd == tlm::TLM_WRITE_COMMAND && v) std::memcpy(buf, v, 4);
        tx.set_command(cmd);
        tx.set_address(off);
        tx.set_data_ptr(buf);
        tx.set_data_length(len);
        tx.set_streaming_width(sw);
        tx.set_byte_enable_ptr(const_cast<uint8_t*>(be));
        tx.set_byte_enable_length(be_len);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = d0;
        sock->b_transport(tx, d);
        if (d_out) *d_out = d;
        wait(d);                                    // synchronise every access
        if (cmd == tlm::TLM_READ_COMMAND && v) std::memcpy(v, buf, 4);
        return tx.get_response_status();
    }
    uint32_t rd(uint32_t off) {
        uint32_t v = 0;
        if (xfer(tlm::TLM_READ_COMMAND, off, &v) != tlm::TLM_OK_RESPONSE)
            check(false, "read response OK at " + hex(off));
        return v;
    }
    void wr(uint32_t off, uint32_t v) {
        if (xfer(tlm::TLM_WRITE_COMMAND, off, &v) != tlm::TLM_OK_RESPONSE)
            check(false, "write response OK at " + hex(off));
    }
    // A register whose window is the DMA direction being IDLE (ETH_CSUM_*_CFG,
    // ETH_TX_MAX_FRAME): halt that direction, write, START again at the same index.
    void tx_dma_cfg(uint32_t off, uint32_t v) {
        wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
        poll(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_IDLE_MASK, ETH_DMA_TX_STAT_IDLE_MASK, sc_time(1, SC_MS));
        wr(off, v);
        wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    }
    void rx_dma_cfg(uint32_t off, uint32_t v) {
        wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
        poll(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_IDLE_MASK, ETH_DMA_RX_STAT_IDLE_MASK, sc_time(1, SC_MS));
        wr(off, v);
        wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    }
    
    bool wr_refused(uint32_t off, uint32_t v) {
        return xfer(tlm::TLM_WRITE_COMMAND, off, &v) == tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    // MAC configuration is written only while that MAC is disabled and idle:
    // the Rx MAC is disabled once the line is idle (disabling aborts a frame),
    // the Tx MAC finishes its frame after TX_EN is cleared.
    void rx_cfg(uint32_t off, uint32_t v) {
        while (rd(ETH_STATUS) & ETH_STATUS_RX_BUSY_MASK) wait(200, SC_NS);
        const uint32_t en = rd(ETH_RX_MAC_CFG);
        wr(ETH_RX_MAC_CFG, 0);
        wr(off, v);
        wr(ETH_RX_MAC_CFG, en);
    }
    void tx_cfg(uint32_t off, uint32_t v) {
        const uint32_t en = rd(ETH_TX_MAC_CFG);
        wr(ETH_TX_MAC_CFG, 0);
        while (rd(ETH_STATUS) & ETH_STATUS_TX_BUSY_MASK) wait(200, SC_NS);
        wr(off, v);
        wr(ETH_TX_MAC_CFG, en);
    }
    bool poll(uint32_t off, uint32_t mask, uint32_t val, sc_time timeout) {
        const sc_time end = sc_time_stamp() + timeout;
        while (sc_time_stamp() < end) {
            if ((rd(off) & mask) == val) return true;
            wait(200, SC_NS);
        }
        return (rd(off) & mask) == val;
    }

    // ---- memory helpers -----------------------------------------------------------
    uint32_t m32(uint64_t a) {
        uint32_t v;
        ddr->read(a, &v, 4);
        return v;
    }
    void w32(uint64_t a, uint32_t v) { ddr->write(a, &v, 4); }
    void tx_desc(unsigned i, uint32_t des0, uint32_t des1, uint32_t des2) {
        w32(kTxRing + 16 * i + 4, des1);
        w32(kTxRing + 16 * i + 8, des2);
        w32(kTxRing + 16 * i + 12, 0xC0DE0000 | i);                 // scratch, never written
        w32(kTxRing + 16 * i, des0);
    }
    void rx_desc(unsigned i, bool own, uint32_t off) {
        w32(kRxRing + 16 * i + 4, 0);
        w32(kRxRing + 16 * i + 8, off);
        w32(kRxRing + 16 * i + 12, 0xFEED0000 | i);
        w32(kRxRing + 16 * i, own ? 0x80000000u : 0);
    }
    uint32_t rx_buf(unsigned i) const { return kRxBuf + kBufStride * i + kRxLead; }

    // queue one frame in software buffers: one descriptor per segment
    void queue_tx(const Bytes& f, std::vector<size_t> seg = {}, std::vector<uint32_t> lead = {}) {
        if (seg.empty()) seg.push_back(f.size());
        size_t pos = 0;
        std::vector<unsigned> idx;
        for (size_t s = 0; s < seg.size(); ++s) {
            const unsigned i = tx_head % kRingLen;
            const uint32_t off = kTxBuf + kBufStride * i + (s < lead.size() ? lead[s] : 0);
            ddr->write(off, f.data() + pos, seg[s]);
            const uint32_t des1 = (s + 1 == seg.size() ? 1u << 31 : 0) | (s == 0 ? 1u << 30 : 0) |
                                  uint32_t(seg[s]);
            tx_desc(i, 0, des1, off);
            idx.push_back(i);
            pos += seg[s];
            ++tx_head;
        }
        
        for (auto it = idx.rbegin(); it != idx.rend(); ++it)
            w32(kTxRing + 16 * *it, 0x80000000u);
    }

    void bringup(bool speed100 = true, uint32_t coal = 0) {
        const std::vector<uint8_t> zero(kBufStride * kRingLen, 0);
        ddr->write(kTxBuf, zero.data(), zero.size());
        ddr->write(kRxBuf, zero.data(), zero.size());
        for (unsigned i = 0; i < kRingLen; ++i) {
            tx_desc(i, 0, 0, 0);
            rx_desc(i, true, rx_buf(i));
        }
        tx_head = rx_head = 0;
        wr(ETH_CTRL, 0x3FFF);
        wr(ETH_RMII_CFG, speed100 ? 1 : 0);
        wr(ETH_UC_ADDR_L0, uint32_t(kMac[0]) | kMac[1] << 8 | kMac[2] << 16 | uint32_t(kMac[3]) << 24);
        wr(ETH_UC_ADDR_H0, uint32_t(kMac[4]) | kMac[5] << 8);
        wr(ETH_UC_CTRL0, 1);
        wr(ETH_DMA_TX_DESC_BASE, kTxRing);
        wr(ETH_DMA_TX_DESC_LEN, kRingLen);
        wr(ETH_DMA_RX_DESC_BASE, kRxRing);
        wr(ETH_DMA_RX_DESC_LEN, kRingLen);
        wr(ETH_DMA_TX_COAL, coal);
        wr(ETH_DMA_RX_COAL, coal);
        wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK);
        wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
        wr(ETH_RX_MAC_CFG, 1);
        wr(ETH_TX_MAC_CFG, 1);
        wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    }
    // HALT both directions and hard-reset the IP between scenarios
    void hard_reset() {
        rst_n.write(false);
        rmii_rst_n.write(false);
        wait(50, SC_NS);
        rst_n.write(true);
        rmii_rst_n.write(true);
        wait(50, SC_NS);
        peer->received().clear();
    }
    // frames the peer received, waiting for `n` of them
    bool wait_peer(size_t n, sc_time timeout) {
        const sc_time end = sc_time_stamp() + timeout;
        while (peer->received().size() < n && sc_time_stamp() < end)
            wait(end - sc_time_stamp(), peer->rx_event());
        return peer->received().size() >= n;
    }
    uint32_t rmon(uint32_t off) { return rd(off); }
    
    bool line_idle(sc_time timeout) { return poll(ETH_STATUS, ETH_STATUS_TX_BUSY_MASK, 0, timeout); }

    void run();
    void test_registers();
    void test_errors_and_timing();
    void test_interrupt_and_ctrl();
    void test_mdio();
    void test_tx();
    void test_tx_timing();
    void test_tx_errors();
    void test_tx_suspend_halt();
    void test_rx();
    void test_rx_filter_classes();
    void test_rx_resources();
    void test_rx_errors();
    void test_coalescing();
    void test_rmon_modes();
    void test_resets();
    void test_loopback_10m();
};

// -----------------------------------------------------------------------------
void Tb::test_registers() {
    // reset values of every mapped register, from the generated table
    icsl_eth::CsrFile table;
    int bad = 0;
    for (const auto& kv : table.regs()) {
        const uint32_t want = kv.second.reset & kv.second.defined();
        const uint32_t got = rd(kv.first);
        if (got != want) {
            ++bad;
            std::printf("  reset %s: got %s want %s\n", kv.second.name, hex(got).c_str(), hex(want).c_str());
        }
    }
    check(bad == 0, "reset value of all " + std::to_string(table.regs().size()) + " registers");
    check(rd(ETH_ID) == 0x45544801u, "ETH_ID = 'ETH' v0.1");
    check(rd(ETH_DMA_BUILDCFG) == 0x29AC2405u && rd(ETH_CAP) == 1, "BUILDCFG and CAP at the defaults");

    // RW fields read back, reserved bits read 0, R fields ignore writes
    wr(ETH_HASH_L, 0xFFFFFFFF);
    wr(ETH_UC_ADDR_H2, 0xFFFFFFFF);
    wr(ETH_FILT_CTRL, 0xFFFFFFFF);
    wr(ETH_MDIO_CTRL, 0xFFFFFFFE);
    wr(ETH_ID, 0);
    check(rd(ETH_HASH_L) == 0xFFFFFFFF && rd(ETH_UC_ADDR_H2) == 0xFFFF && rd(ETH_FILT_CTRL) == 7,
          "RW fields read back, reserved bits read 0");
    check(rd(ETH_MDIO_CTRL) == 0x001FFFFE, "MDIO_CTRL: RW fields, START (W1P) reads 0");
    check(rd(ETH_ID) == 0x45544801u, "write to an R register ignored");
    wr(ETH_INT_EN, 0xFF);
    check(rd(ETH_INT_EN) == 0xBF, "ETH_INT_EN bit 6 reserved");
    wr(ETH_INT_EN, 0);

    // byte enables: lanes 1 and 3 of 0xAABBCCDD <- 0x11223344 = 0x11BB33DD
    wr(ETH_HASH_H, 0xAABBCCDD);
    uint32_t v = 0x11223344;
    const uint8_t be[4] = {0x00, 0xFF, 0x00, 0xFF};
    check(xfer(tlm::TLM_WRITE_COMMAND, ETH_HASH_H, &v, nullptr, be, 4) == tlm::TLM_OK_RESPONSE &&
              rd(ETH_HASH_H) == 0x11BB33DD,
          "byte enables update the selected lanes only");
    const uint8_t none[4] = {0, 0, 0, 0};
    v = 0xFFFFFFFF;
    xfer(tlm::TLM_WRITE_COMMAND, ETH_HASH_H, &v, nullptr, none, 4);
    check(rd(ETH_HASH_H) == 0x11BB33DD, "a write with no strobed byte changes nothing");
    // read with byte enables returns only the enabled lanes
    v = 0;
    uint32_t r = 0xEEEEEEEE;
    xfer(tlm::TLM_READ_COMMAND, ETH_HASH_H, &r, nullptr, be, 4);
    check((r & 0xFF00FF00u) == (0x11BB33DDu & 0xFF00FF00u), "read byte enables copy the enabled lanes");

    // registers that store the value in use (MAS Table 6-47 row 14)
    wr(ETH_TX_MAX_FRAME, 0xFFFF);
    wr(ETH_RX_MAX_FRAME, 2000);
    wr(ETH_IPG, 0);
    wr(ETH_TX_MIN_FRAME, 20);
    check(rd(ETH_TX_MAX_FRAME) == 1518 && rd(ETH_RX_MAX_FRAME) == 1518,
          "MAX_FRAME above PARA_MAX_FRAME_LEN stores PARA_MAX_FRAME_LEN");
    check(rd(ETH_IPG) == 12 && rd(ETH_TX_MIN_FRAME) == 64, "IPG below 12 stores 12, TX_MIN_FRAME below 64 stores 64");
    wr(ETH_TX_MIN_FRAME, 0x0120);
    v = 0;
    const uint8_t lane1[4] = {0x00, 0xFF, 0x00, 0x00};
    xfer(tlm::TLM_WRITE_COMMAND, ETH_TX_MIN_FRAME, &v, nullptr, lane1, 4);
    check(rd(ETH_TX_MIN_FRAME) == 64, "the merged value is limited, not the strobed lane");
    wr(ETH_TX_MAX_FRAME, 1518);
    wr(ETH_RX_MAX_FRAME, 1518);
    wr(ETH_IPG, 12);
    wr(ETH_TX_MIN_FRAME, 64);
    // MAC configuration windows: refused (SLVERR) while the MAC is enabled
    wr(ETH_RX_MAC_CFG, 1);
    wr(ETH_TX_MAC_CFG, 1);
    check(wr_refused(ETH_FILT_CTRL, 2) && wr_refused(ETH_UC_ADDR_L1, 0x55) && wr_refused(ETH_IPG, 40) &&
              wr_refused(ETH_RMII_CFG, 1),
          "Rx/Tx MAC configuration refused with an error while the MAC is enabled");
    check(rd(ETH_FILT_CTRL) == 7 && rd(ETH_UC_ADDR_L1) == 0 && rd(ETH_IPG) == 12 && rd(ETH_RMII_CFG) == 0,
          "and left unchanged");
    wr(ETH_RX_MAC_CFG, 0);
    wr(ETH_TX_MAC_CFG, 0);
    wr(ETH_FILT_CTRL, 2);
    wr(ETH_IPG, 40);
    check(rd(ETH_FILT_CTRL) == 2 && rd(ETH_IPG) == 40, "and written once the MAC is disabled and idle");
    wr(ETH_FILT_CTRL, 1);
    wr(ETH_IPG, 12);

    // stopped-only windows: DESC_BASE drops a write while the direction runs
    wr(ETH_CTRL, 0x3FFF);
    wr(ETH_DMA_TX_DESC_BASE, 0x3000);
    wr(ETH_DMA_TX_DESC_LEN, 0);                     // ring of 65536: no OWN anywhere
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    wait(1, SC_US);
    check(wr_refused(ETH_DMA_TX_DESC_BASE, 0x4000) && rd(ETH_DMA_TX_DESC_BASE) == 0x3000,
          "DESC_BASE write refused while TX runs");
    check(wr_refused(ETH_DMA_AXI_CFG, 0x33) && rd(ETH_DMA_AXI_CFG) == 0,
          "AXI_CFG write refused unless both directions are stopped");
    check(wr_refused(ETH_TX_MAX_FRAME, 1000) && wr_refused(ETH_CSUM_TX_CFG, 0) && rd(ETH_TX_MAX_FRAME) == 1518 &&
              rd(ETH_CSUM_TX_CFG) == 3,
          "TX_MAX_FRAME and CSUM_TX_CFG refused while TX runs");
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x7, ETH_DMA_TX_STAT_HALTED_MASK | ETH_DMA_TX_STAT_IDLE_MASK, sc_time(20, SC_US)),
          "HALT_REQ in SUSPEND: HALTED and IDLE");
    wr(ETH_DMA_TX_DESC_BASE, 0x4001);
    check(rd(ETH_DMA_TX_DESC_BASE) == 0x4000, "DESC_BASE written when stopped, [3:0] read 0");
    check(rd(ETH_DMA_TX_CTRL) == 0, "DMA_TX_CTRL (W1P) reads 0");
    // WC: ETH_DMA_RX_DROP_CNT cleared by any strobed write only (see test_rx_resources)
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_errors_and_timing() {
    uint32_t v = 0;
    sc_time d;
    // 2026-11-02 (review F06): MAS Table 6-47 rows 1, 2 and 9. An ADDR_DEC miss
    // is SLVERR (GENERIC_ERROR) with read data 0; address bits [1:0] select
    // the word that contains the address. v1.0.6 answered ADDRESS_ERROR.
    v = 0xEEEEEEEE;
    check(xfer(tlm::TLM_READ_COMMAND, 0x0024, &v) == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0,
          "reserved offset 0x0024: SLVERR (GENERIC_ERROR), read data 0");
    check(xfer(tlm::TLM_WRITE_COMMAND, 0x9000, &v) == tlm::TLM_GENERIC_ERROR_RESPONSE,
          "unmapped block 0x9000: SLVERR (GENERIC_ERROR)");
    check(xfer(tlm::TLM_READ_COMMAND, 0x10000, &v) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
          "offset beyond the 64 KiB window: ADDRESS_ERROR (never reaches the IP)");
    check(xfer(tlm::TLM_READ_COMMAND, 0x0002, &v) == tlm::TLM_OK_RESPONSE && v == 0x45544801u,
          "unaligned offset 0x0002: the word that contains it, ETH_ID");
    check(xfer(tlm::TLM_READ_COMMAND, ETH_ID, &v, nullptr, nullptr, 0, 2, 2) ==
              tlm::TLM_GENERIC_ERROR_RESPONSE,
          "length 2: GENERIC_ERROR");
    check(xfer(tlm::TLM_READ_COMMAND, ETH_ID, &v, nullptr, nullptr, 0, 4, 2) ==
              tlm::TLM_BURST_ERROR_RESPONSE,
          "streaming width 2: BURST_ERROR");
    const uint8_t be[1] = {0xFF};
    check(xfer(tlm::TLM_READ_COMMAND, ETH_ID, &v, nullptr, be, 0) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
          "byte-enable pointer with length 0: BYTE_ENABLE_ERROR");
    check(xfer(tlm::TLM_IGNORE_COMMAND, ETH_ID, &v) == tlm::TLM_COMMAND_ERROR_RESPONSE,
          "IGNORE command: COMMAND_ERROR");
    xfer(tlm::TLM_READ_COMMAND, ETH_ID, &v, &d, nullptr, 0, 4, 4, sc_time(5, SC_NS));
    check(d == sc_time(25, SC_NS), "b_transport annotates 20 ns (5 ns in, 25 ns out)");

    // debug path: reads without time, refuses writes, never clears a counter
    tlm::tlm_generic_payload t;
    uint8_t buf[4] = {};
    t.set_command(tlm::TLM_READ_COMMAND);
    t.set_address(ETH_ID);
    t.set_data_ptr(buf);
    t.set_data_length(4);
    t.set_streaming_width(4);
    const sc_time t0 = sc_time_stamp();
    const unsigned n = sock->transport_dbg(t);
    uint32_t id;
    std::memcpy(&id, buf, 4);
    check(n == 4 && id == 0x45544801u && sc_time_stamp() == t0, "transport_dbg reads ETH_ID, no time");
    t.set_command(tlm::TLM_WRITE_COMMAND);
    t.set_address(ETH_HASH_L);
    check(sock->transport_dbg(t) == 0, "transport_dbg refuses writes");
    t.set_command(tlm::TLM_READ_COMMAND);
    t.set_address(0x0024);
    check(sock->transport_dbg(t) == 0, "transport_dbg of an unmapped offset: 0 bytes");
    tlm::tlm_dmi dmi;
    t.set_address(ETH_ID);
    check(!sock->get_direct_mem_ptr(t, dmi), "DMI refused on the register window");
}

// -----------------------------------------------------------------------------
void Tb::test_interrupt_and_ctrl() {
    check(!irq.read(), "irq low out of reset");
    wr(ETH_INT_EN, 0x04);
    wr(ETH_INT_INJ, 0x04);
    check(rd(ETH_INT_RAW) == 0 && rd(ETH_INT_STATUS) == 0x04, "INJ reaches STATUS, not RAW");
    check(irq.read(), "irq is a registered level: high one clock after STATUS");
    wr(ETH_INT_INJ, 0);
    check(!irq.read() && rd(ETH_INT_STATUS) == 0, "clearing the source drops the level");
    wr(ETH_INT_INJ, 0x80);
    check(rd(ETH_INT_STATUS) == 0, "a disabled source does not reach STATUS");
    wr(ETH_INT_INJ, 0);
    wr(ETH_INT_EN, 0);

    // ETH_CTRL holds every block but the CSR: START, MDIO and RMON inert
    wr(ETH_CTRL, 0);
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (1u << 7));
    wait(1, SC_US);
    check((rd(ETH_DMA_TX_STAT) & 0x7) == ETH_DMA_TX_STAT_IDLE_MASK && (rd(ETH_MDIO_STAT) & 1) == 0,
          "ETH_CTRL = 0: DMA START and MDIO START land on blocks in reset");
    wr(ETH_CTRL, 0xFFEF);                           // DMA_RST_N alone held
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    check((rd(ETH_DMA_RX_STAT) & 1) == 0, "ETH_CTRL[4] = 0 holds the DMA alone");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_mdio() {
    wr(ETH_CTRL, 0x3FFF);
    wr(ETH_INT_EN, ETH_INT_EN_EN_MDIO_DONE_MASK);
    auto mdio_rd = [&](unsigned phy, unsigned reg, uint32_t extra = 0) {
        wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (phy << 7) | (reg << 2) | (40u << 12) | extra);
    };
    const sc_time t0 = sc_time_stamp();
    mdio_rd(1, 2);
    check(rd(ETH_MDIO_STAT) & ETH_MDIO_STAT_BUSY_MASK, "MDIO BUSY after START");
    mdio_rd(1, 3);
    check(rd(ETH_MDIO_STAT) & ETH_MDIO_STAT_START_WHILE_BUSY_MASK, "START while BUSY: START_WHILE_BUSY");
    wait(irq.posedge_event());
    const sc_time dt = sc_time_stamp() - t0;
    check(rd(ETH_MDIO_DATA) == 0x0007, "PHYID1 read over MDIO");
    check(dt >= sc_time(26, SC_US) && dt < sc_time(26.2, SC_US),
          "65 bit times at CLK_DIV 40: 26.0 us (" + dt.to_string() + ")");
    check((rd(ETH_INT_RAW) & ETH_INT_RAW_MDIO_DONE_MASK) && (rd(ETH_MDIO_STAT) & 1) == 0,
          "DONE sets MDIO_DONE, BUSY clear");
    wr(ETH_MDIO_STAT, 0xE);
    check(rd(ETH_MDIO_STAT) == 0 && !irq.read(), "W1C clears DONE and START_WHILE_BUSY, irq drops");

    // preamble suppression after the first frame: 34 bit times
    const sc_time t1 = sc_time_stamp();
    mdio_rd(1, 3, ETH_MDIO_CTRL_PRE_SUPPRESS_MASK);
    check(poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US)),
          "second MDIO read completes");
    const sc_time dt1 = sc_time_stamp() - t1;
    check(rd(ETH_MDIO_DATA) == 0xC0F1 && dt1 < sc_time(14, SC_US) && dt1 >= sc_time(13.6, SC_US),
          "PHYID2 with a 1-bit preamble in 13.6 us (" + dt1.to_string() + ")");
    wr(ETH_MDIO_STAT, 0xE);

    // no PHY at address 5: turnaround reads 1, data 0xFFFF, NO_RESPONSE
    mdio_rd(5, 1);
    poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US));
    check(rd(ETH_MDIO_DATA) == 0xFFFF && (rd(ETH_MDIO_STAT) & ETH_MDIO_STAT_NO_RESPONSE_MASK),
          "no PHY: data 0xFFFF and NO_RESPONSE");
    wr(ETH_MDIO_STAT, 0xE);

    // write BMCR: loopback, auto-negotiation off, 100 Mbps; read it back
    wr(ETH_MDIO_DATA, 0x6100);
    wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | ETH_MDIO_CTRL_OP_MASK | (1u << 7) | (0u << 2) | (40u << 12));
    poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US));
    check(rd(ETH_MDIO_DATA) == 0x6100 && phy->loopback() && phy->reg(0) == 0x6100,
          "MDIO write reaches the PHY; DATA holds the written value");
    wr(ETH_MDIO_STAT, 0xE);
    // a reset during a frame: no DONE, and the next frame sends the full preamble
    mdio_rd(1, 0, ETH_MDIO_CTRL_PRE_SUPPRESS_MASK);
    wait(5, SC_US);
    wr(ETH_CTRL, 0xFFF7);
    check((rd(ETH_MDIO_STAT) & 0x9) == 0, "MDIO reset mid-frame: BUSY clear at once, no DONE");
    wr(ETH_CTRL, 0x3FFF);
    const sc_time t2 = sc_time_stamp();
    mdio_rd(1, 0, ETH_MDIO_CTRL_PRE_SUPPRESS_MASK);
    poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US));
    check(sc_time_stamp() - t2 >= sc_time(26, SC_US), "first frame after a reset: full 32-bit preamble");
    // restore the PHY: loopback off, auto-negotiation on
    wr(ETH_MDIO_STAT, 0xE);
    wr(ETH_MDIO_DATA, 0x3100);
    wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | ETH_MDIO_CTRL_OP_MASK | (1u << 7) | (40u << 12));
    poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US));
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_tx() {
    bringup();
    wr(ETH_INT_EN, ETH_INT_EN_EN_MASK);
    // 1: a 20-byte frame, padded to 64 on the line
    const Bytes f1 = l2(kPeer, kMac, 0x88B5, pattern(6, 1));
    // 2: UDP with garbage checksums in three unaligned, odd-length buffers
    const Bytes f2 = l2(kPeer, kMac, 0x0800, ipv4(17, pattern(301, 2), 0xBEEF));
    // 3: TCP, maximum frame (1514 + FCS)
    const Bytes f3 = l2(kPeer, kMac, 0x0800, ipv4(6, pattern(1460, 3), 0x1234));
    // 4: broadcast, non-IP
    const uint8_t bc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const Bytes f4 = l2(bc, kMac, 0x0806, pattern(46, 4));
    queue_tx(f1);
    queue_tx(f2, {17, 200, f2.size() - 217}, {3, 1, 2});
    queue_tx(f3);
    queue_tx(f4);
    wr(ETH_DMA_TX_POLL, 1);
    check(wait_peer(4, sc_time(2, SC_MS)) && line_idle(sc_time(200, SC_US)), "four frames reach the line");
    if (peer->received().size() < 4) return;
    const auto& rx = peer->received();
    check(rx[0].bytes == core::tx_wire_frame(f1, 64) && rx[0].bytes.size() == 64,
          "short frame padded to 64 bytes with a valid FCS");
    Bytes e2 = f2;
    core::tx_checksum_insert(e2, true, true);
    check(rx[1].bytes == core::tx_wire_frame(e2, 64), "multi-buffer frame joined across unaligned buffers");
    const Bytes w2(rx[1].bytes.begin(), rx[1].bytes.end() - 4);
    check(core::rx_checksum(w2, true, true) == 0x2A, "UDP: IPv4 and UDP checksums inserted (verdict 0x2A)");
    const Bytes w3(rx[2].bytes.begin(), rx[2].bytes.end() - 4);
    check(rx[2].bytes.size() == 1518 && core::rx_checksum(w3, true, true) == 0x29,
          "TCP 1518-byte frame: checksums inserted (verdict 0x29)");
    check(rx[3].bytes == core::tx_wire_frame(f4, 64), "ARP broadcast untouched by checksum offload");
    for (const auto& r : rx) check(core::fcs_ok(r.bytes.data(), r.bytes.size()), "FCS residue on the line");
    // DES0 write-back: OWN = 0, status 0; DES3 untouched
    bool wb = true;
    for (unsigned i = 0; i < 6; ++i) wb = wb && m32(kTxRing + 16 * i) == 0 && m32(kTxRing + 16 * i + 12) == (0xC0DE0000u | i);
    check(wb, "every descriptor written back OWN = 0, status 0; DES3 untouched");
    check(rd(ETH_DMA_TX_CUR_DESC) == 6 && (rd(ETH_DMA_TX_STAT) & 0x3F) == (0x20 | 0x8 | 0x1),
          "CUR_DESC 6, DONE set, SUSPENDED on the empty ring");
    check((rd(ETH_INT_RAW) & 0x11) == 0x11 && irq.read(), "TX_DONE and TX_BUF_UNAVAIL raise the interrupt");
    check(rmon(RMON_TX_PKTS) == 4 && rmon(RMON_TX_OCTETS) == 64 + w2.size() + 4 + 1518 + 64 &&
              rmon(RMON_TX_BCAST) == 1 && rmon(RMON_TX_PKTS_64) == 2 &&
              rmon(RMON_TX_PKTS_256_511) == 1 && rmon(RMON_TX_PKTS_1024_1518) == 1,
          "RMON transmit counters");
    wr(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    check((rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_DONE_MASK) == 0, "DONE is W1C");

    // checksum insertion off: the frame leaves as built
    tx_dma_cfg(ETH_CSUM_TX_CFG, 0);
    peer->received().clear();
    queue_tx(f2);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(1, sc_time(1, SC_MS));
    check(!peer->received().empty() && peer->received()[0].bytes == core::tx_wire_frame(f2, 64),
          "CSUM_TX_EN = 0: garbage checksums transmitted unchanged");
    // IPv4 only: header checksum inserted, UDP field untouched
    tx_dma_cfg(ETH_CSUM_TX_CFG, 1);
    peer->received().clear();
    queue_tx(f2);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(1, sc_time(1, SC_MS));
    Bytes e2b = f2;
    core::tx_checksum_insert(e2b, true, false);
    check(!peer->received().empty() && peer->received()[0].bytes == core::tx_wire_frame(e2b, 64) &&
              e2b[14 + 20 + 6] == 0xBE,
          "CSUM_TX_L4_EN = 0: IPv4 header only");
    // ETH_TX_MIN_FRAME above 64 pads further
    tx_dma_cfg(ETH_CSUM_TX_CFG, 3);
    tx_cfg(ETH_TX_MIN_FRAME, 100);
    peer->received().clear();
    queue_tx(f1);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(1, sc_time(1, SC_MS));
    check(!peer->received().empty() && peer->received()[0].bytes.size() == 100,
          "ETH_TX_MIN_FRAME = 100 pads to 100 bytes");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_tx_timing() {
    bringup();
    wr(ETH_TX_MAC_CFG, 0);                          // hold the Tx MAC: frames wait in the FIFO
    const Bytes f = l2(kPeer, kMac, 0x88B5, pattern(100, 7));   // 114 + FCS = 118 on the line
    for (int i = 0; i < 3; ++i) queue_tx(f);
    wr(ETH_DMA_TX_POLL, 1);
    wait(50, SC_US);
    check(peer->received().empty(), "TX_EN = 0: nothing on the line");
    const sc_time t_en = sc_time_stamp();
    wr(ETH_TX_MAC_CFG, 1);
    wait_peer(3, sc_time(1, SC_MS));
    const auto& r = peer->received();
    const sc_time bt(80, SC_NS);
    check(r.size() == 3 && r[0].start - t_en >= bt * 12.0 && r[0].start - t_en < bt * 13.0,
          "first frame starts one IPG after the claim");
    check(r.size() == 3 && r[1].start - r[0].start == bt * double(8 + 118 + 12) &&
              r[2].start - r[1].start == bt * double(8 + 118 + 12),
          "back-to-back: exactly 12 byte times between frames");
    tx_cfg(ETH_IPG, 20);
    peer->received().clear();
    for (int i = 0; i < 2; ++i) queue_tx(f);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(2, sc_time(1, SC_MS));
    check(r.size() == 2 && r[1].start - r[0].start == bt * double(8 + 118 + 20), "ETH_IPG = 20 byte times");
    tx_cfg(ETH_IPG, 5);
    peer->received().clear();
    for (int i = 0; i < 2; ++i) queue_tx(f);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(2, sc_time(1, SC_MS));
    check(rd(ETH_IPG) == 12 && r.size() == 2 && r[1].start - r[0].start == bt * double(8 + 118 + 12),
          "ETH_IPG below 12 stores 12 and serves 12");
    wait(bt * double(8 + 118));                     // the link partner records a frame at its start
    check((rd(ETH_STATUS) & ETH_STATUS_TX_BUSY_MASK) == 0, "TX_BUSY clear with the line idle");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_tx_errors() {
    const Bytes ok = l2(kPeer, kMac, 0x88B5, pattern(80, 9));
    // ---- TX_TOO_LONG: a 3-buffer frame of 1515 + FCS > 1518, then a good one
    bringup();
    const Bytes big = l2(kPeer, kMac, 0x88B5, pattern(1501, 8));   // 1515 bytes
    queue_tx(big, {600, 600, 315});
    queue_tx(ok);
    wr(ETH_DMA_TX_POLL, 1);
    wait_peer(1, sc_time(1, SC_MS));
    wait(20, SC_US);
    check(peer->received().size() == 1 && peer->received()[0].bytes == core::tx_wire_frame(ok, 64),
          "TX_TOO_LONG frame never reaches the line; the next one does");
    check(m32(kTxRing) == 0 && m32(kTxRing + 16) == 0 && m32(kTxRing + 32) == 0x00010000u &&
              m32(kTxRing + 48) == 0,
          "forwarded descriptors OWN = 0, the failing one DES0[16]");
    check((rd(ETH_DMA_TX_STAT) & 0x13) == 0x11 && rd(ETH_DMA_TX_ERRINFO) == 4,
          "ERR set, ERRINFO class 4 (TX_TOO_LONG), engine still running");
    check(rmon(RMON_TX_OVERSIZE) == 1 && rmon(RMON_TX_PKTS) == 1, "RMON_TX_OVERSIZE counts the rejection");
    wr(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_ERR_MASK);
    check((rd(ETH_DMA_TX_STAT) & 0x10) == 0 && rd(ETH_DMA_TX_ERRINFO) == 0, "ERR W1C clears ERRINFO");
    // exactly ETH_TX_MAX_FRAME passes
    peer->received().clear();
    queue_tx(l2(kPeer, kMac, 0x88B5, pattern(1500, 1)));          // 1514 + 4 = 1518
    wr(ETH_DMA_TX_POLL, 1);
    check(wait_peer(1, sc_time(1, SC_MS)) && peer->received()[0].bytes.size() == 1518,
          "a frame of exactly ETH_TX_MAX_FRAME passes");
    hard_reset();

    // ---- CFG_NO_FS: halts, DES0[16], class 6
    bringup();
    tx_desc(0, 0x80000000u, (1u << 31) | 60, kTxBuf);               // LS without FS
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)), "CFG_NO_FS halts the direction");
    check(rd(ETH_DMA_TX_ERRINFO) == 6 && m32(kTxRing) == 0x00010000u && rd(ETH_DMA_TX_CUR_DESC) == 0,
          "CFG_NO_FS: class 6, DES0[16], CUR_DESC at the fault");
    hard_reset();

    // ---- CFG_LEN_ZERO
    bringup();
    tx_desc(0, 0x80000000u, (3u << 30) | 0, kTxBuf);
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 5 &&
              m32(kTxRing) == 0x00010000u,
          "CFG_LEN_ZERO: halted, class 5, DES0[16]");
    hard_reset();

    // ---- TX_DATA_RD_ERR, SLVERR: buffer in the error window
    bringup();
    wr(ETH_INT_EN, ETH_INT_EN_EN_MASK);
    tx_desc(0, 0x80000000u, (3u << 30) | 100, kErrBase + 8);
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 3 &&
              m32(kTxRing) == 0x00020000u,
          "TX_DATA_RD_ERR (SLVERR): class 3, DES0[17]");
    check(rmon(RMON_TX_DROP) == 1 && peer->received().empty(), "RMON_TX_DROP counts it; nothing sent");
    check((rd(ETH_INT_RAW) & ETH_INT_RAW_TX_ERR_MASK) && irq.read() && (rd(ETH_STATUS) & 0x10),
          "TX_ERR interrupt and ETH_STATUS.AXI_ERR_STICKY");
    
    wr(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_ERR_MASK);
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_SRST_MASK);
    check(rd(ETH_DMA_TX_CUR_DESC) == 0, "SRST (HALTED) returns the index to 0");
    for (unsigned i = 0; i < kRingLen; ++i) tx_desc(i, 0, 0, 0);
    tx_head = 0;
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    queue_tx(ok);
    wr(ETH_DMA_TX_POLL, 1);
    check(wait_peer(1, sc_time(1, SC_MS)), "direction recovered after SRST + START");
    hard_reset();

    // ---- TX_DATA_RD_ERR with DECERR: an unmapped buffer
    bringup();
    tx_desc(0, 0x80000000u, (3u << 30) | 100, kUnmapped);
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 0x13 &&
              m32(kTxRing) == 0x00060000u,
          "TX_DATA_RD_ERR (DECERR): ERRINFO 0x13, DES0[18:17]");
    hard_reset();

    // ---- TX_DESC_RD_ERR: ring in unmapped space
    bringup();
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    poll(ETH_DMA_TX_STAT, 0x2, 0x2, sc_time(50, SC_US));
    wr(ETH_DMA_TX_DESC_BASE, kUnmapped);
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 0x11,
          "TX_DESC_RD_ERR (DECERR): ERRINFO 0x11");
    hard_reset();

    // ---- TX_DESC_WR_ERR: descriptor readable, write-back refused (SLVERR);
    // the frame was committed and still goes out, the index is already past it
    bringup();
    ddr->set_write_error_window(kTxRing, 4);
    queue_tx(ok);
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_TX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 2 &&
              rd(ETH_DMA_TX_CUR_DESC) == 1 && m32(kTxRing) == 0x80000000u,
          "TX_DESC_WR_ERR: class 2, CUR_DESC past the descriptor, DES0 unwritten");
    check(wait_peer(1, sc_time(100, SC_US)), "the committed frame still reaches the line");
    ddr->set_write_error_window(0, 0);
    hard_reset();

    // ---- TX_DESC_WR_ERR with HALT_REQ while the write-back is outstanding
    
    bringup();
    ddr->set_write_error_window(kTxRing, 4, sc_time(20, SC_US));
    queue_tx(ok);
    wr(ETH_DMA_TX_POLL, 1);
    wait(5, SC_US);
    check((rd(ETH_DMA_TX_STAT) & 0x16) == 0, "write-back outstanding, not halted, no error yet");
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x37, 0x36, sc_time(50, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 2,
          "TX B error after HALT_REQ: HALTED, IDLE, ERR, DONE, ERRINFO 2");
    ddr->set_write_error_window(0, 0);
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_tx_suspend_halt() {
    bringup();
    wr(ETH_INT_EN, ETH_INT_EN_EN_MASK);
    wait(2, SC_US);
    check((rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_SUSPENDED_MASK) && (rd(ETH_INT_RAW) & 0x10) && irq.read(),
          "empty ring: SUSPENDED, TX_BUF_UNAVAIL interrupt");
    // a descriptor queued without a doorbell is found at the next poll tick
    const Bytes f = l2(kPeer, kMac, 0x88B5, pattern(60, 3));
    queue_tx(f);
    const sc_time t0 = sc_time_stamp();
    check(wait_peer(1, sc_time(200, SC_US)), "descriptor found without POLL");
    check(sc_time_stamp() - t0 < sc_time(10.24 + 8, SC_US), "within one poll interval (10.24 us) plus the frame");
    // an open frame whose tail is not owned parks in SUSPEND; HALT_REQ closes it
    peer->received().clear();
    const unsigned i = tx_head % kRingLen;
    tx_desc(i, 0x80000000u, (1u << 30) | 60, kTxBuf);              // FS, not LS
    tx_desc((i + 1) % kRingLen, 0, (1u << 31) | 60, kTxBuf);       // LS, not owned
    wr(ETH_DMA_TX_POLL, 1);
    wait(5, SC_US);
    check(rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_SUSPENDED_MASK, "open frame parked in SUSPEND");
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x7, 0x6, sc_time(20, SC_US)) && rd(ETH_DMA_TX_ERRINFO) == 0,
          "HALT_REQ in SUSPEND with a frame open: HALTED, no error");
    wait(50, SC_US);
    check(peer->received().empty(), "the aborted partial frame never reaches the line");
    check(m32(kTxRing + 16 * i) == 0, "the forwarded first buffer was written back");
    
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK | ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x7, 0x6, sc_time(20, SC_US)), "START with HALT_REQ: halts at once");
    hard_reset();

    // HALT_REQ while a frame streams completes the buffer, halts at DESC_WB;
    // a second HALT_REQ aborts at once
    bringup();
    const Bytes big = l2(kPeer, kMac, 0x88B5, pattern(1400, 5));
    queue_tx(big, {700, 714});
    wr(ETH_DMA_TX_POLL, 1);
    wait(1, SC_US);
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    wr(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_TX_STAT, 0x7, 0x6, sc_time(50, SC_US)), "second HALT_REQ: halted");
    wait(100, SC_US);
    check(peer->received().empty(), "the force-aborted frame never reaches the line");
    check(m32(kTxRing + 16) == 0x80000000u, "the tail descriptor keeps OWN (software reclaims it)");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_rx() {
    bringup();
    wr(ETH_INT_EN, ETH_INT_EN_EN_MASK);
    EthFrameExt x;
    const Bytes u = l2(kMac, kPeer, 0x0800, ipv4(17, pattern(200, 1), 0));
    Bytes uc = u;
    core::tx_checksum_insert(uc, true, true);
    Bytes bad = uc;
    bad[14 + 20 + 8 + 5] ^= 0x40;                   // payload bit flip: UDP checksum error
    const Bytes t = l2(kMac, kPeer, 0x0800, ipv4(6, pattern(1000, 2), 0));
    Bytes tc = t;
    core::tx_checksum_insert(tc, true, true);
    peer->send(with_fcs(uc), x);
    peer->send(with_fcs(bad), x);
    peer->send(with_fcs(tc), x);
    const uint8_t bc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const Bytes arp = l2(bc, kPeer, 0x0806, pattern(28, 3));      // 42 bytes: runt at the source
    peer->send(core::tx_wire_frame(arp, 64), x);        // padded properly: 64 + 4?
    wait(20, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 4, "four frames stored");
    auto des0 = [&](unsigned i) { return m32(kRxRing + 16 * i); };
    auto buf_eq = [&](unsigned i, const Bytes& f) {
        Bytes b(f.size());
        ddr->read(rx_buf(i), b.data(), b.size());
        uint8_t guard[2];
        ddr->read(rx_buf(i) - 2, guard, 2);
        uint8_t after;
        ddr->read(rx_buf(i) + f.size(), &after, 1);
        return b == f && guard[0] == 0 && guard[1] == 0 && after == 0;
    };
    check(des0(0) == ((0x2Au << 20) | uc.size()) && buf_eq(0, uc),
          "UDP good: DES0 verdict 0x2A, LENGTH, buffer at DES2+2, guards intact");
    check(des0(1) == ((0x2Eu << 20) | bad.size()) && buf_eq(1, bad), "UDP bad: L4_ERR, still delivered");
    check(des0(2) == ((0x29u << 20) | tc.size()) && buf_eq(2, tc), "TCP good: verdict 0x29");
    const Bytes arpw = core::tx_wire_frame(arp, 64);
    const Bytes arpnf(arpw.begin(), arpw.end() - 4);
    check(des0(3) == arpnf.size() && buf_eq(3, arpnf), "broadcast ARP: no verdict, 60 bytes (FCS stripped)");
    check(m32(kRxRing + 12) == 0xFEED0000u && m32(kRxRing + 8) == rx_buf(0), "DES2/DES3 untouched");
    check((rd(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_DONE_MASK) && (rd(ETH_INT_RAW) & 2) && irq.read(),
          "RX_DONE interrupt");
    check(rmon(RMON_RX_PKTS) == 4 && rmon(RMON_RX_BCAST_PKTS) == 1 &&
              rmon(RMON_RX_OCTETS) == uc.size() + bad.size() + tc.size() + 12 + 64 &&
              rmon(RMON_RX_PKTS_128_255) == 2 && rmon(RMON_RX_PKTS_1024_1518) == 1 &&
              rmon(RMON_RX_PKTS_64) == 1 && rmon(RMON_RX_CRC_ALIGN_ERR) == 0,
          "RMON receive counters (checksum errors count nowhere)");
    // checksum verdict off: DES0[25:20] = 0
    rx_dma_cfg(ETH_CSUM_RX_CFG, 0);
    peer->send(with_fcs(uc), x);
    wait(10, SC_US);
    check(des0(4) == uc.size(), "CSUM_RX_EN = 0: DES0[25:20] written 0");
    // L4 off: IPv4 checked only, L4_PROTO still reported
    rx_dma_cfg(ETH_CSUM_RX_CFG, 1);
    peer->send(with_fcs(bad), x);
    wait(10, SC_US);
    check(des0(5) == ((0x22u << 20) | bad.size()), "CSUM_RX_L4_EN = 0: IPV4_CHECKED, proto UDP, no L4 check");
    rx_dma_cfg(ETH_CSUM_RX_CFG, 3);
    check((rd(ETH_STATUS) & ETH_STATUS_RX_BUSY_MASK) == 0, "RX_BUSY clear with the line idle");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_rx_filter_classes() {
    bringup();
    EthFrameExt x;
    auto send = [&](const Bytes& f, bool rx_er = false, bool dribble = false) {
        EthFrameExt e = x;
        e.rx_er = rx_er;
        e.dribble = dribble;
        peer->send(f, e);
        wait(2, SC_US);
    };
    const uint8_t other[6] = {0x02, 0, 0, 0, 0, 0x33};
    const uint8_t mc[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    const uint8_t bc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const Bytes good = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(100, 1)), 64);
    send(with_fcs(l2(other, kPeer, 0x88B5, pattern(100, 1))));    // clean filter miss
    send(with_fcs(l2(mc, kPeer, 0x88B5, pattern(100, 1))));       // multicast, hash off
    rx_cfg(ETH_FILT_CTRL, 5);                                      // BCAST + HASH_EN
    const unsigned h = core::mcast_hash_index(mc);
    rx_cfg(h < 32 ? ETH_HASH_L : ETH_HASH_H, 1u << (h % 32));
    send(with_fcs(l2(mc, kPeer, 0x88B5, pattern(100, 1))));       // hash hit
    rx_cfg(ETH_FILT_CTRL, 0);
    send(with_fcs(l2(bc, kPeer, 0x88B5, pattern(100, 1))));       // broadcast rejected
    rx_cfg(ETH_FILT_CTRL, 1);
    Bytes crc = with_fcs(l2(kMac, kPeer, 0x88B5, pattern(100, 1)));
    crc[20] ^= 1;
    send(crc);                                                     // CRC error
    send(crc, false, true);                                        // alignment (dribble + CRC)
    send(with_fcs(l2(kMac, kPeer, 0x88B5, pattern(40, 1))));      // 58 + 4: undersize
    Bytes frag = with_fcs(l2(kMac, kPeer, 0x88B5, pattern(40, 1)));
    frag[20] ^= 1;
    send(frag);                                                    // fragment
    send(with_fcs(l2(kMac, kPeer, 0x88B5, pattern(1510, 1))));    // 1528: oversize
    Bytes jab = with_fcs(l2(kMac, kPeer, 0x88B5, pattern(1510, 1)));
    jab[30] ^= 1;
    send(jab);                                                     // jabber
    send(good, true);                                              // rx_er, good FCS
    Bytes miss_crc = with_fcs(l2(other, kPeer, 0x88B5, pattern(100, 1)));
    miss_crc[20] ^= 1;
    send(miss_crc);                                                // filter miss + CRC: counted in full
    send(good, false, true);                                       // dribble with good FCS: committed
    wait(20, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 2, "only the hash hit and the dribbling good frame are stored");
    check(rmon(RMON_RX_FILTER_DROP) == 3, "clean misses: RMON_RX_FILTER_DROP only");
    check(rmon(RMON_RX_MCAST_PKTS) == 1 && rmon(RMON_RX_BCAST_PKTS) == 0, "multicast counted when accepted");
    check(rmon(RMON_RX_CRC_ALIGN_ERR) == 3, "CRC, alignment and the untrusted miss: CRC_ALIGN_ERR");
    check(rmon(RMON_RX_UNDERSIZE) == 1 && rmon(RMON_RX_FRAGMENTS) == 1, "undersize and fragment");
    check(rmon(RMON_RX_OVERSIZE) == 1 && rmon(RMON_RX_JABBERS) == 1, "oversize and jabber");
    check(rmon(RMON_RX_PKTS) == 10 && rmon(RMON_RX_COLLISIONS) == 0, "RMON_RX_PKTS counts every non-miss frame");
    check(rmon(RMON_RX_PKTS_65_127) == 6 && rmon(RMON_RX_PKTS_64) == 0,
          "size bins: none outside 64..1518, rx_er frame binned");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_rx_resources() {
    EthFrameExt x;
    const Bytes f = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(200, 4)), 64);
    // ---- no descriptor: DISCARD, drop counters, RX_BUF_UNAVAIL; re-armed by POLL
    bringup();
    wr(ETH_INT_EN, ETH_INT_EN_EN_MASK);
    for (unsigned i = 0; i < kRingLen; ++i) rx_desc(i, i < 2, rx_buf(i));
    for (int k = 0; k < 4; ++k) peer->send(f, x);
    wait(10, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 2 && rd(ETH_DMA_RX_DROP_CNT) == 2,
          "two stored, two discarded for lack of a descriptor");
    check(rmon(RMON_RX_DROP_EVENTS) == 2 && rmon(RMON_RX_PKTS) == 4, "DropEvents counts them; the MAC counted all");
    check((rd(ETH_DMA_RX_STAT) & 0x9) == 0x9 && (rd(ETH_INT_RAW) & 0x20) && irq.read(),
          "SUSPENDED and RX_BUF_UNAVAIL");
    const uint8_t no_be[4] = {0, 0, 0, 0};
    uint32_t z = 0;
    xfer(tlm::TLM_WRITE_COMMAND, ETH_DMA_RX_DROP_CNT, &z, nullptr, no_be, 4);
    check(rd(ETH_DMA_RX_DROP_CNT) == 2, "WC: a write with no strobed byte keeps the count");
    wr(ETH_DMA_RX_DROP_CNT, 0x1234);
    check(rd(ETH_DMA_RX_DROP_CNT) == 0, "WC: any strobed write clears it");
    rx_desc(2, true, rx_buf(2));
    wr(ETH_DMA_RX_POLL, 1);
    wait(1, SC_US);
    check((rd(ETH_DMA_RX_STAT) & 0x8) == 0 && (rd(ETH_INT_RAW) & 0x20) == 0, "POLL re-arms: RX_BUF_UNAVAIL clears");
    peer->send(f, x);
    wait(10, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 3, "the next frame is stored");
    // without a doorbell: the poll interval re-fetches even while frames keep arriving
    rx_desc(3, true, rx_buf(3));
    peer->send(f, x);
    peer->send(f, x);
    wait(20, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 4, "poll tick re-arms without POLL within 10.24 us");
    hard_reset();

    // ---- RX_TRUNC: BUF_SIZE 100 against a 216-byte frame
    bringup();
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    poll(ETH_DMA_RX_STAT, 0x2, 0x2, sc_time(20, SC_US));
    wr(ETH_DMA_RX_BUF_SIZE, 100);
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    peer->send(f, x);
    peer->send(f, x);
    wait(10, SC_US);
    uint8_t after = 0xAA;
    ddr->read(rx_buf(0) + 100, &after, 1);
    check(m32(kRxRing) == (0x00010000u | 100) && after == 0, "RX_TRUNC: DES0[16], LENGTH = BUF_SIZE, no overrun");
    check((rd(ETH_DMA_RX_STAT) & 0x11) == 0x11 && rd(ETH_DMA_RX_ERRINFO) == 4 && rd(ETH_DMA_RX_CUR_DESC) == 2,
          "RX_TRUNC: ERR, class 4, the engine continues");
    hard_reset();

    // ---- RX FIFO overflow: DMA not started, 8 KiB fills
    bringup();
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    const Bytes big = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(1500, 4)), 64);
    for (int k = 0; k < 7; ++k) peer->send(big, x);               // 7 x 1514 > 8192
    wait(5, SC_US);
    check((rd(ETH_FIFO_STAT) & 1) && rmon(RMON_RX_DROP_EVENTS) == 2 && rmon(RMON_RX_PKTS) == 7,
          "RX FIFO full: RXFIFO_OVF, two frames dropped, all seven counted by the MAC");
    wr(ETH_FIFO_STAT, 1);
    check((rd(ETH_FIFO_STAT) & 1) == 0, "FIFO_STAT W1C");
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    wait(50, SC_US);
    check(rd(ETH_DMA_RX_CUR_DESC) == 5, "restarted: the five buffered frames are delivered");
    hard_reset();

    // ---- RX_EN cleared mid-frame: no RMON event, nothing stored
    bringup();
    const Bytes lng = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(1400, 4)), 64);
    send_frame(peer->line_tx, lng, x);
    wait(20, SC_US);
    check(rd(ETH_STATUS) & ETH_STATUS_RX_BUSY_MASK, "RX_BUSY while a frame arrives");
    wr(ETH_RX_MAC_CFG, 0);
    wait(200, SC_US);
    check(rmon(RMON_RX_PKTS) == 0 && rd(ETH_DMA_RX_CUR_DESC) == 0 && (rd(ETH_STATUS) & 2) == 0,
          "RX_EN cleared mid-frame: aborted, no event");
    // a frame at the wrong line rate is not received
    wr(ETH_RX_MAC_CFG, 1);
    EthFrameExt slow;
    slow.speed100 = false;
    peer->send(f, slow);
    wait(10, SC_US);
    check(rmon(RMON_RX_PKTS) == 0, "a 10 Mbps frame at SPEED = 100 is not received");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_rx_errors() {
    EthFrameExt x;
    const Bytes f = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(200, 4)), 64);
    const uint32_t n = uint32_t(f.size() - 4);
    // RX_DATA_WR_ERR (SLVERR): the held descriptor written back with DES0[17]
    bringup();
    rx_desc(0, true, kErrBase + 2);
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    poll(ETH_DMA_RX_STAT, 0x2, 0x2, sc_time(20, SC_US));
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    peer->send(f, x);
    // LENGTH is undefined with DES0[17] set (MAS Table 6-29 row 7): not checked
    (void)n;
    check(poll(ETH_DMA_RX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_RX_ERRINFO) == 3 &&
              (m32(kRxRing) & 0xFFFF0000u) == 0x00020000u && rd(ETH_DMA_RX_CUR_DESC) == 0,
          "RX_DATA_WR_ERR: halted, class 3, DES0[17], CUR_DESC at the fault");
    hard_reset();
    // RX_DESC_RD_ERR (DECERR)
    bringup();
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    poll(ETH_DMA_RX_STAT, 0x2, 0x2, sc_time(20, SC_US));
    wr(ETH_DMA_RX_DESC_BASE, kUnmapped);
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    check(poll(ETH_DMA_RX_STAT, 0x3, 0x2, sc_time(20, SC_US)) && rd(ETH_DMA_RX_ERRINFO) == 0x11,
          "RX_DESC_RD_ERR (DECERR): ERRINFO 0x11");
    hard_reset();
    // RX_DESC_WR_ERR: the frame is in memory, the write-back refused
    bringup();
    ddr->set_write_error_window(kRxRing, 4);
    peer->send(f, x);
    check(poll(ETH_DMA_RX_STAT, 0x3, 0x2, sc_time(50, SC_US)) && rd(ETH_DMA_RX_ERRINFO) == 2 &&
              rd(ETH_DMA_RX_CUR_DESC) == 1 && m32(kRxRing) == 0x80000000u,
          "RX_DESC_WR_ERR: class 2, CUR_DESC past the descriptor");
    ddr->set_write_error_window(0, 0);
    check((rd(ETH_DMA_TX_STAT) & 0x1) == 1, "a receive fault leaves transmit running");
    hard_reset();
    
    bringup();
    ddr->set_write_error_window(kRxRing, 4, sc_time(100, SC_US));
    peer->send(f, x);
    wait(f.size() * 80 + 5000, SC_NS);                   // the frame is in, its write-back held
    check((rd(ETH_DMA_RX_STAT) & 0x16) == 0, "RX write-back outstanding, no error yet");
    wr(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    check(poll(ETH_DMA_RX_STAT, 0x17, 0x16, sc_time(200, SC_US)) && rd(ETH_DMA_RX_ERRINFO) == 2,
          "RX B error after HALT_REQ: HALTED, IDLE, ERR, ERRINFO 2");
    ddr->set_write_error_window(0, 0);
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_coalescing() {
    bringup(true, 3);                               // COAL_CNT 3, no timer
    EthFrameExt x;
    const Bytes f = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(60, 4)), 64);
    peer->send(f, x);
    peer->send(f, x);
    wait(5, SC_US);
    check((rd(ETH_DMA_RX_STAT) & 0x20) == 0, "COAL_CNT 3: no DONE after two frames");
    peer->send(f, x);
    wait(5, SC_US);
    check(rd(ETH_DMA_RX_STAT) & 0x20, "DONE on the third frame");
    wr(ETH_DMA_RX_STAT, 0x20);
    // timer: COAL_TIMER 10 (1.28 us x 10 = 12.8 us) after the last frame
    wr(ETH_DMA_RX_COAL, (10u << 8) | 8);
    peer->send(f, x);
    const sc_time t0 = sc_time_stamp();
    check(poll(ETH_DMA_RX_STAT, 0x20, 0x20, sc_time(40, SC_US)), "timer expiry reports one frame");
    const sc_time dt = sc_time_stamp() - t0;
    check(dt >= sc_time(12.8, SC_US) && dt < sc_time(14, SC_US),
          "DONE 12.8 us after the frame (" + dt.to_string() + ")");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_rmon_modes() {
    bringup();
    EthFrameExt x;
    const Bytes f = core::tx_wire_frame(l2(kMac, kPeer, 0x88B5, pattern(60, 4)), 64);
    peer->send(f, x);
    wait(5, SC_US);
    // debug reads never clear
    tlm::tlm_generic_payload t;
    uint8_t buf[4];
    t.set_command(tlm::TLM_READ_COMMAND);
    t.set_address(RMON_RX_PKTS);
    t.set_data_ptr(buf);
    t.set_data_length(4);
    t.set_streaming_width(4);
    wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_READ_CLEAR_EN_MASK);
    sock->transport_dbg(t);
    check(buf[0] == 1 && rd(RMON_RX_PKTS) == 1, "read-clear: returns the count ...");
    check(rd(RMON_RX_PKTS) == 0, "... and clears it; a debug read does not");
    wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_FREEZE_MASK);
    peer->send(f, x);
    wait(5, SC_US);
    check(rd(RMON_RX_PKTS) == 0 && rd(RMON_RX_OCTETS) == f.size(), "FREEZE: events discarded, counters hold");
    wr(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_CLEAR_ALL_MASK);
    check(rd(RMON_RX_OCTETS) == 0 && (rd(RMON_CTRL) & RMON_CTRL_CLEAR_ALL_MASK) == 0,
          "CLEAR_ALL zeroes every counter, reads 0");
    wr(RMON_CTRL, 0);
    peer->send(f, x);
    wait(5, SC_US);
    check(rd(RMON_RX_PKTS) == 0, "RMON_EN = 0: nothing counts");
    wr(RMON_CTRL, 1);
    peer->send(f, x);
    wait(5, SC_US);
    wr(ETH_CTRL, 0xF7FF);                           // RMON held in reset
    check(rd(RMON_RX_PKTS) == 0, "RMON held in reset: counter reads 0");
    wr(ETH_CTRL, 0x3FFF);
    check(rd(RMON_RX_PKTS) == 0, "... and was cleared by the reset");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::test_resets() {
    bringup();
    const Bytes big = l2(kPeer, kMac, 0x88B5, pattern(1400, 5));
    queue_tx(big);
    wr(ETH_DMA_TX_POLL, 1);
    wait(2, SC_US);
    wr(ETH_CTRL, 0xFFEF);                           // DMA reset while it reads the frame
    check((rd(ETH_DMA_TX_STAT) & 0x7) == 0x4 && rd(ETH_DMA_TX_CUR_DESC) == 0,
          "ETH_CTRL[4] mid-frame: DMA idle, index 0, HALTED 0");
    wait(200, SC_US);
    check(peer->received().empty(), "nothing of the reset frame reaches the line");
    wr(ETH_CTRL, 0x3FFF);
    
    rx_cfg(ETH_HASH_L, 0x1234);
    check(rd(ETH_HASH_L) == 0x1234, "ETH_HASH_L written with the Rx MAC disabled");
    rst_n.write(false);
    wait(20, SC_NS);
    uint32_t v = 0;
    check(xfer(tlm::TLM_WRITE_COMMAND, ETH_HASH_L, &v) == tlm::TLM_OK_RESPONSE, "access during reset answers");
    rst_n.write(true);
    wait(20, SC_NS);
    check(rd(ETH_HASH_L) == 0 && rd(ETH_CTRL) == 0 && (rd(ETH_DMA_RX_STAT) & 0x3F) == 0x4,
          "rst_n resets the CSR and the engines");
    hard_reset();
    // TX path group (MAS 6.2): ETH_CTRL[2] also resets both TX FIFOs
    bringup();
    wr(ETH_TX_MAC_CFG, 0);                          // TX_EN = 0: frames wait in the TX FIFO
    queue_tx(l2(kPeer, kMac, 0x88B5, pattern(200, 6)));
    queue_tx(l2(kPeer, kMac, 0x88B5, pattern(300, 7)));
    wr(ETH_DMA_TX_POLL, 1);
    wait(50, SC_US);
    wr(ETH_CTRL, 0x3FFB);                           // Tx Checksum reset pulse
    wr(ETH_CTRL, 0x3FFF);
    wr(ETH_TX_MAC_CFG, 1);
    wait(200, SC_US);
    check(peer->received().empty(), "ETH_CTRL[2] empties the TX FIFO: queued frames are lost");
    const Bytes next = l2(kPeer, kMac, 0x88B5, pattern(100, 8));
    queue_tx(next);
    wr(ETH_DMA_TX_POLL, 1);
    check(wait_peer(1, sc_time(200, SC_US)) && peer->received().size() == 1 &&
              peer->received()[0].bytes == core::tx_wire_frame(next, 64),
          "... and the next frame is sent after release");
    hard_reset();
    // Exercise each path-group bit with a claimed frame, both during IPG
    
    for (unsigned bit : {2u, 5u, 12u}) {
        for (bool on_wire : {false, true}) {
            bringup();
            wr(ETH_TX_MAC_CFG, 0);
            wr(ETH_IPG, 255);                       // ample time to reset during IPG
            queue_tx(big);
            wr(ETH_DMA_TX_POLL, 1);
            wait(50, SC_US);                       // DMA commits while MAC disabled
            wr(ETH_TX_MAC_CFG, 1);
            check(poll(ETH_STATUS, ETH_STATUS_TX_BUSY_MASK, ETH_STATUS_TX_BUSY_MASK,
                       sc_time(5, SC_US)), "TX claims frame before path reset");
            if (on_wire)
                check(wait_peer(1, sc_time(30, SC_US)), "frame starts before path reset");
            const std::string tag = "ETH_CTRL[" + std::to_string(bit) + "] " +
                                    (on_wire ? "during frame" : "during IPG");
            wr(ETH_CTRL, 0x3FFFu & ~(1u << bit));
            check((rd(ETH_STATUS) & ETH_STATUS_TX_BUSY_MASK) == 0, tag + ": clears BUSY");
            wait(150, SC_US);                      // past the cancelled completion
            check(rd(RMON_TX_PKTS) == 0, tag + ": no completion counted");
            check(peer->received().size() == (on_wire ? 1u : 0u),
                  tag + ": no additional PHY transaction");
            // A frame-level PHY cannot retract a transaction already delivered.
            peer->received().clear();
            wr(ETH_CTRL, 0x3FFF);
            queue_tx(next);
            wr(ETH_DMA_TX_POLL, 1);
            check(wait_peer(1, sc_time(200, SC_US)) &&
                      peer->received().back().bytes == core::tx_wire_frame(next, 64) &&
                      line_idle(sc_time(200, SC_US)) && rd(RMON_TX_PKTS) == 1,
                  tag + ": next frame completes after release");
            hard_reset();
        }
    }
}

// -----------------------------------------------------------------------------
void Tb::test_loopback_10m() {
    // PHY loopback at 10 Mbps, auto-negotiation off: the frame comes back
    bringup(false);
    wr(ETH_MDIO_DATA, 0x4100);                      // loopback, 10 Mbps, full duplex
    wr(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | ETH_MDIO_CTRL_OP_MASK | (1u << 7) | (40u << 12));
    poll(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, sc_time(40, SC_US));
    const Bytes f = l2(kMac, kPeer, 0x0800, ipv4(17, pattern(100, 6), 0));
    queue_tx(f);
    const sc_time t0 = sc_time_stamp();
    wr(ETH_DMA_TX_POLL, 1);
    check(poll(ETH_DMA_RX_CUR_DESC, 0xFFFF, 1, sc_time(1, SC_MS)), "loopback frame received");
    Bytes e = f;
    core::tx_checksum_insert(e, true, true);
    Bytes b(e.size());
    ddr->read(rx_buf(0), b.data(), b.size());
    check(b == e && m32(kRxRing) == ((0x2Au << 20) | e.size()), "looped-back frame intact, checksums verified");
    // 10 Mbps: (12 IPG + 8 + 146) bytes x 800 ns at least
    check(sc_time_stamp() - t0 >= sc_time((12 + 8 + 146) * 0.8, SC_US), "10 Mbps wire time");
    check(peer->received().empty(), "loopback keeps the frame off the line");
    hard_reset();
}

// -----------------------------------------------------------------------------
void Tb::run() {
    std::printf("=== eth_mac_tlm unit test ===\n");
    wait(20, SC_NS);
    rst_n.write(true);
    rmii_rst_n.write(true);
    wait(20, SC_NS);
    test_registers();
    test_errors_and_timing();
    test_interrupt_and_ctrl();
    test_mdio();
    test_tx();
    test_tx_timing();
    test_tx_errors();
    test_tx_suspend_halt();
    test_rx();
    test_rx_filter_classes();
    test_rx_resources();
    test_rx_errors();
    test_coalescing();
    test_rmon_modes();
    test_resets();
    test_loopback_10m();
    std::printf("=== eth_mac_tlm: %d checks, %d failures ===\n", g_pass + g_fail, g_fail);
    sc_stop();
}

}  // namespace

int sc_main(int, char**) {
    Tb tb("tb");
    vp::BusRouter bus("bus", 1, {{"ddr", 0, kDdrSize}});
    vp::MemoryTlm ddr("ddr", kDdrSize, sc_time(50, SC_NS), sc_time(10, SC_NS), 4);
    EthMacTlm mac("mac");
    icsl_eth::EthPhyTlm phy("phy", 1);
    icsl_eth::EthLinkPartner peer("peer");
    tb.sock.bind(mac.csr);
    mac.axi.bind(bus.initiator_port(0));
    bus.target_port("ddr").bind(ddr.socket);
    mac.phy_tx.bind(phy.mac_tx);
    phy.mac_rx.bind(mac.phy_rx);
    mac.mdio.bind(phy.mdio);
    phy.line_tx.bind(peer.line_rx);
    peer.line_tx.bind(phy.line_rx);
    mac.rst_n(tb.rst_n);
    mac.rmii_rst_n(tb.rmii_rst_n);
    mac.irq(tb.irq);
    ddr.set_error_window(kErrBase, kErrSize);
    tb.mac = &mac;
    tb.ddr = &ddr;
    tb.phy = &phy;
    tb.peer = &peer;
    sc_start();
    return g_fail == 0 ? 0 : 1;
}
