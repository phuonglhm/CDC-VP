#include "vp_fx1_full_soc_top.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <bus_router.h>
#include <memory_tlm.h>
#include <clint_tlm.h>
#include <plic_tlm.h>
#include <uart.h>        // UartTLM            (global)
#include <uart_host_bridge.h> // cdc::components::uart_host_bridge
#include <i2c.h>         // i2c               (global)
#include <spi_tlm.h>     // cdc::components::spi_tlm
#include <timer.h>       // cdc::components::Timer
#include <wdt_tlm.h>     // cdc::components::wdt_tlm
#include <pwm.h>         // PWM               (global)
#include <dma_tlm.h>     // cdc::components::dma_tlm
#include <trng_tlm.h>    // cdc::components::trng_tlm
#include <clkmgr.h>      // Clkmgr            (global)
#include <pmu.h>         // cdc::components::Pwrmgr, pwrmgr_reg::*
#include <dmic.h>        // cdc::components::DmicTLM
#include <otp.h>         // otp               (global)
#include <qspi_tlm.h>    // cdc::components::qspi_tlm
#include <flash_nor_tlm.h>
#include <rtc_tlm.h>     // cdc::components::rtc_tlm
#include <adc_tlm.h>     // cdc::components::adc_tlm
#include <gpio_tlm.h>    // cdc::components::gpio_tlm
#if CDC_ENABLE_SAURIA_NPU_V4
#include <npu_tlm.h>
#endif

#include <riscv_vp_wrapper.h>

namespace cdc::platforms::vp_fx1_full_soc {
namespace {

using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

// ── SoC address map (docs/peripheral_memory_map.md) ─────────────────────────
constexpr std::uint64_t kClintBase = 0x0200'0000, kClintSize = 0x1'0000;
constexpr std::uint64_t kPlicBase  = 0x0C00'0000, kPlicSize  = 0x40'0000;
constexpr std::uint64_t kMmio      = 0x1000;
constexpr std::uint64_t kUart0 = 0x1000'0000, kI2c0  = 0x1001'0000, kSpi0   = 0x1002'0000;
constexpr std::uint64_t kTimer0= 0x1003'0000, kWdt0  = 0x1004'0000, kPwm0   = 0x1005'0000;
constexpr std::uint64_t kDma0  = 0x1006'0000, kTrng0 = 0x1007'0000, kCmu0   = 0x1008'0000;
constexpr std::uint64_t kPmu0  = 0x1009'0000, kDmic0 = 0x100A'0000, kOtp0   = 0x100B'0000;
constexpr std::uint64_t kQspi0 = 0x100C'0000;
#if CDC_ENABLE_SAURIA_NPU_V4
constexpr std::uint64_t kNpu0  = 0x1020'0000, kAccelMmio = 0x10'0000;
#endif
constexpr std::uint64_t kUart1 = 0x1010'0000, kI2c1  = 0x1011'0000, kSpi1   = 0x1012'0000;
constexpr std::uint64_t kTimer1= 0x1013'0000, kRtc0  = 0x1014'0000;
constexpr std::uint64_t kAdc0  = 0x1015'0000;   // resolves map "ADC base TBD"
constexpr std::uint64_t kGpio0 = 0x1016'0000;   // boot-mode strap on pin 1
#if CDC_ENABLE_SAURIA_NPU_V4
static_assert(kGpio0 + kMmio <= kNpu0,
              "NPU0 must not overlap the instance-1 peripheral MMIO region");
#endif
constexpr std::uint64_t kRamBase = 0x8000'0000, kRamSize = 0x1000'0000; // 256 MiB
// ROM-code boot flow (firmware team's boot-sequence spec):
constexpr std::uint64_t kBootromBase = 0x0000'0000, kBootromSize = 0x1'0000;  // 64 KiB
constexpr std::uint64_t kIflashBase  = 0x0400'0000, kIflashSize  = 0x40'0000; // 4 MiB
// ISP0 0x100D and VPU0 0x100E remain reserved (planned, not bound).

constexpr unsigned kNumPlic = 31;            // PLIC sources 1..31 (id 0 reserved)
constexpr std::size_t kFlashSize = 16u * 1024u * 1024u;
#if CDC_ENABLE_SAURIA_NPU_V4
constexpr unsigned kNpuInstances = 1;
#else
constexpr unsigned kNpuInstances = 0;
#endif

// Minimal SPI off-chip peripheral: a benign target for spi.to_peri_socket so the
// controller's initiator socket is bound. Returns OK, echoes nothing.
class spi_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<spi_dummy> socket;
    explicit spi_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {
        socket.register_b_transport(this, &spi_dummy::b_transport);
    }
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// Idle PDM stimulus source: binds DMIC's pdm_target_socket (no samples driven).
class pdm_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<pdm_dummy> socket;
    explicit pdm_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {}
};

} // namespace

