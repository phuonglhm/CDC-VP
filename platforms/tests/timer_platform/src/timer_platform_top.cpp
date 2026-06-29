//author: linhtk55-fpt

#include "timer_platform_top.h"

#include <cstdint>
#include <iostream>

#include <timer.h>
#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart.h>

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::tests::timer_platform {
namespace {

using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t ktimerBase = 0x1003'0000;
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 1;

} // namespace

struct timer_platform_top::impl : public sc_core::sc_module {
   cpu_backend_t cpu;
   cdc::components::bus_router bus;
   cdc::components::memory_tlm ram;
   UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
   cdc::components::clint_tlm clint;
   cdc::components::plic_tlm plic;
   cdc::components::Timer timer;
   sc_core::sc_signal<bool> timer_irq;
   sc_core::sc_signal<bool> timer_rst_n;
   sc_core::sc_signal<bool> timer_extin;

   impl(sc_core::sc_module_name name, const std::string &config_path)
       : sc_core::sc_module(name)
       , cpu("cpu")
       , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
       , ram("ram", kRamSize)
       , uart("uart")
       , uart_tx("uart_tx")
       , uart_irq("uart_irq")
       , clint("clint", cpu)
       , plic("plic", cpu, kNumPlicSources)
       , timer("timer",
               1, 2, 3, 4, 5, 6, 7, 8,
               1, 2, 3, 4,
               sc_core::sc_time(10, sc_core::SC_NS))
       , timer_irq("timer_irq") {
      
      if (cpu.has_unified_bus()) {
         cpu.data_bus().bind(bus.cpu_port(0));
      } else {
         cpu.instr_bus().bind(bus.cpu_port(0));
         cpu.data_bus().bind(bus.cpu_port(1));
      }
     
      bus.add_target(kRamBase, kRamSize).bind(ram.socket);
      bus.add_target(kUartBase, kMmioSize).bind(uart.bus);
      uart.tx(uart_tx);
      uart.irq(uart_irq);
      bus.add_target(kClintBase, kClintSize).bind(clint.socket);
      bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
      bus.add_target(ktimerBase, kMmioSize).bind(timer.socket);

      timer.irq_out(timer_irq);
      plic.irq_in[0](timer_irq);

      //dummy signals
      timer.reset_n(timer_rst_n);
      timer.extin(timer_extin);

      //reset is high, external input is low
      timer_rst_n.write(true);
      timer_extin.write(false);

      if (!config_path.empty()) {
         std::cout << "timer_platform config: " << config_path << '\n';
         std::cout << "cpu backend: " << cpu.backend_name() << '\n';
         std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                   << "CLINT=0x02000000 PLIC=0x0C000000 timer=0x10030000\n";
         std::cout << "irq map: timer0 -> PLIC source 1 -> MEIP\n";
      }
   }
};

timer_platform_top::timer_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path)) {
}

timer_platform_top::~timer_platform_top() = default;

void timer_platform_top::load_firmware(const std::string &path) {
   impl_->cpu.load_elf(path);
}

std::string timer_platform_top::backend_name() const {
   return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::timer_platform
