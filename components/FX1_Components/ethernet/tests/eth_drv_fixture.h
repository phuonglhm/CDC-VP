// =============================================================================
// Module: virtual_platform.eth_mac_tlm.tests.eth_drv_fixture
// Description: Bench of the driver tests: the MAC model, PHY and link partner
//              on a bus with DDR, a HAL for the portable driver through TLM
//              (each CSR/memory access waits out its bus time), and a write
//              delayer in front of memory to hold DES0 write-backs (F08).
//              Shared by test_eth_drv_lifecycle (portable driver) and
//              platforms/fx1_soc/tests/test_eth_manager (FX1 manager).
// Changelog:
//   2026-11-03: Taken out of test_eth_drv_lifecycle.cpp (review ER01-ER04).
// =============================================================================
#pragma once
#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "eth_drv.h"
#include "eth_regs.h"
#include "icsl_eth/eth_core.h"
#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"
#include "vp/bus_router.h"
#include "vp/memory_tlm.h"

namespace ethfix {

using namespace sc_core;
using Bytes = std::vector<uint8_t>;

inline int g_fail = 0, g_pass = 0;

inline void check(bool ok, const std::string& what) {
    ++(ok ? g_pass : g_fail);
    std::printf("[%s] %s @ %s\n", ok ? "PASS" : "FAIL", what.c_str(), sc_time_stamp().to_string().c_str());
}

inline constexpr uint64_t kDdrSize = 0x400'0000, kEthBase = 0x4000'0000;
inline constexpr uint32_t kTxRing = 0x1'0000, kRxRing = 0x2'0000, kTxBuf = 0x10'0000, kRxBuf = 0x20'0000;
inline constexpr uint32_t kStride = 0x800, kLead = 2, kRing = 8;
inline constexpr uint32_t kUnmapped = 0x800'0000;          // above the 64 MiB DDR: DECERR
inline const uint8_t kMac[6] = {0x02, 0, 0, 0, 0, 0x01};
inline const uint8_t kPeer[6] = {0x02, 0, 0, 0, 0, 0x02};

inline Bytes frame(size_t len, uint32_t tag, const uint8_t* da = kMac, const uint8_t* sa = kPeer) {
    Bytes f(da, da + 6);
    f.insert(f.end(), sa, sa + 6);
    f.push_back(0x88);
    f.push_back(0xB5);
    for (int i = 0; i < 4; ++i) f.push_back(uint8_t(tag >> (8 * i)));     // the tag: bytes 14..17
    while (f.size() < len) f.push_back(uint8_t(f.size() * 3 + tag));
    return f;
}
inline uint32_t tag_of(const Bytes& b) { return b.size() < 18 ? 0 : b[14] | b[15] << 8 | b[16] << 16 | uint32_t(b[17]) << 24; }

// Holds every write into [lo, hi) for `delay` before it reaches memory: the
// DES0 write-back of the MAC is issued (DONE counted) but not yet visible.
// With `delay_of` set, each such write is held for what it returns instead.
struct SlowWb : sc_module {
    tlm_utils::simple_target_socket<SlowWb> target{"target"};
    tlm_utils::simple_initiator_socket<SlowWb> out{"out"};
    uint64_t lo = 0, hi = 0;
    sc_time delay = SC_ZERO_TIME;
    std::function<sc_time(uint64_t addr)> delay_of;
    // the next `err_left` writes into [err_lo, err_hi) answer SLVERR, memory untouched
    uint64_t err_lo = 0, err_hi = 0;
    unsigned err_left = 0;
    explicit SlowWb(sc_module_name n) : sc_module(n) { target.register_b_transport(this, &SlowWb::b_transport); }
    void b_transport(tlm::tlm_generic_payload& t, sc_time& d) {
        if (t.is_write() && t.get_address() >= lo && t.get_address() < hi) {
            const sc_time hold = delay_of ? delay_of(t.get_address()) : delay;
            if (hold != SC_ZERO_TIME) wait(hold);
        }
        if (t.is_write() && err_left && t.get_address() >= err_lo && t.get_address() < err_hi) {
            --err_left;
            t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        out->b_transport(t, d);
    }
};

struct Node : sc_module {
    tlm_utils::simple_initiator_socket<Node> host{"host"};
    vp::BusRouter bus;
    vp::MemoryTlm ddr;
    SlowWb slow{"slow"};
    icsl_eth::EthMacTlm mac;
    icsl_eth::EthPhyTlm phy;
    icsl_eth::EthLinkPartner peer;
    sc_signal<bool> rst_n{"rst_n", false};
    sc_signal<bool> irq{"irq", false};
    uint64_t mmio = 0, mem_writes = 0;
    // accesses take no simulated time: an interrupt handler that the owner
    // thread must not interleave with (test_eth_manager)
    bool atomic = false;
    bool bus_error = false;