struct vp_fx1_full_soc_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    // ── Core / infrastructure ───────────────────────────────────────────────
    cpu_backend_t cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    cdc::components::memory_tlm bootrom;   // ROM-code image, entry 0x0
    cdc::components::memory_tlm iflash;    // internal code flash (XIP-able ROM)
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;

    // ── Peripherals ─────────────────────────────────────────────────────────
    UartTLM uart0, uart1;
    cdc::components::uart_host_bridge uart0_host; // boot-flow UART download path
    i2c i2c0, i2c1;
    cdc::components::spi_tlm spi0, spi1;
    cdc::components::flash_nor_tlm spi_flash0; // NOR behind SPI0 (boot-flow SPI download)
    spi_dummy spi1_peri;
    cdc::components::Timer timer0, timer1;
    cdc::components::wdt_tlm wdt0;
    PWM pwm0;
    cdc::components::dma_tlm dma0;
    cdc::components::trng_tlm trng0;
    Clkmgr cmu0;
    cdc::components::Pwrmgr pmu0;
    cdc::components::DmicTLM dmic0;
    pdm_dummy pdm_src;
    otp otp0;
    cdc::components::qspi_tlm qspi0;
    cdc::components::flash_nor_tlm flash0;
    cdc::components::rtc_tlm rtc0;
    cdc::components::adc_tlm adc0;
    cdc::components::gpio_tlm gpio0;   // pin 1 = ROM-code boot-mode strap
#if CDC_ENABLE_SAURIA_NPU_V4
    cdc::components::npu_tlm npu0;
#endif

    // ── Signals ─────────────────────────────────────────────────────────────
    sc_core::sc_buffer<unsigned char> uart0_tx, uart1_tx;
    sc_core::sc_signal<bool> uart0_irq, uart1_irq;
    sc_core::sc_signal<bool> i2c0_irq, i2c1_irq;
    sc_core::sc_signal<bool> spi0_irq, spi1_irq, spi0_rst, spi1_rst;
    // SPI0 chip-select to the NOR flash. MANY_WRITERS: driven from spi0's
    // reset method and from register writes running in the CPU's process.
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> spi0_cs_n;
    sc_core::sc_signal<bool> timer0_irq, timer1_irq, timer0_rst, timer1_rst, timer0_ext, timer1_ext;
    sc_core::sc_signal<bool> wdt0_irq, wdt0_rst, wdt0_rsto;
    sc_core::sc_signal<bool> pwm0_out;
    sc_core::sc_signal<std::uint32_t> dma0_irq;
    sc_core::sc_signal<bool> dma0_irq_nonzero, dma0_irq_abort, dma0_rst;
    sc_core::sc_signal<bool> trng0_irq, trng0_rst;
    /* trng_tlm declares (and requires binding of) a clk input but never uses
     * it; a real sc_clock here costs 2e8 edges per simulated second for
     * nothing, so bind a static-low signal instead. */
    sc_core::sc_signal<bool> trng0_clk;
    sc_core::sc_signal<bool> dmic0_irq, dmic0_rst;
    sc_core::sc_signal<bool> otp0_irq;
    sc_core::sc_signal<bool> qspi0_irq, qspi0_rst;
    sc_core::sc_signal<bool> rtc0_irq, rtc0_rst;
    sc_core::sc_signal<bool> adc0_irq, adc0_rst;
#if CDC_ENABLE_SAURIA_NPU_V4
    sc_core::sc_signal<bool> npu0_irq, npu0_rst;
