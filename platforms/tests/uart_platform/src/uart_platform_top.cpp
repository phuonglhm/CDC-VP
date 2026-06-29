#include "uart_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart.h>

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::uart_platform {
namespace {

#if defined(CDC_CPU_BACKEND_riscv_vp)
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
#else
using cpu_backend_t = cdc::cpu::riscv_tlm_cpu;
#endif

constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase  = 0x0C00'0000;
constexpr std::uint64_t kPlicSize  = 0x0040'0000;
constexpr std::uint64_t kUartBase  = 0x1000'0000;
constexpr std::uint64_t kMmioSize  = 0x1000;
constexpr std::uint64_t kRamBase   = 0x8000'0000;
constexpr std::uint64_t kRamSize   = 0x0010'0000;

constexpr unsigned kNumPlicSources = 1;

} // namespace

struct uart_platform_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);
    
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    UartTLM uart;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    sc_core::sc_buffer<unsigned char> uart_tx;
    sc_core::sc_signal<bool> uart_irq;
    sc_core::sc_signal<bool> plic_dummy;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/4, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , uart_tx("uart_tx")
        , uart_irq("uart_irq")
        , plic_dummy("plic_dummy")
    {
        SC_METHOD(monitor_tx);  
        sensitive << uart_tx;
        dont_initialize();

        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase,   kRamSize).bind(ram.socket);
        bus.add_target(kUartBase,  kMmioSize).bind(uart.bus);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase,  kPlicSize).bind(plic.socket);

        uart.tx(uart_tx);
        uart.irq(uart_irq);
        plic.irq_in[0](plic_dummy);
        plic_dummy.write(false);

        if (!config_path.empty()) {
            std::cout << "uart_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000\n";
        }
    }
    
    void monitor_tx() {
        std::cout << uart_tx.read();
        std::cout.flush();
    }
};

uart_platform_top::uart_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

uart_platform_top::~uart_platform_top() = default;

void uart_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string uart_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::uart_platform