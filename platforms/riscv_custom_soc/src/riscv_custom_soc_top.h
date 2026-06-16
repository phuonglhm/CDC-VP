#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::riscv_custom_soc {

// Step 3 platform: a custom RISC-V SoC with a team-controlled interrupt subsystem
// (CLINT for timer/software interrupts, PLIC for external/peripheral interrupts),
// running on the CPU backend selected by CDC_CPU_BACKEND.
class riscv_custom_soc_top : public sc_core::sc_module {
public:
    riscv_custom_soc_top(sc_core::sc_module_name name, std::string config_path);
    ~riscv_custom_soc_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::riscv_custom_soc
