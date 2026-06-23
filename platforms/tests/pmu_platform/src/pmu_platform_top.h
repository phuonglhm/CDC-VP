#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::pmu_platform {

// Small Bremen/RISC-V SoC for PMU/PWRMGR IP verification:
// CPU -> bus_router -> RAM/UART/CLINT/PLIC/PMU,
// with PMU wakeup_irq connected to PLIC external interrupt.
class pmu_platform_top : public sc_core::sc_module {
public:
    pmu_platform_top(sc_core::sc_module_name name, std::string config_path);
    ~pmu_platform_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::pmu_platform
