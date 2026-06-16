#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::i2c_platform {

// Small Bremen/RISC-V SoC for I2C IP verification:
// CPU -> bus_router -> RAM/UART/CLINT/PLIC/I2C, with I2C external IRQ via PLIC.
class i2c_platform_top : public sc_core::sc_module {
public:
    i2c_platform_top(sc_core::sc_module_name name, std::string config_path);
    ~i2c_platform_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::i2c_platform
