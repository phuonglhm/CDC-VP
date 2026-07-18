#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::vp_fx1_full_soc {

// VP_FX1 full SoC: the integrated Bremen RV32 virtual SoC assembled from all
// implemented TLM IP at their SoC-spec addresses (docs/peripheral_memory_map.md).
// CLINT/PLIC interrupt subsystem; SAURIA NPUv4 can be enabled for internal
// builds while ISP/VPU windows remain reserved.
class vp_fx1_full_soc_top : public sc_core::sc_module {
public:
    vp_fx1_full_soc_top(sc_core::sc_module_name name, std::string config_path);
    ~vp_fx1_full_soc_top() override;

    void load_firmware(const std::string& path);

    // Preload the internal-flash ROM window (0x0400_0000) from a raw binary.
    void load_int_flash(const std::string& path);

    // Preload the NOR flash behind SPI0 (boot-flow SPI download source).
    void load_spi_flash(const std::string& path);

    // Drive the ROM-code boot-mode strap (GPIO0 pin 1). Default low.
    void set_boot_pin(bool high);

    // UART0 host input path (boot-flow download branch). Call before sc_start.
    void set_uart0_socket(std::uint16_t port, bool wait_for_client);
    void set_uart0_rx_file(const std::string& path);

    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::vp_fx1_full_soc
