#include "riscv_cpu_eval_top.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm>

namespace {

std::string get_opt(int argc, char* argv[], const std::string& name, const std::string& def)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return def;
}

bool has_flag(int argc, char* argv[], const std::string& name)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) {
            return true;
        }
    }
    return false;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config = get_opt(argc, argv, "-c",
                                       "platforms/riscv_cpu_eval/configs/default.yaml");
    const std::string fw = get_opt(argc, argv, "--fw", "");
    const bool bench = has_flag(argc, argv, "--bench");

    // TLM global quantum (ns). Must be >= the ISS cycle time (10ns). Larger = more
    // batching = faster host simulation (quantum-keeper effect). Set BEFORE the
    // platform (the Bremen ISS reads it in its constructor).
    const std::uint64_t quantum_ns =
        std::stoull(get_opt(argc, argv, "--quantum", "1000"));
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(quantum_ns), sc_core::SC_NS));

    cdc::platforms::riscv_cpu_eval::riscv_cpu_eval_top top("riscv_cpu_eval", config);
    if (!fw.empty()) {
        top.load_firmware(fw);
    }

    if (bench) {
        // Run a fixed amount of simulated time and measure host wall-clock +
        // retired instructions -> MIPS. Quantum-keeper batching shows up as
        // higher MIPS for larger --quantum.
        const std::uint64_t sim_ms =
            std::stoull(get_opt(argc, argv, "--sim-ms", "20"));
        const auto t0 = std::chrono::steady_clock::now();
        sc_core::sc_start(sc_core::sc_time(static_cast<double>(sim_ms), sc_core::SC_MS));
        const auto t1 = std::chrono::steady_clock::now();

        const double host_s =
            std::chrono::duration<double>(t1 - t0).count();
        const std::uint64_t instret = top.get_instret();
        const double mips = host_s > 0 ? (instret / host_s) / 1e6 : 0.0;

        std::cout << "BENCH"
                  << " backend=\"" << top.backend_name() << "\""
                  << " quantum_ns=" << quantum_ns
                  << " sim_ms=" << sim_ms
                  << " instret=" << instret
                  << " host_s=" << host_s
                  << " MIPS=" << mips << '\n';
        return 0;
    }

    // Normal mode: bounded simulated time (firmware has no self-stop).
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
    return 0;
}
