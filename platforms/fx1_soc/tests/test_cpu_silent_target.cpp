// Negative integration test (review G2-R2): a real VP++ hart reads a target
// that never writes a response. VP++ pre-sets the payload to OK, so without
// the CpuPortAdapter's INCOMPLETE reset the guest would carry on and write
// PASS. The run must instead stop with an integration error before any verdict.
//
//   test_fx1_cpu_silent_target <silent_read.elf>
// Exit 0 when the integration error is raised as expected, 1 otherwise.

#include <exception>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <bus/bus_system.h>
#include <fx1/cpu_port_adapter.h>
#include <fx1/fx1_memory_map.h>
#include <fx1/sim_control.h>
#include <fx1/sparse_ram.h>
#include <riscv_vp_plusplus_wrapper.h>

namespace {
struct silent_target : sc_core::sc_module {
    tlm_utils::simple_target_socket<silent_target> socket{"socket"};
    unsigned reads = 0;
    explicit silent_target(sc_core::sc_module_name name) : sc_module(name) {
        socket.register_b_transport(this, &silent_target::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&) { ++reads; }  // never answers
};

struct harness : sc_core::sc_module {
    bus::BusSystem fabric;
    fx1::SparseRam ddr{"ddr", FX1_DDR_SIZE};
    silent_target silent{"silent"};
    fx1::SimControl sim{"sim_control"};
    cdc::cpu::riscv_vp_plusplus_cpu cpu;
    fx1::CpuPortAdapter port{"cpu_port"};

    static bus::BusConfig config() {
        bus::BusConfig cfg;
        cfg.initiators = {{"CPU1"}};
        cfg.targets = {
            {"DDR", FX1_DDR_BASE, FX1_DDR_SIZE, bus::TargetPath::SysBus1Axi, true},
            {"SILENT", FX1_SYS_DMA_CSR_BASE, FX1_APB_SLOT_SIZE, bus::TargetPath::Peribus0Apb, true},
            {"SIM_CTRL", FX1_SIM_CTRL_BASE, FX1_APB_SLOT_SIZE, bus::TargetPath::Peribus0Apb, true}};
        return cfg;
    }
    static cdc::cpu::cpu_config hart0() {
        cdc::cpu::cpu_config cfg;
        cfg.hart_id = 0;
        return cfg;
    }
    static cdc::cpu::riscv_vp_plusplus_options options() {
        cdc::cpu::riscv_vp_plusplus_options opts;
        opts.disable_extensions = "DV";
        return opts;
    }

    harness(sc_core::sc_module_name name, const std::string& elf)
        : sc_module(name), fabric("fabric", config()), cpu("cpu", hart0(), options()) {
        fabric.target("DDR").bind(ddr.socket);
        fabric.target("SILENT").bind(silent.socket);
        fabric.target("SIM_CTRL").bind(sim.socket);
        cpu.data_bus().bind(port.target);
        port.out.bind(fabric.initiator("CPU1"));
        cpu.load_elf(elf);
    }
};
} // namespace

int sc_main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: test_fx1_cpu_silent_target <silent_read.elf>\n";
        return 2;
    }
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(1, sc_core::SC_US));
    harness top("top", argv[1]);
    std::string error;
    try {
        sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_MS));
    } catch (const std::exception& e) {
        error = e.what();
    }
    const bool raised = error.find("no target set a response") != std::string::npos;
    const bool verdict = top.sim.result() != fx1::SimControl::Result::running;
    std::cout << "silent reads=" << top.silent.reads << " integration_error=" << raised
              << " guest_verdict=" << verdict << '\n';
    if (!raised || verdict || top.silent.reads != 1) {
        std::cout << "FAIL: expected exactly one silent read, an integration error and no verdict\n"
                  << (error.empty() ? std::string("(no error raised)") : error) << '\n';
        return 1;
    }
    std::cout << "PASS: silent target stopped the run as an integration error\n";
    return 0;
}