#endif

    // CMU / clkmgr environment
    sc_core::sc_vector<sc_core::sc_signal<bool>> cmu_idle;
    sc_core::sc_signal<bool> cmu_byp_req, cmu_byp_ack;

    // PLIC reserved-source tie-low
    sc_core::sc_signal<bool> tie_low;

    // PMU / pwrmgr environment (driven by pmu_env_sequence)
    sc_core::sc_signal<bool> por_rst_n, core_sleeping, otp_done, lc_done, rom_done, rom_good,
        flash_idle, lc_test_state, main_pok, ndmreset_req, sw_rst_req, esc_rx, esc_clk_alive,
        ast_main_pd_n, rst_lc_n, clk_en_2nd, fetch_en, strap_o, low_power_o, sys_rst_n, wakeup_irq;
    sc_core::sc_signal<sc_dt::sc_bv<pwrmgr_reg::NUM_WAKEUPS>> wakeups;
    sc_core::sc_signal<sc_dt::sc_bv<pwrmgr_reg::NUM_RESET_REQS>> rstreqs;

    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", /*num_targets=*/25 + kNpuInstances,
              /*num_initiators=*/(cpu.has_unified_bus() ? 1u : 2u) +
                  1u + kNpuInstances /* DMA + optional NPU master */)
        , ram("ram", kRamSize)
        , bootrom("bootrom", kBootromSize, /*read_only=*/true)
        , iflash("iflash", kIflashSize, /*read_only=*/true)
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlic)
        , uart0("uart0"), uart1("uart1")
        , uart0_host("uart0_host")
        , i2c0("i2c0"), i2c1("i2c1")
        , spi0("spi0"), spi1("spi1")
        , spi_flash0("spi_flash0", kFlashSize)
        , spi1_peri("spi1_peri")
        , timer0("timer0", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, sc_core::sc_time(10, sc_core::SC_NS))
        , timer1("timer1", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, sc_core::sc_time(10, sc_core::SC_NS))
        , wdt0("wdt0", sc_core::sc_time(10, sc_core::SC_NS))
        , pwm0("pwm0")
        , dma0("dma0")
        , trng0("trng0")
        , cmu0("cmu0")
        , pmu0("pmu0")
        , dmic0("dmic0")
        , pdm_src("pdm_src")
        , otp0("otp0")
        , qspi0("qspi0")
        , flash0("flash0", kFlashSize)
        , rtc0("rtc0")
        , adc0("adc0")
        , gpio0("gpio0")
#if CDC_ENABLE_SAURIA_NPU_V4
        , npu0("npu0", sc_core::sc_time(1.25, sc_core::SC_NS))
#endif
        , uart0_tx("uart0_tx"), uart1_tx("uart1_tx")
        , trng0_clk("trng0_clk")
        , cmu_idle("cmu_idle", 4)
    {
        // ── Upstream initiators: CPU instruction/data + DMA master ───────────
        unsigned port = 0;
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(port++));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(port++));
            cpu.data_bus().bind(bus.cpu_port(port++));
        }
        dma0.master_socket.bind(bus.cpu_port(port++));
#if CDC_ENABLE_SAURIA_NPU_V4
        npu0.master_socket.bind(bus.cpu_port(port++));
#endif

        // ── Downstream targets (25 plus optional NPU0) ────────────────────────
        bus.add_target(kBootromBase, kBootromSize).bind(bootrom.socket);
        bus.add_target(kIflashBase,  kIflashSize ).bind(iflash.socket);
        bus.add_target(kRamBase,   kRamSize ).bind(ram.socket);
        bus.add_target(kClintBase, kClintSize).bind(clint.socket);
        bus.add_target(kPlicBase,  kPlicSize ).bind(plic.socket);
        bus.add_target(kUart0, kMmio).bind(uart0.bus);
        bus.add_target(kI2c0,  kMmio).bind(i2c0.socket);
        bus.add_target(kSpi0,  kMmio).bind(spi0.from_apb_socket);
        bus.add_target(kTimer0,kMmio).bind(timer0.socket);
        bus.add_target(kWdt0,  kMmio).bind(wdt0.target_socket);
        bus.add_target(kPwm0,  kMmio).bind(pwm0.socket);
        bus.add_target(kDma0,  kMmio).bind(dma0.target_socket);
        bus.add_target(kTrng0, kMmio).bind(trng0.socket);
        bus.add_target(kCmu0,  kMmio).bind(cmu0.socket);
        bus.add_target(kPmu0,  kMmio).bind(pmu0.tl_socket);
        bus.add_target(kDmic0, kMmio).bind(dmic0.bus_target_socket);
        bus.add_target(kOtp0,  kMmio).bind(otp0.socket);
        bus.add_target(kQspi0, kMmio).bind(qspi0.from_apb_socket);
