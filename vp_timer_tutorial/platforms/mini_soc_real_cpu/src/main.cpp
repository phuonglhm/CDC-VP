#include "mini_soc_top.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include <systemc>
#include <tlm>

namespace {

std::string option(int argc, char* argv[], const std::string& name,
                   const std::string& fallback)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string firmware = option(argc, argv, "--fw", "");
    if (firmware.empty()) {
        std::cerr << "usage: mini_soc_edu_timer --fw <firmware.elf> "
                     "[--sim-us N] [--quantum-ns N] [--vcd <file>]\n";
        return 2;
    }

    const std::uint64_t quantum_ns =
        std::stoull(option(argc, argv, "--quantum-ns", "1000"));
    const std::uint64_t sim_us =
        std::stoull(option(argc, argv, "--sim-us", "1000"));

    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(quantum_ns), sc_core::SC_NS));

    tutorial::mini_soc_top top("mini_soc", "tutorial/default");
    const std::string vcd = option(argc, argv, "--vcd", "");
    if (!vcd.empty()) {
        top.enable_tracing(vcd);
    }
    top.load_firmware(firmware);
    sc_core::sc_start(sc_core::sc_time(static_cast<double>(sim_us),
                                      sc_core::SC_US));

    std::cout << "SIM: backend=" << top.cpu_backend_name() << '\n';
    std::cout << "SIM: retired=" << top.retired_instructions() << '\n';
    std::cout << "SIM: completed " << sim_us << " us\n";
    return 0;
}

