#include "pwm_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart_tlm.h>
#include "pwm.h"

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::pwm_platform {
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
constexpr std::uint64_t kPwmBase   = 0x1005'0000;
constexpr std::uint64_t kMmioSize  = 0x1000;
constexpr std::uint64_t kRamBase   = 0x8000'0000;
constexpr std::uint64_t kRamSize   = 0x0010'0000;

constexpr unsigned kNumPlicSources = 0;

} // namespace

struct pwm_platform_top::impl : public sc_core::sc_module {
    cpu_backend_t              cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm  uart;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm  plic;
    PWM                        pwm;
    sc_core::sc_signal<bool>   pwm_out_sig;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , pwm("pwm")
        , pwm_out_sig("pwm_out_sig")
    {
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase,  kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kMmioSize).bind(uart.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kPwmBase,  kMmioSize).bind(pwm.socket);

        pwm.pwm_out(pwm_out_sig);

        if (!config_path.empty()) {
            std::cout << "pwm_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 PWM=0x10060000\n";
        }
    }
};

pwm_platform_top::pwm_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{}

pwm_platform_top::~pwm_platform_top() = default;

void pwm_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string pwm_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::pwm_platform