#include "riscv_custom_soc_top.h"

#include <cstdint>
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

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config = get_opt(argc, argv, "-c",
                                       "platforms/riscv_custom_soc/configs/default.yaml");
    const std::string fw = get_opt(argc, argv, "--fw", "");

    // Bremen's ISS requires the TLM global quantum >= its cycle time; set it before
    // building the platform. Default 1us.
    const std::uint64_t quantum_ns =
        std::stoull(get_opt(argc, argv, "--quantum", "1000"));
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(quantum_ns), sc_core::SC_NS));

    cdc::platforms::riscv_custom_soc::riscv_custom_soc_top top("riscv_custom_soc", config);
    if (!fw.empty()) {
        top.load_firmware(fw);
    }

    // Bounded run (firmware parks after the demo).
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_MS));
    return 0;
}
