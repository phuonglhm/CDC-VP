#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::riscv_cpu_eval {

// Step 2 platform: a real RISC-V core (selected by CDC_CPU_BACKEND) fetching and
// executing firmware from memory_tlm and printing through uart_tlm, wired via the
// cpu_base interface and bus_router.
class riscv_cpu_eval_top : public sc_core::sc_module {
public:
    riscv_cpu_eval_top(sc_core::sc_module_name name, std::string config_path);
    ~riscv_cpu_eval_top() override;

    // Queue an ELF firmware image; loaded into memory at start_of_simulation().
    void load_firmware(const std::string& path);

    // Introspection for benchmarking.
    std::string backend_name() const;
    std::uint64_t get_instret() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::riscv_cpu_eval
