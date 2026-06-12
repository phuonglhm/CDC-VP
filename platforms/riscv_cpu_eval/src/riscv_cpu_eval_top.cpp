#include "riscv_cpu_eval_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <memory_tlm.h>
#include <timer_tlm.h>
#include <uart_tlm.h>

// Select the concrete CPU backend at build time (CDC_CPU_BACKEND via CMake).
#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::riscv_cpu_eval {
namespace {

#if defined(CDC_CPU_BACKEND_riscv_vp)
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
#else
using cpu_backend_t = cdc::cpu::riscv_tlm_cpu;
#endif

constexpr std::uint64_t kRamBase = 0x8000'0000;   // firmware text/data/stack
constexpr std::uint64_t kRamSize = 0x1'0000;      // 64 KiB
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kTimerBase = 0x1001'0000;
constexpr std::uint64_t kRegionSize = 0x1000;

// RISC-V machine timer interrupt cause (mcause MTIP / mie MTIE bit).
constexpr std::uint32_t kCauseMachineTimer = 7;

} // namespace

struct riscv_cpu_eval_top::impl : public sc_core::sc_module {
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm uart;
    cdc::components::timer_tlm timer;
    sc_core::sc_signal<bool> timer_irq;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/3, cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , timer("timer", sc_core::sc_time(50, sc_core::SC_US))
        , timer_irq("timer_irq")
    {
        // Bremen exposes one combined bus; mariusmm has separate instr/data buses.
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kRegionSize).bind(uart.socket);
        bus.add_target(kTimerBase, kRegionSize).bind(timer.socket);

        // IRQ bridge: convert the timer's signal edge into a machine-timer
        // interrupt injected into the real CPU.
        timer.irq_out(timer_irq);
        SC_HAS_PROCESS(impl);
        SC_METHOD(on_timer_irq);
        sensitive << timer_irq.posedge_event();
        dont_initialize();

        if (!config_path.empty()) {
            std::cout << "riscv_cpu_eval config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
        }
    }

    void on_timer_irq()
    {
        cpu.raise_irq(kCauseMachineTimer);
    }
};

riscv_cpu_eval_top::riscv_cpu_eval_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

riscv_cpu_eval_top::~riscv_cpu_eval_top() = default;

void riscv_cpu_eval_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string riscv_cpu_eval_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

std::uint64_t riscv_cpu_eval_top::get_instret() const
{
    return impl_->cpu.get_instret();
}

} // namespace cdc::platforms::riscv_cpu_eval
