//Author: QuanNH107
//Verified: trangmn20

#include "clkmgr_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <clkmgr.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <uart.h>

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::tests::clkmgr_platform {
namespace {

using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;
constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kClkmgrBase = 0x1006'0000;
constexpr std::uint64_t kMmioSize = 0x1000;
constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

constexpr unsigned kNumPlicSources = 1;

} // namespace

struct clkmgr_platform_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    Clkmgr clkmgr;
    sc_core::sc_signal<bool> plic_source1_tieoff;
    sc_core::sc_vector<sc_core::sc_signal<bool>> idle_i;
    sc_core::sc_signal<bool> io_clk_byp_req;
    sc_core::sc_signal<bool> io_clk_byp_ack;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/5, /*num_initiators=*/cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
       , uart_tx("uart_tx")
       , uart_irq("uart_irq")
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlicSources)
        , clkmgr("clkmgr")
        , plic_source1_tieoff("plic_source1_tieoff")
        , idle_i("idle_i", 4)
        , io_clk_byp_req("io_clk_byp_req")
        , io_clk_byp_ack("io_clk_byp_ack")
    {
        SC_THREAD(mock_transactional_idle);
        SC_THREAD(mock_ast_handshake);

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
        bus.add_target(kClkmgrBase, kMmioSize).bind(clkmgr.socket);

        // Keep the ADC reference PLIC source-1 map as clkmgr0.irq_out. The
        // current clkmgr_tlm model has no interrupt output, so tie it low.
        plic_source1_tieoff.write(false);
        plic.irq_in[0](plic_source1_tieoff);

        for (unsigned i = 0; i < idle_i.size(); ++i) {
            clkmgr.idle_i[i](idle_i[i]);
        }
        clkmgr.io_clk_byp_req_o(io_clk_byp_req);
        clkmgr.io_clk_byp_ack_i(io_clk_byp_ack);

        // The standalone VP has no life-cycle controller, so run the clkmgr in
        // DEV mode to let firmware exercise software external-clock switching.
        clkmgr.set_lc_state(Clkmgr::LcState::Dev);

        if (!config_path.empty()) {
            std::cout << "clkmgr_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 CLKMGR=0x10060000\n";
            std::cout << "irq map: CLKMGR0 -> PLIC source 1 -> MEIP "
                      << "(tied low in this model)\n";
            std::cout << "lc state: DEV, enabling software external-clock switch tests\n";
            std::cout << "mock env: idle_i low at reset, high after 1 ms; "
                      << "AST ack follows request after 1 ms\n";
        }
    }

    void mock_transactional_idle()
    {
        for (unsigned i = 0; i < idle_i.size(); ++i) {
            idle_i[i].write(false);
        }

        wait(sc_core::sc_time(1, sc_core::SC_MS));

        for (unsigned i = 0; i < idle_i.size(); ++i) {
            idle_i[i].write(true);
        }
        std::cout << sc_core::sc_time_stamp()
                  << " [CLKMGR MOCK] transactional idle_i asserted\n";
    }

    void mock_ast_handshake()
    {
        io_clk_byp_ack.write(false);

        for (;;) {
            wait(io_clk_byp_req.value_changed_event());

            if (io_clk_byp_req.read()) {
                std::cout << sc_core::sc_time_stamp()
                          << " [CLKMGR MOCK] AST bypass request observed\n";
                wait(sc_core::sc_time(1, sc_core::SC_MS));
                if (io_clk_byp_req.read()) {
                    io_clk_byp_ack.write(true);
                    std::cout << sc_core::sc_time_stamp()
                              << " [CLKMGR MOCK] AST bypass ack asserted\n";
                }
            } else {
                io_clk_byp_ack.write(false);
                std::cout << sc_core::sc_time_stamp()
                          << " [CLKMGR MOCK] AST bypass ack deasserted\n";
            }
        }
    }
};

clkmgr_platform_top::clkmgr_platform_top(sc_core::sc_module_name name,
                                         std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

clkmgr_platform_top::~clkmgr_platform_top() = default;

void clkmgr_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string clkmgr_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::clkmgr_platform
