// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.cdc_vp.riscv_eth_soc
// Description: RV32 SoC with the Ethernet MAC, built from CDC-VP components (Linux).
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
//   riscv_eth_soc --fw firmware.elf [--sim-ms N] [--quantum-ns N]
//
//   Bremen RV32 ISS (CDC-VP riscv_vp) --+
//   ETH axi (DMA) ----------------------+-- CDC-VP bus_router --+-- DDR       0x0000_0000 64 MiB
//                                                               +-- ETH CSR   0x4000_0000 64 KiB
//                                                               +-- UART      0x4001_0000
//                                                               +-- SIM_CTRL  0x4002_0000
//                                                               +-- PLIC      0x4C00_0000 (CDC-VP)
//                                                               +-- RAM       0x8000_0000 256 KiB
//   ETH irq (level) -> PLIC source 1 -> MEIP (cause 11) -> firmware trap handler
//   ETH phy/mdio <-> PHY (MDIO address 1) <-> line <-> link partner (records frames)
//
// The MAC component is the same library the host platform and the unit tests
// use; here it is bound to CDC-VP's router, PLIC and CPU wrapper unchanged --
// local offsets in, level interrupt out.  DRAM is at 0, so the IP's buffer
// offsets are the CPU's addresses.  The firmware ends the run through
// SIM_CTRL with its verdict; --sim-ms bounds a firmware that never gets there.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <systemc>
#include <tlm>

#include <bus_router.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <riscv_vp_wrapper.h>

#include "icsl_eth/eth_mac_tlm.h"
#include "icsl_eth/eth_phy_tlm.h"
#include "vp/memory_tlm.h"
#include "vp/sim_ctrl_tlm.h"
#include "vp/uart_tlm.h"

using namespace sc_core;

namespace {

constexpr uint64_t kDdrBase = 0x0000'0000, kDdrSize = 0x400'0000;
constexpr uint64_t kEthBase = 0x4000'0000, kEthSize = 0x1'0000;
constexpr uint64_t kUartBase = 0x4001'0000, kUartSize = 0x1000;
constexpr uint64_t kSimBase = 0x4002'0000, kSimSize = 0x1000;
constexpr uint64_t kPlicBase = 0x4C00'0000, kPlicSize = 0x40'0000;
constexpr uint64_t kRamBase = 0x8000'0000, kRamSize = 0x4'0000;

struct RiscvEthSoc : sc_module {
    cdc::cpu::riscv_vp_cpu cpu;
    cdc::components::bus_router bus;
    vp::MemoryTlm ddr;
    icsl_eth::EthMacTlm eth;
    icsl_eth::EthPhyTlm phy;
    icsl_eth::EthLinkPartner partner;
    vp::UartTlm uart;
    vp::SimCtrlTlm sim;
    cdc::components::plic_tlm plic;
    cdc::components::memory_tlm ram;
    sc_signal<bool> eth_rst_n{"eth_rst_n", false};
    sc_signal<bool> eth_irq{"eth_irq", false};

    SC_HAS_PROCESS(RiscvEthSoc);
    RiscvEthSoc(sc_module_name n, const icsl_eth::EthMacParams& ep)
        : sc_module(n),
          cpu("cpu"),
          bus("bus", /*targets=*/6, /*initiators=*/2),
          ddr("ddr", kDdrSize, sc_time(60, SC_NS), ep.clk_period, ep.axi_data_bytes),
          eth("eth", ep),
          phy("phy", 1),
          partner("partner"),
          uart("uart"),
          sim("sim_ctrl"),
          plic("plic", cpu, 1),
          ram("ram", kRamSize) {
        cpu.data_bus().bind(bus.cpu_port(0));        // Bremen: one combined bus
        eth.axi.bind(bus.cpu_port(1));
        bus.add_target(kDdrBase, kDdrSize).bind(ddr.socket);
        bus.add_target(kEthBase, kEthSize).bind(eth.csr);
        bus.add_target(kUartBase, kUartSize).bind(uart.socket);
        bus.add_target(kSimBase, kSimSize).bind(sim.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        eth.phy_tx.bind(phy.mac_tx);
        phy.mac_rx.bind(eth.phy_rx);
        eth.mdio.bind(phy.mdio);
        phy.line_tx.bind(partner.line_rx);
        partner.line_tx.bind(phy.line_rx);
        eth.rst_n(eth_rst_n);
        eth.rmii_rst_n(eth_rst_n);
        eth.irq(eth_irq);
        plic.irq_in[0](eth_irq);                     // PLIC source 1
        SC_THREAD(power_on_reset);
    }

    // a real reset pulse, not a line held high from elaboration
    void power_on_reset() {
        eth_rst_n.write(false);
        wait(40, SC_NS);
        eth_rst_n.write(true);
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    std::string fw;
    double sim_ms = 1000, quantum_ns = 1000;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--fw") fw = next();
        else if (a == "--sim-ms") sim_ms = std::stod(next());
        else if (a == "--quantum-ns") quantum_ns = std::stod(next());
        else {
            std::fprintf(stderr, "usage: %s --fw ELF [--sim-ms N] [--quantum-ns N]\n", argv[0]);
            return 2;
        }
    }
    if (fw.empty()) {
        std::fprintf(stderr, "--fw is required\n");
        return 2;
    }
    // Bremen's ISS needs a global quantum of at least its 10 ns cycle
    tlm::tlm_global_quantum::instance().set(sc_time(std::max(quantum_ns, 10.0), SC_NS));

    icsl_eth::EthMacParams ep;
    RiscvEthSoc soc("soc", ep);
    soc.cpu.load_elf(fw);
    std::printf("riscv_eth_soc: cpu backend %s, firmware %s\n", soc.cpu.backend_name().c_str(), fw.c_str());
    std::fflush(stdout);
    sc_start(sc_time(sim_ms, SC_MS));
    std::printf("SIM: backend=%s\n", soc.cpu.backend_name().c_str());
    std::printf("SIM: retired=%llu\n", (unsigned long long)soc.cpu.get_instret());
    std::printf("SIM: %s simulated, %llu frames sent, %llu received, %zu on the line\n",
                sc_time_stamp().to_string().c_str(), (unsigned long long)soc.eth.frames_sent(),
                (unsigned long long)soc.eth.frames_received(), soc.partner.received().size());
    if (!soc.sim.exited()) {
        std::printf("SIM: TIMEOUT -- the firmware did not report a verdict in %.0f ms\n", sim_ms);
        return 124;
    }
    std::printf("SIM: firmware exit %d\n", soc.sim.exit_code());
    return soc.sim.exit_code();
}
