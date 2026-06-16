#include "riscv_custom_soc_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <i2c_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart_tlm.h>

// Select the concrete CPU backend at build time (CDC_CPU_BACKEND via CMake).
#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::riscv_custom_soc {
namespace {

#if defined(CDC_CPU_BACKEND_riscv_vp)
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
#else
using cpu_backend_t = cdc::cpu::riscv_tlm_cpu;
#endif

constexpr std::uint64_t kRamBase = 0x8000'0000;   // firmware text/data/stack
constexpr std::uint64_t kRamSize = 0x1'0000;      // 64 KiB
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kClintBase = 0x0200'0000; // standard RISC-V CLINT base
constexpr std::uint64_t kClintSize = 0x1'0000;    // covers msip/mtimecmp/mtime
constexpr std::uint64_t kPlicBase = 0x0C00'0000;  // standard RISC-V PLIC base
constexpr std::uint64_t kPlicSize = 0x40'0000;    // 4 MiB PLIC window
constexpr std::uint64_t kI2cBase = 0x1001'0000;   // external-interrupt source
constexpr std::uint64_t kRegionSize = 0x1000;

// PLIC source id 1 = I2C.
constexpr unsigned kNumPlicSources = 1;

} // namespace

struct riscv_custom_soc_top::impl : public sc_core::sc_module {
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm uart;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    cdc::components::i2c_tlm i2c;
    sc_core::sc_signal<bool> i2c_irq;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/5, cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)   // CLINT drives timer/software interrupts (MTIP/MSIP)
        , plic("plic", cpu, kNumPlicSources)  // PLIC drives external interrupts (MEIP)
        , i2c("i2c")
        , i2c_irq("i2c_irq")
    {
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kRegionSize).bind(uart.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kI2cBase, kRegionSize).bind(i2c.socket);

        // I2C interrupt line -> PLIC source 1.
        i2c.irq_out(i2c_irq);
        plic.irq_in[0](i2c_irq);

        if (!config_path.empty()) {
            std::cout << "riscv_custom_soc config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
        }
    }
};

riscv_custom_soc_top::riscv_custom_soc_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

riscv_custom_soc_top::~riscv_custom_soc_top() = default;

void riscv_custom_soc_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string riscv_custom_soc_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::riscv_custom_soc
