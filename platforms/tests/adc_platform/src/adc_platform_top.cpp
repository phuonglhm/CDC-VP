#include "adc_platform_top.h"

#include <cstdint>
#include <iostream>

#include <adc_tlm.h>
#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart_tlm.h>

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::adc_platform {
namespace {

#if defined(CDC_CPU_BACKEND_riscv_vp)
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;
#else
using cpu_backend_t = cdc::cpu::riscv_tlm_cpu;
#endif
// Memory map của platform: base address + size cho từng vùng.
// Hardcode tại đây (chưa đọc từ YAML). CPU truy cập một địa chỉ ->
// bus_router decode xem nó rơi vào vùng nào -> chuyển tới IP tương ứng.
constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kAdcBase = 0x1006'0000;
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 1;

} // namespace

struct adc_platform_top::impl : public sc_core::sc_module {
    // Các module phần cứng của platform: CPU + bus + các IP memory-mapped
    // + một dây tín hiệu cho ngắt của ADC.
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm uart;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    cdc::components::adc_tlm adc;
    sc_core::sc_signal<bool> adc_irq;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , adc("adc")
        , adc_irq("adc_irq")
    {
        // Nối (các) initiator socket của CPU vào upstream port của bus.
        //   - Bremen (riscv_vp): 1 bus chung cho cả lệnh lẫn dữ liệu -> 1 port.
        //   - mariusmm (riscv_tlm): tách bus instr và data -> 2 port.
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
        bus.add_target(kUartBase, kMmioSize).bind(uart.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kAdcBase, kMmioSize).bind(adc.socket);

        // Đường ngắt của ADC: ADC -> PLIC -> CPU.
        // ADC kéo adc_irq lên mức 1 khi có sự kiện ngắt.
        adc.irq_out(adc_irq);
        // PLIC nhận tín hiệu này ở irq_in[0]. Lưu ý: irq_in[index] tương ứng
        // PLIC source id = index + 1 (source 0 bị reserve theo chuẩn RISC-V),
        // nên đây là PLIC source 1. PLIC sẽ báo external interrupt (MEIP) về CPU.
        plic.irq_in[0](adc_irq);

        if (!config_path.empty()) {
            std::cout << "adc_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 ADC=0x10060000\n";
            std::cout << "irq map: ADC0 -> PLIC source 1 -> MEIP\n";
        }
    }
};

adc_platform_top::adc_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

adc_platform_top::~adc_platform_top() = default;

void adc_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string adc_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::adc_platform