#if CDC_ENABLE_SAURIA_NPU_V4
        bus.add_target(kNpu0,  kAccelMmio).bind(npu0.target_socket);
#endif
        bus.add_target(kUart1, kMmio).bind(uart1.bus);
        bus.add_target(kI2c1,  kMmio).bind(i2c1.socket);
        bus.add_target(kSpi1,  kMmio).bind(spi1.from_apb_socket);
        bus.add_target(kTimer1,kMmio).bind(timer1.socket);
        bus.add_target(kRtc0,  kMmio).bind(rtc0.socket);
        bus.add_target(kAdc0,  kMmio).bind(adc0.socket);
        bus.add_target(kGpio0, kMmio).bind(gpio0.socket);

        // ── Per-IP secondary sockets / outputs ───────────────────────────────
        uart0.tx(uart0_tx); uart0.irq(uart0_irq);
        uart1.tx(uart1_tx); uart1.irq(uart1_irq);
        SC_METHOD(monitor_uart0); sensitive << uart0_tx; dont_initialize();
        // Host bridge sits on UART0's pin side; idles unless configured
        // via --uart0-socket / --uart0-rx-file.
        uart0_host.rx_out(uart0.rx);
        uart0_host.tx_in(uart0_tx);

        i2c0.irq(i2c0_irq); i2c1.irq(i2c1_irq);

        // SPI0 master ↔ NOR flash byte-stream face; CS from SSPCSR via signal.
        spi0.to_peri_socket.bind(spi_flash0.from_spi_socket);
        spi0.cs_n(spi0_cs_n);
        spi0.irq(spi0_irq); spi0.reset_n(spi0_rst);
        SC_METHOD(spi0_cs_bridge); sensitive << spi0_cs_n;
        spi1.to_peri_socket.bind(spi1_peri.socket); spi1.irq(spi1_irq); spi1.reset_n(spi1_rst);

        timer0.irq_out(timer0_irq); timer0.reset_n(timer0_rst); timer0.extin(timer0_ext);
        timer1.irq_out(timer1_irq); timer1.reset_n(timer1_rst); timer1.extin(timer1_ext);

        wdt0.irq(wdt0_irq); wdt0.reset_n(wdt0_rst); wdt0.reset_o(wdt0_rsto);

        pwm0.pwm_out(pwm0_out);

        dma0.reset_n(dma0_rst); dma0.irq(dma0_irq); dma0.irq_abort(dma0_irq_abort);
        SC_METHOD(dma_irq_bridge); sensitive << dma0_irq; dont_initialize();

        trng0.reset_n(trng0_rst); trng0.irq_out(trng0_irq); trng0.clk(trng0_clk);

        for (int i = 0; i < 4; ++i) cmu0.idle_i[i](cmu_idle[i]);
        cmu0.io_clk_byp_req_o(cmu_byp_req);
        cmu0.io_clk_byp_ack_i(cmu_byp_ack);

        dmic0.reset_n(dmic0_rst); dmic0.irq_out(dmic0_irq);
        pdm_src.socket.bind(dmic0.pdm_target_socket); // idle PDM source

        otp0.irq_out(otp0_irq);

        qspi0.to_flash_socket.bind(flash0.from_qspi_socket);
        qspi0.irq(qspi0_irq); qspi0.reset_n(qspi0_rst);

        rtc0.irq_out(rtc0_irq); rtc0.reset_n(rtc0_rst);
        adc0.irq_out(adc0_irq); adc0.reset_n(adc0_rst);
#if CDC_ENABLE_SAURIA_NPU_V4
        npu0.irq_out(npu0_irq); npu0.reset_n(npu0_rst);
