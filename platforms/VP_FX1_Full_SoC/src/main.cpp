#include "vp_fx1_full_soc_top.h"

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>

namespace {

std::string get_opt(int argc, char* argv[], const std::string& name, const std::string& def)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name && i + 1 < argc) return argv[i + 1];
    }
    return def;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config = get_opt(argc, argv, "-c",
                                       "platforms/VP_FX1_Full_SoC/configs/default.yaml");
    const std::string fw = get_opt(argc, argv, "--fw", "");

    // Bremen's ISS requires the TLM global quantum >= its cycle time; set before build.
    const std::uint64_t quantum_ns = std::stoull(get_opt(argc, argv, "--quantum", "1000"));
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(quantum_ns), sc_core::SC_NS));

    cdc::platforms::vp_fx1_full_soc::vp_fx1_full_soc_top top("vp_fx1_full_soc", config);
    if (!fw.empty()) top.load_firmware(fw);

    // Simulated run length in ms (0 = elaboration/smoke check only).
    const double sim_ms = std::stod(get_opt(argc, argv, "--sim-ms", "5"));
    sc_core::sc_start(sc_core::sc_time(sim_ms, sc_core::SC_MS));
    return 0;
}
