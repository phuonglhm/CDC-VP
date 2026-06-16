//Author: QuanNH107
//Verified by: HoangV11

#include "spi_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <spi_tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <uart_tlm.h>

#if defined(CDC_CPU_BACKEND_riscv_vp)
#include <riscv_vp_wrapper.h>
#else
#include <riscv_tlm_wrapper.h>
#endif

namespace cdc::platforms::tests::spi_platform {
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
constexpr std::uint64_t kSpiBase = 0x1002'0000;
constexpr std::uint64_t kSpiSize = 0x0000'1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kSpiPlicSource = 3;
constexpr unsigned kNumPlicSources = kSpiPlicSource;

class spi_loopback_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<spi_loopback_target> socket;

    SC_HAS_PROCESS(spi_loopback_target);

    explicit spi_loopback_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &spi_loopback_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        // Placeholder SPI slave: keep MOSI data unchanged as loopback MISO data.
        // Replace this module when a real SPI peripheral/slave model is provided.
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

} // namespace

struct spi_platform_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::uart_tlm uart;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    cdc::components::spi_tlm spi;
    spi_loopback_target spi_loopback;
    sc_core::sc_signal<bool> plic_source_1_unused;
    sc_core::sc_signal<bool> plic_source_2_unused;
    sc_core::sc_signal<bool> spi_irq;
    sc_core::sc_signal<bool> spi_reset_n;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , spi("spi")
        , spi_loopback("spi_loopback")
        , plic_source_1_unused("plic_source_1_unused")
        , plic_source_2_unused("plic_source_2_unused")
        , spi_irq("spi_irq")
        , spi_reset_n("spi_reset_n")
    {
        SC_THREAD(reset_sequence);

        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kSpiSize).bind(uart.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kSpiBase, kSpiSize).bind(spi.from_apb_socket);

        spi.to_peri_socket.bind(spi_loopback.socket);

        plic_source_1_unused.write(false);
        plic_source_2_unused.write(false);

        plic.irq_in[0](plic_source_1_unused);
        plic.irq_in[1](plic_source_2_unused);
        spi.irq(spi_irq);
        plic.irq_in[kSpiPlicSource - 1](spi_irq);

        // The SPI model reset port is active-low.
        spi.reset_n(spi_reset_n);

        if (!config_path.empty()) {
            std::cout << "spi_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 SPI0=0x10020000\n";
            std::cout << "irq map: SPI0 -> PLIC source 3 -> MEIP\n";
            std::cout << "reset map: SPI reset_n active-low, assert at 0ns, release at 100ns\n";
            std::cout << "spi peripheral: local loopback placeholder bound to to_peri_socket\n";
        }
    }

    void reset_sequence()
    {
        spi_reset_n.write(false);
        wait(sc_core::sc_time(100, sc_core::SC_NS));
        spi_reset_n.write(true);
    }
};

spi_platform_top::spi_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

spi_platform_top::~spi_platform_top() = default;

void spi_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string spi_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::spi_platform