#endif

        // PMU environment
        pmu0.por_rst_n(por_rst_n);       pmu0.core_sleeping(core_sleeping);
        pmu0.otp_done(otp_done);         pmu0.lc_done(lc_done);
        pmu0.rom_done(rom_done);         pmu0.rom_good(rom_good);
        pmu0.flash_idle(flash_idle);     pmu0.lc_test_state(lc_test_state);
        pmu0.main_pok(main_pok);         pmu0.wakeups(wakeups);
        pmu0.rstreqs(rstreqs);           pmu0.ndmreset_req(ndmreset_req);
        pmu0.sw_rst_req(sw_rst_req);     pmu0.esc_rx(esc_rx);
        pmu0.esc_clk_alive(esc_clk_alive);
        pmu0.ast_main_pd_n(ast_main_pd_n); pmu0.rst_lc_n(rst_lc_n);
        pmu0.clk_en_2nd(clk_en_2nd);     pmu0.fetch_en(fetch_en);
        pmu0.strap_o(strap_o);           pmu0.low_power_o(low_power_o);
        pmu0.sys_rst_n(sys_rst_n);       pmu0.wakeup_irq(wakeup_irq);
        SC_THREAD(pmu_env_sequence);

        // ── PLIC source bindings (id = index + 1; see peripheral_memory_map) ──
        plic.irq_in[0](uart0_irq);          // 1  UART0
        plic.irq_in[1](i2c0_irq);           // 2  I2C0
        plic.irq_in[2](spi0_irq);           // 3  SPI0
        plic.irq_in[3](timer0_irq);         // 4  TIMER0
        plic.irq_in[4](wdt0_irq);           // 5  WDT0
        plic.irq_in[5](tie_low);            // 6  PWM0 (reserved, no IRQ)
        plic.irq_in[6](dma0_irq_nonzero);   // 7  DMA0 done/vector
        plic.irq_in[7](dma0_irq_abort);     // 8  DMA0 abort
        plic.irq_in[8](trng0_irq);          // 9  TRNG0
        plic.irq_in[9](tie_low);            // 10 CMU0 (reserved)
        plic.irq_in[10](wakeup_irq);        // 11 PMU0 wakeup
        plic.irq_in[11](dmic0_irq);         // 12 DMIC0
        plic.irq_in[12](otp0_irq);          // 13 OTP0
        plic.irq_in[13](qspi0_irq);         // 14 QSPI0
        plic.irq_in[14](tie_low);           // 15 ISP0 (reserved)
        plic.irq_in[15](tie_low);           // 16 VPU0 (reserved)
#if CDC_ENABLE_SAURIA_NPU_V4
        plic.irq_in[16](npu0_irq);          // 17 NPU0
#else
        plic.irq_in[16](tie_low);           // 17 NPU0 (optional, disabled)
#endif
        plic.irq_in[17](uart1_irq);         // 18 UART1
        plic.irq_in[18](i2c1_irq);          // 19 I2C1
        plic.irq_in[19](spi1_irq);          // 20 SPI1
        plic.irq_in[20](timer1_irq);        // 21 TIMER1
        plic.irq_in[21](rtc0_irq);          // 22 RTC0
        plic.irq_in[22](adc0_irq);          // 23 ADC0 (resolves map "ADC IRQ TBD")
        for (unsigned i = 23; i < kNumPlic; ++i) plic.irq_in[i](tie_low); // 24..31 reserved

        // ── Static reset / environment levels ────────────────────────────────
        tie_low.write(false);
        spi0_rst.write(true);  spi1_rst.write(true);
        timer0_rst.write(true); timer1_rst.write(true);
        timer0_ext.write(false); timer1_ext.write(false);
        wdt0_rst.write(true);  dma0_rst.write(true);
        trng0_rst.write(true); dmic0_rst.write(true);
        qspi0_rst.write(true); rtc0_rst.write(true); adc0_rst.write(true);
#if CDC_ENABLE_SAURIA_NPU_V4
        npu0_rst.write(true);
