#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::fx1_soc {

struct fx1_soc_options {
    // ELF image written into DDR through the bus backdoor before reset. Every
    // hart resets at its entry point (one-image boot, see README).
    std::string firmware;
    // Mirror of the UART TX stream; empty for none.
    std::string uart_log;
    bool uart_stdout = true;
    // Print every routed bus transaction.
    bool bus_trace = false;
    // Plan C7 exclusive monitor: real LR/SC reservations shared with SYS_DMA
    // and ISP ODMA. false = upstream VP++ lock model (negative control only).
    bool exclusive_monitor = true;
};

// FX1 SoC for FW/SW bring-up. Stage G1 content: two VP++ RV32IMAFC harts,
// FX1 bus, BootROM, sparse DDR, CLINT, UART and the VP-only sim-control
// finisher. Addresses come from fx1/fx1_memory_map.h.
class fx1_soc_top : public sc_core::sc_module {
public:
    enum class result { running, pass, fail };

    fx1_soc_top(sc_core::sc_module_name name, const fx1_soc_options& options);
    ~fx1_soc_top() override;

    result outcome() const;
    std::uint32_t fail_code() const;
    unsigned harts() const;
    std::uint64_t instret(unsigned hart) const;
    std::uint64_t pc(unsigned hart) const;
    // Copy `length` bytes of DDR at global `address` into `path` (debug
    // artifacts such as ISP NV12 output). Throws if out of range or unwritable.
    void dump_ddr(std::uint64_t address, std::uint64_t length, const std::string& path) const;
    // One line of exclusive-monitor counters ("off" without a monitor).
    std::string atomics_report() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::fx1_soc
