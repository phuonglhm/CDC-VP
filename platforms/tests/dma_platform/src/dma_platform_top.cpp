//Author: trangmn20

#include "dma_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart_tlm.h>
#include <dma_tlm.h>

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::dma_platform {
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
constexpr std::uint64_t kDmaBase   = 0x1007'0000;   // DMA APB register window
constexpr std::uint64_t kMmioSize  = 0x1000;
constexpr std::uint64_t kRamBase   = 0x8000'0000;
constexpr std::uint64_t kRamSize   = 0x0010'0000;

constexpr unsigned kNumPlicSources = 0;

} // namespace

struct dma_platform_top::impl : public sc_core::sc_module {
    cpu_backend_t               cpu;
    cdc::components::bus_router bus;       // shared bus: CPU + DMA master both go through this
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm   uart;
    cdc::components::clint_tlm  clint;
    cdc::components::plic_tlm   plic;
    cdc::components::dma_tlm    dma;
    sc_core::sc_signal<bool>     dma_reset_n;
    sc_core::sc_signal<uint32_t> dma_irq;
    sc_core::sc_signal<bool>     dma_irq_abort;

    SC_HAS_PROCESS(impl);

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        // num_initiators = 2: upstream port 0 = CPU, port 1 = DMA master.
        // If the CPU backend needs 2 ports itself (split instr/data bus),
        // that adds one more, but riscv_vp uses a unified bus -> 1 CPU port.
        , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 2u : 3u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , dma("dma")
        , dma_reset_n("dma_reset_n")
        , dma_irq("dma_irq")
        , dma_irq_abort("dma_irq_abort")
    {
        unsigned next_cpu_port = 0;
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(next_cpu_port++));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(next_cpu_port++));
            cpu.data_bus().bind(bus.cpu_port(next_cpu_port++));
        }

        // DMA's master_socket is its own upstream initiator on the same bus,
        // so its accesses go through the same address map as the CPU
        // (RAM is at kRamBase, just like for the CPU).
        const unsigned dma_cpu_port = next_cpu_port++;
        dma.master_socket.bind(bus.cpu_port(dma_cpu_port));

        bus.add_target(kRamBase,   kRamSize).bind(ram.socket);
        bus.add_target(kUartBase,  kMmioSize).bind(uart.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase,  kPlicSize).bind(plic.socket);
        bus.add_target(kDmaBase,   kMmioSize).bind(dma.target_socket);

        dma.reset_n(dma_reset_n);
        dma.irq(dma_irq);
        dma.irq_abort(dma_irq_abort);

        SC_THREAD(reset_sequence);

        if (!config_path.empty()) {
            std::cout << "dma_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 DMA=0x10070000\n";
            std::cout << "note: DMA master_socket routed through shared bus_router; "
                         "DMA SAR/DAR/CPC use the SAME 0x80000000-based addresses as the CPU\n";
        }
    }

    void reset_sequence() {
        dma_reset_n.write(false);
        wait(sc_core::sc_time(100, sc_core::SC_NS));
        dma_reset_n.write(true);
    }
};

dma_platform_top::dma_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{}

dma_platform_top::~dma_platform_top() = default;

void dma_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string dma_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::dma_platform