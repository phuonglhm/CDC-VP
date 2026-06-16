#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::spi_platform {

// Small Bremen/RISC-V SoC for SPI IP verification:
// CPU -> bus_router -> RAM/UART/CLINT/PLIC/SPI, with SPI external IRQ via PLIC.
class spi_platform_top : public sc_core::sc_module {
public:
    spi_platform_top(sc_core::sc_module_name name, std::string config_path);
    ~spi_platform_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::spi_platform
