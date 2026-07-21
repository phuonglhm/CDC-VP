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

bool has_flag(int argc, char* argv[], const std::string& name)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) return true;
    }
    return false;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string config = get_opt(argc, argv, "-c",
                                       "platforms/VP_FX1_Full_SoC/configs/default.yaml");
    const std::string fw = get_opt(argc, argv, "--fw", "");
    const std::string int_flash = get_opt(argc, argv, "--int-flash", "");
    const std::string spi_flash = get_opt(argc, argv, "--spi-flash", "");
    const std::string boot_pin = get_opt(argc, argv, "--boot-pin", "low");
    // UART0 host input path: TCP bridge or deterministic file replay. A replay
    // delay is useful for RTOS firmware that enables RX interrupts after boot.
    const std::string uart0_socket = get_opt(argc, argv, "--uart0-socket", "");
    const bool uart0_wait = has_flag(argc, argv, "--uart0-wait");
    const std::string uart0_rx_file = get_opt(argc, argv, "--uart0-rx-file", "");
    const std::uint64_t uart0_rx_delay_us =
        std::stoull(get_opt(argc, argv, "--uart0-rx-delay-us", "0"));

    // Bremen's ISS requires the TLM global quantum >= its cycle time; set before build.
    const std::uint64_t quantum_ns = std::stoull(get_opt(argc, argv, "--quantum", "1000"));
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(quantum_ns), sc_core::SC_NS));

    cdc::platforms::vp_fx1_full_soc::vp_fx1_full_soc_top top("vp_fx1_full_soc", config);
    if (!fw.empty()) top.load_firmware(fw);
    if (!int_flash.empty()) top.load_int_flash(int_flash);
    if (!spi_flash.empty()) top.load_spi_flash(spi_flash);
    top.set_boot_pin(boot_pin == "high" || boot_pin == "1");
    if (!uart0_socket.empty())
        top.set_uart0_socket(static_cast<std::uint16_t>(std::stoul(uart0_socket)), uart0_wait);
    if (!uart0_rx_file.empty())
        top.set_uart0_rx_file(uart0_rx_file, uart0_rx_delay_us);

    // Simulated run length in ms (0 = elaboration/smoke check only).
    const double sim_ms = std::stod(get_opt(argc, argv, "--sim-ms", "5"));
    sc_core::sc_start(sc_core::sc_time(sim_ms, sc_core::SC_MS));
    return 0;
}
