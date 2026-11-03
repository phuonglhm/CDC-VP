// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.platforms.eth_soc.eth_node
// Description: One network node: host port, router, DDR, MAC, PHY.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
//   host program (SC_THREAD) --+
//   MAC axi (DMA) -------------+-- router --+-- DDR   0x0000_0000  64 MiB
//                                           +-- ETH   0x4000_0000  64 KiB (local offsets)
//   MAC phy_tx/phy_rx/mdio <-> PHY (MDIO address 1) <-> line_tx / line_rx
//   MAC irq (level) -> the host program's wait_irq()
//
// The node gives the bare-metal driver (sw/eth_drv.h) its HAL: register and
// memory accesses become b_transport calls from the host thread, whose
// annotated delays it waits out, so the software's time is on the same clock
// as the hardware's.  DRAM is at 0 in the node's map, so the IP's buffer
// offsets and the host's addresses coincide (dram_base = 0).
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "eth_drv.h"
#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"
#include "vp/bus_router.h"
#include "vp/memory_tlm.h"

namespace eth_soc {

constexpr uint64_t kDdrBase = 0x0000'0000, kDdrSize = 0x400'0000;
constexpr uint64_t kEthBase = 0x4000'0000, kEthSize = 0x1'0000;

struct EthNode : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<EthNode> host;
    vp::BusRouter bus;
    vp::MemoryTlm ddr;
    icsl_eth::EthMacTlm mac;
    icsl_eth::EthPhyTlm phy;
    sc_core::sc_signal<bool> rst_n{"rst_n", false};
    sc_core::sc_signal<bool> irq{"irq", false};
    bool trace = false;
    uint64_t mmio = 0;                              // register accesses by the host

    SC_HAS_PROCESS(EthNode);
    EthNode(sc_core::sc_module_name n, const icsl_eth::EthMacParams& p = icsl_eth::EthMacParams())
        : sc_module(n),
          host("host"),
          bus("bus", 2, {{"ddr", kDdrBase, kDdrSize}, {"eth", kEthBase, kEthSize}}),
          ddr("ddr", kDdrSize, sc_core::sc_time(60, sc_core::SC_NS), p.clk_period, p.axi_data_bytes),
          mac("mac", p),
          phy("phy", 1) {
        host.bind(bus.initiator_port(0));
        mac.axi.bind(bus.initiator_port(1));
        bus.target_port("ddr").bind(ddr.socket);
        bus.target_port("eth").bind(mac.csr);
        mac.phy_tx.bind(phy.mac_tx);
        phy.mac_rx.bind(mac.phy_rx);
        mac.mdio.bind(phy.mdio);
        mac.rst_n(rst_n);
        mac.rmii_rst_n(rst_n);                      // one board reset for both domains
        mac.irq(irq);
        SC_THREAD(power_on_reset);
    }

    // the driver's HAL, bound to this node
    eth_hal_t hal() {
        eth_hal_t h;
        h.ctx = this;
        h.rd = [](void* c, uint32_t off) { return static_cast<EthNode*>(c)->reg_rd(off); };
        h.wr = [](void* c, uint32_t off, uint32_t v) { static_cast<EthNode*>(c)->reg_wr(off, v); };
        h.mem_wr = [](void* c, uint32_t a, const void* s, uint32_t n) {
            static_cast<EthNode*>(c)->access(tlm::TLM_WRITE_COMMAND, kDdrBase + a, const_cast<void*>(s), n);
        };
        h.mem_rd = [](void* c, uint32_t a, void* d, uint32_t n) {
            static_cast<EthNode*>(c)->access(tlm::TLM_READ_COMMAND, kDdrBase + a, d, n);
        };
        h.wait_irq = [](void* c, uint32_t us) { return static_cast<EthNode*>(c)->wait_irq(us); };
        h.delay_us = [](void*, uint32_t us) { sc_core::wait(double(us), sc_core::SC_US); };
        return h;
    }

    uint32_t reg_rd(uint32_t off) {
        uint32_t v = 0;
        access(tlm::TLM_READ_COMMAND, kEthBase + off, &v, 4);
        if (trace) std::printf("[%s] %s rd %04X = %08X\n", name(), sc_core::sc_time_stamp().to_string().c_str(), off, v);
        return v;
    }
    void reg_wr(uint32_t off, uint32_t v) {
        if (trace) std::printf("[%s] %s wr %04X = %08X\n", name(), sc_core::sc_time_stamp().to_string().c_str(), off, v);
        access(tlm::TLM_WRITE_COMMAND, kEthBase + off, &v, 4);
    }
    int wait_irq(uint32_t us) {
        if (irq.read()) return 1;
        sc_core::wait(sc_core::sc_time(double(us), sc_core::SC_US), irq.posedge_event());
        return irq.read() ? 1 : 0;
    }

private:
    void access(tlm::tlm_command cmd, uint64_t a, void* p, uint32_t n) {
        tlm::tlm_generic_payload t;
        t.set_command(cmd);
        t.set_address(a);
        t.set_data_ptr(static_cast<unsigned char*>(p));
        t.set_data_length(n);
        t.set_streaming_width(n);
        t.set_byte_enable_ptr(nullptr);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        host->b_transport(t, d);
        if (t.is_response_error())
            SC_REPORT_ERROR(name(), ("bus error at " + std::to_string(a)).c_str());
        if (a >= kEthBase) ++mmio;
        sc_core::wait(d);
    }
    // a real reset pulse, not a line held high from elaboration
    void power_on_reset() {
        rst_n.write(false);
        sc_core::wait(100, sc_core::SC_NS);
        rst_n.write(true);
    }
};

}  // namespace eth_soc
