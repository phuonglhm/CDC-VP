//author: linhtk55-fpt

#include "trng_platform_top.h"

#include <cstdint>
#include <iostream>

#include <trng_model.h>
#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart_tlm.h>
// Ensure trng_tlm type is visible
#include <trng_tlm.h>

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::trng_platform {
namespace {

#if defined(CDC_CPU_BACKEND_riscv_vp)
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
#else
using cpu_backend_t = cdc::cpu::riscv_tlm_cpu;
#endif

constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t ktrngBase = 0x1470'0000; //random unoccupied address in FX1 mem map
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 1;

} // namespace

struct trng_platform_top::impl : public sc_core::sc_module {
   cpu_backend_t cpu;
   cdc::components::bus_router bus;
   cdc::components::memory_tlm ram;
   cdc::components::uart_tlm uart;
   cdc::components::clint_tlm clint;
   cdc::components::plic_tlm plic;
   cdc::components::trng_tlm trng;
   sc_core::sc_signal<bool> trng_irq;
   sc_core::sc_signal<bool> trng_reset;
   sc_core::sc_clock trng_clk;

   impl(sc_core::sc_module_name name, const std::string &config_path)
       : sc_core::sc_module(name)
       , cpu("cpu")
       , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
       , ram("ram", kRamSize)
       , uart("uart")
       , clint("clint", cpu)
       , plic("plic", cpu, kNumPlicSources)
       , trng("trng", sc_core::sc_time(10, sc_core::SC_NS))
       , trng_irq("trng_irq")
       , trng_reset("")
       , trng_clk("trng_clk", 10, sc_core::SC_NS) {
      
      if (cpu.has_unified_bus()) {
         cpu.data_bus().bind(bus.cpu_port(0));
      } else {
         cpu.instr_bus().bind(bus.cpu_port(0));
         cpu.data_bus().bind(bus.cpu_port(1));
      }
     
      bus.add_target(kRamBase, kRamSize).bind(ram.socket);
      bus.add_target(kUartBase, kMmioSize).bind(uart.socket);
      bus.add_target(kClintBase, kClintSize).bind(clint.socket);
      bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
      bus.add_target(ktrngBase, kMmioSize).bind(trng.socket);

      trng.reset_n(trng_reset);
      trng.clk(trng_clk);
      trng.irq_out(trng_irq);
      plic.irq_in[0](trng_irq);

      //reset (activelow)
      trng_reset.write(true);

      if (!config_path.empty()) {
         std::cout << "trng_platform config: " << config_path << '\n';
         std::cout << "cpu backend: " << cpu.backend_name() << '\n';
         std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                   << "CLINT=0x02000000 PLIC=0x0C000000 trng=0x14700000\n";
         std::cout << "irq map: trng -> PLIC source 1 -> MEIP\n";
      }
   }
};

trng_platform_top::trng_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path)) {
}

trng_platform_top::~trng_platform_top() = default;

void trng_platform_top::load_firmware(const std::string &path) {
   impl_->cpu.load_elf(path);
}

std::string trng_platform_top::backend_name() const {
   return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::trng_platform
