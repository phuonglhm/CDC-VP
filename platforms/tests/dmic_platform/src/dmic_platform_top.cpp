#include "dmic_platform_top.h"

#include <cstdint>
#include <iostream>

#include <dmic.h>
#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart.h>
#include "tlm_utils/simple_initiator_socket.h"

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::tests::dmic_platform {
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
constexpr std::uint64_t kdmicBase = 0x100A'0000;
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 12;

} // namespace

struct dmic_platform_top::impl : public sc_core::sc_module {
   SC_HAS_PROCESS(impl);

   // Các module phần cứng của platform: CPU + bus + các IP memory-mapped
   // + một dây tín hiệu cho ngắt của DMIC.
   cpu_backend_t cpu;
   cdc::components::bus_router bus;
   cdc::components::memory_tlm ram;
   UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
   cdc::components::clint_tlm clint;
   cdc::components::plic_tlm plic;
   cdc::components::DmicTLM dmic;
   sc_core::sc_signal<bool> reset_n;
   sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> dmic_irq;
   sc_core::sc_signal<bool> dummy_irq;
   tlm_utils::simple_initiator_socket<impl> pdm_dummy_socket;

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
       , dmic("dmic")
       , reset_n("reset_n")
       , dmic_irq("dmic_irq")
       , pdm_dummy_socket("pdm_dummy_socket") {
      SC_THREAD(reset_sequence);
      SC_THREAD(pdm_stimulus_thread);

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
      bus.add_target(kdmicBase, kMmioSize).bind(dmic.bus_target_socket);

      pdm_dummy_socket.bind(dmic.pdm_target_socket);

      // Đường ngắt của DMIC: DMIC -> PLIC -> CPU.
      // dmic kéo dmic_irq lên mức 1 khi có sự kiện ngắt.
      dmic.reset_n(reset_n);
      dmic.irq_out(dmic_irq);
      for (unsigned i = 0; i < kNumPlicSources; ++i) {
         if (i == 11) {
            plic.irq_in[i](dmic_irq);
         } else {
            plic.irq_in[i](dummy_irq);
         }
      }

      if (!config_path.empty()) {
         std::cout << "dmic_platform config: " << config_path << '\n';
         std::cout << "cpu backend: " << cpu.backend_name() << '\n';
         std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                   << "CLINT=0x02000000 PLIC=0x0C000000 dmic=0x100A0000\n";
         std::cout << "irq map: dmic0 -> PLIC source 12 -> MEIP\n";
         std::cout << "reset map: DMIC reset_n active-low, assert at 0ns, release at 100ns\n";
      }
   }

   void reset_sequence() {
      // Reset active-low cho DMIC.
      // Lúc đầu kéo reset_n = 0 để reset register trong DMIC, sau 100ns nhả reset.
      reset_n.write(false);
      wait(sc_core::sc_time(100, sc_core::SC_NS));
      reset_n.write(true);
   }

   void pdm_stimulus_thread() {
      // Wait for reset release
      wait(reset_n.posedge_event());

      tlm::tlm_generic_payload trans;
      pdm_payload payload;
      sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
      trans.set_data_ptr(reinterpret_cast<unsigned char *>(&payload));
      trans.set_data_length(sizeof(pdm_payload));
      trans.set_command(tlm::TLM_WRITE_COMMAND);

      double sample_count = 0.0;
      const double PI = 3.141592653589793;

      while (true) {
         double analog_signal = std::sin(2.0 * PI * sample_count / 3000.0);
         double random_val = (double)rand() / RAND_MAX * 2.0 - 1.0;
         payload.density = (analog_signal > random_val) ? 1 : -1;

         pdm_dummy_socket->b_transport(trans, delay);
         wait(delay);
         delay = sc_core::SC_ZERO_TIME;
         sample_count++;

         // 3 MHz PDM clock -> ~333 ns per sample
         wait(sc_core::sc_time(333, sc_core::SC_NS));
      }
   }
};

dmic_platform_top::dmic_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path)) {
}

dmic_platform_top::~dmic_platform_top() = default;

void dmic_platform_top::load_firmware(const std::string &path) {
   impl_->cpu.load_elf(path);
}

std::string dmic_platform_top::backend_name() const {
   return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::dmic_platform
