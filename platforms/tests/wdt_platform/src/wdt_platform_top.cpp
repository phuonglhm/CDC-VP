// Author: hoangv11
// Verified by: quannh107

#include "wdt_platform_top.h"

#include <cstdint>
#include <iostream>

#include <wdt_tlm.h>
#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart.h>

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::tests::wdt_platform {
namespace {

using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
// Memory map của platform: base address + size cho từng vùng.
// Hardcode tại đây (chưa đọc từ YAML). CPU truy cập một địa chỉ ->
// bus_router decode xem nó rơi vào vùng nào -> chuyển tới IP tương ứng.
constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kWdtBase = 0x1004'0000;
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 5;

} // namespace

struct wdt_platform_top::impl : public sc_core::sc_module {
   SC_HAS_PROCESS(impl);

   // Các module phần cứng của platform: CPU + bus + các IP memory-mapped
   // + một dây tín hiệu cho ngắt của wdt.
   cpu_backend_t cpu;
   cdc::components::bus_router bus;
   cdc::components::memory_tlm ram;
   UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
   cdc::components::clint_tlm clint;
   cdc::components::plic_tlm plic;
   cdc::components::wdt_tlm wdt;
   sc_core::sc_signal<bool> reset_n;
   sc_core::sc_signal<bool> wdt_irq;
   sc_core::sc_signal<bool> wdt_reset_o;

   // Dummy signals to tie off unused PLIC inputs
   sc_core::sc_signal<bool> dummy_irq[kNumPlicSources - 1];

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
       , wdt("wdt", sc_core::sc_time(10, sc_core::SC_NS))
       , reset_n("reset_n")
       , wdt_irq("wdt_irq")
       , wdt_reset_o("wdt_reset_o") {

      SC_THREAD(reset_sequence);

      // reset method when watchdog issues a reset
      SC_METHOD(handle_wdt_reset);
      sensitive << wdt_reset_o;
      dont_initialize();

      // Nối (các) initiator socket của CPU vào upstream port của bus.
      //   - Bremen (riscv_vp): 1 bus chung cho cả lệnh lẫn dữ liệu -> 1 port.
      if (cpu.has_unified_bus()) {
         cpu.data_bus().bind(bus.cpu_port(0));
      } else {
         cpu.instr_bus().bind(bus.cpu_port(0));
         cpu.data_bus().bind(bus.cpu_port(1));
      }
      // Khai báo memory map của bus. Mỗi add_target(base, size) đăng ký một
      // vùng địa chỉ và trả về socket downstream của bus để bind vào IP.
      // Số lần gọi add_target phải khớp num_targets=5 ở init list bên trên.
      bus.add_target(kRamBase, kRamSize).bind(ram.socket);
      bus.add_target(kUartBase, kMmioSize).bind(uart.bus);
      uart.tx(uart_tx);
      uart.irq(uart_irq);
      bus.add_target(kClintBase, kClintSize).bind(clint.socket);
      bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
      bus.add_target(kWdtBase, kMmioSize).bind(wdt.target_socket);

      // Đường ngắt của wdt: wdt -> PLIC -> CPU.
      // wdt kéo wdt_irq lên mức 1 khi có sự kiện ngắt.
      wdt.reset_n(reset_n);
      wdt.irq(wdt_irq);
      wdt.reset_o(wdt_reset_o);

      // Tie off unused PLIC ports (0, 1, 2, 3)
      for (unsigned i = 0; i < kNumPlicSources - 1; ++i) {
         dummy_irq[i].write(false); // Tie to ground
         plic.irq_in[i](dummy_irq[i]);
      }

      plic.irq_in[4](wdt_irq);

      if (!config_path.empty()) {
         std::cout << "wdt_platform config: " << config_path << '\n';
         std::cout << "cpu backend: " << cpu.backend_name() << '\n';
         std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                   << "CLINT=0x02000000 PLIC=0x0C000000 wdt=0x10040000\n";
         std::cout << "irq map: wdt0 -> PLIC source 5 -> MEIP\n";
         std::cout << "reset map: WDT reset_n active-low, assert at 0ns, release at 100ns\n";
      }
   }

   void reset_sequence() {
      // Reset active-low cho WDT và các IP khác.
      reset_n.write(false);
      wait(sc_core::sc_time(100, sc_core::SC_NS));
      reset_n.write(true);
   }

   void handle_wdt_reset() {
      if (wdt_reset_o.read() == true) {
         std::cout << sc_core::sc_time_stamp() << " [PLATFORM] Watchdog IRQ not clear -> Resetting System\n";
         cpu.reset_cpu();
      }
   }
};

wdt_platform_top::wdt_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path)) {
}

wdt_platform_top::~wdt_platform_top() = default;

void wdt_platform_top::load_firmware(const std::string &path) {
   impl_->cpu.load_elf(path);
}

std::string wdt_platform_top::backend_name() const {
   return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::wdt_platform