    explicit Node(sc_module_name n)
        : sc_module(n), bus("bus", 2, {{"ddr", 0, kDdrSize}, {"eth", kEthBase, 0x10000}}),
          ddr("ddr", kDdrSize, sc_time(60, SC_NS), sc_time(10, SC_NS), 4), mac("mac"), phy("phy", 1), peer("peer") {
        host.bind(bus.initiator_port(0));
        mac.axi.bind(slow.target);
        slow.out.bind(bus.initiator_port(1));
        bus.target_port("ddr").bind(ddr.socket);
        bus.target_port("eth").bind(mac.csr);
        mac.phy_tx.bind(phy.mac_tx);
        phy.mac_rx.bind(mac.phy_rx);
        mac.mdio.bind(phy.mdio);
        phy.line_tx.bind(peer.line_rx);
        peer.line_tx.bind(phy.line_rx);
        mac.rst_n(rst_n);
        mac.rmii_rst_n(rst_n);
        mac.irq(irq);
    }
    void access(tlm::tlm_command cmd, uint64_t a, void* p, uint32_t n) {
        tlm::tlm_generic_payload t;
        t.set_command(cmd);
        t.set_address(a);
        t.set_data_ptr(static_cast<unsigned char*>(p));
        t.set_data_length(n);
        t.set_streaming_width(n);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d = SC_ZERO_TIME;
        host->b_transport(t, d);
        if (t.is_response_error()) bus_error = true;
        if (a >= kEthBase) ++mmio;
        else if (cmd == tlm::TLM_WRITE_COMMAND) ++mem_writes;
        if (!atomic) wait(d);
    }
    eth_hal_t hal() {
        eth_hal_t h;
        h.ctx = this;
        h.rd = [](void* c, uint32_t off) {
            uint32_t v = 0;
            static_cast<Node*>(c)->access(tlm::TLM_READ_COMMAND, kEthBase + off, &v, 4);
            return v;
        };
        h.wr = [](void* c, uint32_t off, uint32_t v) {
            static_cast<Node*>(c)->access(tlm::TLM_WRITE_COMMAND, kEthBase + off, &v, 4);
        };
        h.mem_wr = [](void* c, uint32_t a, const void* s, uint32_t n) {
            static_cast<Node*>(c)->access(tlm::TLM_WRITE_COMMAND, a, const_cast<void*>(s), n);
        };
        h.mem_rd = [](void* c, uint32_t a, void* d, uint32_t n) {
            static_cast<Node*>(c)->access(tlm::TLM_READ_COMMAND, a, d, n);
        };
        h.wait_irq = [](void* c, uint32_t us) {
            Node* nd = static_cast<Node*>(c);
            if (!nd->irq.read()) sc_core::wait(sc_time(double(us), SC_US), nd->irq.posedge_event());
            return nd->irq.read() ? 1 : 0;
        };
        h.delay_us = [](void*, uint32_t us) { sc_core::wait(double(us), SC_US); };
        return h;
    }
    uint32_t rd(uint32_t off) {
        uint32_t v = 0;
        access(tlm::TLM_READ_COMMAND, kEthBase + off, &v, 4);
        return v;
    }
    void wr(uint32_t off, uint32_t v) { access(tlm::TLM_WRITE_COMMAND, kEthBase + off, &v, 4); }
    void reset() {
        rst_n.write(false);
        wait(100, SC_NS);
        rst_n.write(true);
        wait(100, SC_NS);
        peer.received().clear();
        slow.delay = SC_ZERO_TIME;
        slow.delay_of = nullptr;
        slow.err_left = 0;
    }
    void inject(const Bytes& f, bool s100 = true) {
        Bytes w = f;
        const auto c = icsl_eth::core::fcs(w.data(), w.size());
        w.insert(w.end(), c.begin(), c.end());
        icsl_eth::EthFrameExt x;
        x.speed100 = s100;
        icsl_eth::EthLinkPartner* p = &peer;
        sc_spawn([p, w, x]() { p->send(w, x); });
    }
};

inline eth_cfg_t base_cfg(bool s100 = true) {
    eth_cfg_t c;
    std::memset(&c, 0, sizeof c);
    std::memcpy(c.mac, kMac, 6);
    c.speed100 = s100;
    c.phy_addr = 1;
    c.tx_ring = kTxRing;
    c.rx_ring = kRxRing;
    c.tx_buf = kTxBuf;
    c.rx_buf = kRxBuf;
    c.ring_len = kRing;
    c.buf_stride = kStride;
    c.rx_lead = kLead;
    c.int_en = ETH_INT_RAW_TX_DONE_MASK | ETH_INT_RAW_RX_DONE_MASK | ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK;
    c.filt_ctrl = ETH_FILT_CTRL_BCAST_ACCEPT_MASK;
    c.dma_base = 0;
    c.dma_size = uint32_t(kDdrSize);
    return c;
}

// frames received by the driver, by tag
inline std::vector<uint32_t> drain_rx(eth_dev_t& d) {
    std::vector<uint32_t> tags;
    uint8_t buf[1600];
    uint32_t des0;
    int n;
    while ((n = eth_recv(&d, buf, sizeof buf, &des0)) > 0) tags.push_back(tag_of(Bytes(buf, buf + n)));
    return tags;
}

inline bool wait_for(const std::function<bool()>& f, sc_time limit) {
    const sc_time end = sc_time_stamp() + limit;
    while (!f() && sc_time_stamp() < end) wait(1, SC_US);
    return f();
}

inline bool quiescent(Node& n) {
    const uint32_t st = n.rd(ETH_STATUS);
    return (n.rd(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_HALTED_MASK) && (n.rd(ETH_DMA_RX_STAT) & ETH_DMA_RX_STAT_HALTED_MASK) &&
           !(st & (ETH_STATUS_TX_BUSY_MASK | ETH_STATUS_RX_BUSY_MASK)) && n.rd(ETH_TX_MAC_CFG) == 0 &&
           n.rd(ETH_RX_MAC_CFG) == 0 && n.rd(ETH_INT_EN) == 0;
}

}  // namespace ethfix