#endif
        cmu_byp_ack.write(false);
        for (int i = 0; i < 4; ++i) cmu_idle[i].write(false);

        if (!config_path.empty()) {
            std::cout << "VP_FX1_Full_SoC config: " << config_path << '\n';
            std::cout << "cpu backend: " << cpu.backend_name() << '\n';
            std::cout << "RAM 256 MiB @ 0x80000000 | MMIO @ 0x1000_0000 region | "
                         "CLINT @ 0x0200_0000 | PLIC @ 0x0C00_0000\n";
            std::cout << "BOOTROM 64 KiB @ 0x0 | IFLASH 4 MiB @ 0x0400_0000 | "
                         "GPIO0 @ 0x1016_0000 (pin1 = boot strap)\n";
            std::cout << "IPs: UARTx2 I2Cx2 SPIx2 TIMERx2 WDT PWM DMA TRNG CMU PMU "
                         "DMIC OTP QSPI(+flash) RTC ADC GPIO | SPI0+NOR (boot) | "
#if CDC_ENABLE_SAURIA_NPU_V4
                         "NPUv4 | ISP/VPU reserved\n";
#else
                         "ISP/VPU/NPU reserved (enable optional SAURIA build)\n";
#endif
        }
    }

    void monitor_uart0() { std::cout << uart0_tx.read(); std::cout.flush(); }

    // Active-low line -> flash's "selected" state; deassert edge ends a
    // NOR command (resets the flash's SPI state machine).
    void spi0_cs_bridge() { spi_flash0.spi_cs(!spi0_cs_n.read()); }

    void dma_irq_bridge() { dma0_irq_nonzero.write(dma0_irq.read() != 0u); }

    // Drive the pwrmgr through a benign power-up handshake so its FSM leaves
    // reset (outputs rst_lc_n/sys_rst_n are observed only, not routed to reset).
    void pmu_env_sequence() {
        core_sleeping.write(false);
        rom_good.write(true);
        flash_idle.write(true);
        lc_test_state.write(true);
        main_pok.write(true);
        wakeups.write(sc_dt::sc_bv<pwrmgr_reg::NUM_WAKEUPS>(0));
        rstreqs.write(sc_dt::sc_bv<pwrmgr_reg::NUM_RESET_REQS>(0));
        ndmreset_req.write(false);
        sw_rst_req.write(false);
        esc_rx.write(false);
        esc_clk_alive.write(true);

        por_rst_n.write(false);
        wait(1, sc_core::SC_US);
        por_rst_n.write(true);
        wait(1, sc_core::SC_US);
        otp_done.write(true);
        wait(1, sc_core::SC_US);
        lc_done.write(true);
        wait(1, sc_core::SC_US);
        rom_done.write(true);
    }
};

vp_fx1_full_soc_top::vp_fx1_full_soc_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

vp_fx1_full_soc_top::~vp_fx1_full_soc_top() = default;

void vp_fx1_full_soc_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

void vp_fx1_full_soc_top::load_int_flash(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SC_REPORT_ERROR("vp_fx1_full_soc", ("cannot open int-flash image: " + path).c_str());
        return;
    }
    std::vector<std::uint8_t> img((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    if (img.size() > impl_->iflash.size()) {
        SC_REPORT_ERROR("vp_fx1_full_soc", ("int-flash image larger than 4 MiB window: " + path).c_str());
        return;
    }
    impl_->iflash.load(img.data(), img.size());
    std::cout << "IFLASH loaded " << img.size() << " bytes from " << path << '\n';
}

void vp_fx1_full_soc_top::load_spi_flash(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SC_REPORT_ERROR("vp_fx1_full_soc", ("cannot open spi-flash image: " + path).c_str());
        return;
    }
    std::vector<std::uint8_t> img((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    if (img.size() > impl_->spi_flash0.size()) {
        SC_REPORT_ERROR("vp_fx1_full_soc", ("spi-flash image larger than flash: " + path).c_str());
        return;
    }
    impl_->spi_flash0.load(img.data(), img.size());
    std::cout << "SPI0 NOR loaded " << img.size() << " bytes from " << path << '\n';
}

void vp_fx1_full_soc_top::set_boot_pin(bool high)
{
    impl_->gpio0.set_pin(1, high);
}

void vp_fx1_full_soc_top::set_uart0_socket(std::uint16_t port, bool wait_for_client)
{
    impl_->uart0_host.listen_on(port, wait_for_client);
}

void vp_fx1_full_soc_top::set_uart0_rx_file(const std::string& path,
                                            std::uint64_t start_delay_us)
{
    impl_->uart0_host.replay_file(
        path, sc_core::sc_time(static_cast<double>(start_delay_us),
                              sc_core::SC_US));
}

std::string vp_fx1_full_soc_top::backend_name() const
{
    return impl_->cpu.backend_name();
}

} // namespace cdc::platforms::vp_fx1_full_soc
