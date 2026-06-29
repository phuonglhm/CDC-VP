#include "pmu_platform_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <clint_tlm.h>
#include <memory_tlm.h>
#include <plic_tlm.h>
#include <pmu.h>
#include <uart.h>

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::tests::pmu_platform {
namespace {

using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

// Memory map của platform.
// CPU truy cập địa chỉ -> bus_router decode -> chuyển tới IP tương ứng.
constexpr std::uint64_t kClintBase = 0x0200'0000;
constexpr std::uint64_t kClintSize = 0x0001'0000;

constexpr std::uint64_t kPlicBase = 0x0C00'0000;
constexpr std::uint64_t kPlicSize = 0x0040'0000;

constexpr std::uint64_t kUartBase = 0x1000'0000;

constexpr std::uint64_t kPmuBase = 0x1007'0000;

constexpr std::uint64_t kMmioSize = 0x1000;

constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0010'0000;

// PMU chỉ đưa 1 interrupt ra PLIC: wakeup_irq.
// PLIC source id = 1 tương ứng irq_in[0].
constexpr unsigned kNumPlicSources = 1;

} // namespace

struct pmu_platform_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    // Các module phần cứng của platform.
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    UartTLM uart;
   sc_core::sc_buffer<unsigned char> uart_tx;
   sc_core::sc_signal<bool> uart_irq;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;
    cdc::components::Pwrmgr pmu;

    // Input signals cho PMU.
    sc_core::sc_signal<bool> por_rst_n;
    sc_core::sc_signal<bool> core_sleeping;
    sc_core::sc_signal<bool> otp_done;
    sc_core::sc_signal<bool> lc_done;
    sc_core::sc_signal<bool> rom_done;
    sc_core::sc_signal<bool> rom_good;
    sc_core::sc_signal<bool> flash_idle;
    sc_core::sc_signal<bool> lc_test_state;
    sc_core::sc_signal<bool> main_pok;

    sc_core::sc_signal<sc_dt::sc_bv<pwrmgr_reg::NUM_WAKEUPS>> wakeups;
    sc_core::sc_signal<sc_dt::sc_bv<pwrmgr_reg::NUM_RESET_REQS>> rstreqs;

    sc_core::sc_signal<bool> ndmreset_req;
    sc_core::sc_signal<bool> sw_rst_req;
    sc_core::sc_signal<bool> esc_rx;
    sc_core::sc_signal<bool> esc_clk_alive;

    // Output signals từ PMU.
    sc_core::sc_signal<bool> ast_main_pd_n;
    sc_core::sc_signal<bool> rst_lc_n;
    sc_core::sc_signal<bool> clk_en_2nd;
    sc_core::sc_signal<bool> fetch_en;
    sc_core::sc_signal<bool> strap_o;
    sc_core::sc_signal<bool> low_power_o;
    sc_core::sc_signal<bool> sys_rst_n;
    sc_core::sc_signal<bool> wakeup_irq;

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
        , pmu("pmu")
        , por_rst_n("por_rst_n")
        , core_sleeping("core_sleeping")
        , otp_done("otp_done")
        , lc_done("lc_done")
        , rom_done("rom_done")
        , rom_good("rom_good")
        , flash_idle("flash_idle")
        , lc_test_state("lc_test_state")
        , main_pok("main_pok")
        , wakeups("wakeups")
        , rstreqs("rstreqs")
        , ndmreset_req("ndmreset_req")
        , sw_rst_req("sw_rst_req")
        , esc_rx("esc_rx")
        , esc_clk_alive("esc_clk_alive")
        , ast_main_pd_n("ast_main_pd_n")
        , rst_lc_n("rst_lc_n")
        , clk_en_2nd("clk_en_2nd")
        , fetch_en("fetch_en")
        , strap_o("strap_o")
        , low_power_o("low_power_o")
        , sys_rst_n("sys_rst_n")
        , wakeup_irq("wakeup_irq")
    {
        SC_THREAD(pmu_env_sequence);

        // Nối CPU vào bus.
        // riscv_vp: unified bus -> 1 port.
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        // Memory map.
        // Số add_target phải khớp num_targets=5.
        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kMmioSize).bind(uart.bus);
      uart.tx(uart_tx);
      uart.irq(uart_irq);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase, kPlicSize).bind(plic.socket);
        bus.add_target(kPmuBase, kMmioSize).bind(pmu.tl_socket);

        // Bind input PMU.
        pmu.por_rst_n(por_rst_n);
        pmu.core_sleeping(core_sleeping);
        pmu.otp_done(otp_done);
        pmu.lc_done(lc_done);
        pmu.rom_done(rom_done);
        pmu.rom_good(rom_good);
        pmu.flash_idle(flash_idle);
        pmu.lc_test_state(lc_test_state);
        pmu.main_pok(main_pok);

        pmu.wakeups(wakeups);
        pmu.rstreqs(rstreqs);

        pmu.ndmreset_req(ndmreset_req);
        pmu.sw_rst_req(sw_rst_req);
        pmu.esc_rx(esc_rx);
        pmu.esc_clk_alive(esc_clk_alive);

        // Bind output PMU.
        pmu.ast_main_pd_n(ast_main_pd_n);
        pmu.rst_lc_n(rst_lc_n);
        pmu.clk_en_2nd(clk_en_2nd);
        pmu.fetch_en(fetch_en);
        pmu.strap_o(strap_o);
        pmu.low_power_o(low_power_o);
        pmu.sys_rst_n(sys_rst_n);
        pmu.wakeup_irq(wakeup_irq);

        // PMU wakeup interrupt -> PLIC source 1 -> CPU MEIP.
        plic.irq_in[0](wakeup_irq);

        if (!config_path.empty()) {
            std::cout << "pmu_platform config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "memory map: RAM=0x80000000 UART=0x10000000 "
                      << "CLINT=0x02000000 PLIC=0x0C000000 PMU=0x10070000\n";
            std::cout << "irq map: PMU wakeup_irq -> PLIC source 1 -> MEIP\n";
            std::cout << "boot env: por_rst_n release at 100ns, "
                      << "otp_done/lc_done/rom_done asserted after boot delay\n";
        }
    }

    void pmu_env_sequence()
    {
        // Giá trị ban đầu của môi trường PMU.
        por_rst_n.write(false);

        core_sleeping.write(false);

        otp_done.write(false);
        lc_done.write(false);
        rom_done.write(false);

        rom_good.write(true);
        flash_idle.write(true);
        lc_test_state.write(true);
        main_pok.write(true);

        sc_dt::sc_bv<pwrmgr_reg::NUM_WAKEUPS> zero_wakeups;
        zero_wakeups = 0;
        wakeups.write(zero_wakeups);

        sc_dt::sc_bv<pwrmgr_reg::NUM_RESET_REQS> zero_rstreqs;
        zero_rstreqs = 0;
        rstreqs.write(zero_rstreqs);

        ndmreset_req.write(false);
        sw_rst_req.write(false);
        esc_rx.write(false);
        esc_clk_alive.write(true);

        // POR reset active-low.
        wait(sc_core::sc_time(100, sc_core::SC_NS));
        por_rst_n.write(true);

        // Giả lập các IP boot dependency đã init xong.
        wait(sc_core::sc_time(50, sc_core::SC_NS));
        otp_done.write(true);

        wait(sc_core::sc_time(20, sc_core::SC_NS));
        lc_done.write(true);

        wait(sc_core::sc_time(50, sc_core::SC_NS));
        rom_done.write(true);
    }
};

pmu_platform_top::pmu_platform_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

pmu_platform_top::~pmu_platform_top() = default;

void pmu_platform_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

std::string pmu_platform_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::tests::pmu_platform
