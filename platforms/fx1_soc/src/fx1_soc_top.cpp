#include "fx1_soc_top.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <bus/bus_system.h>
#include <dma/dma.h>
#include <fx1/clint.h>
#include <fx1/cpu_port_adapter.h>
#include <fx1/exclusive_monitor.h>
#include <fx1/fx1_memory_map.h>
#include <fx1/plic.h>
#include <fx1/sim_control.h>
#include <fx1/sparse_ram.h>
#include <fx1/word_access_guard.h>
#include <fx1_isp/fx1_isp_tlm.h>
#include <riscv_vp_plusplus_wrapper.h>
#include <uart.h>

namespace cdc::platforms::fx1_soc {
namespace {

// RISC-V machine interrupt causes (mip bit numbers).
constexpr unsigned kCauseMachineSoftware = 3;
constexpr unsigned kCauseMachineTimer = 7;
constexpr unsigned kCauseMachineExternal = 11;

// SYS_DMA and ISP reset pulse at power-up (both resets are active low).
const sc_core::sc_time kDmaResetPulse(20, sc_core::SC_NS);

// Exclusive-monitor master IDs of the non-CPU writers (harts use 0..N-1).
constexpr unsigned kMasterSysDma = 0x100;
constexpr unsigned kMasterIspOdma = 0x101;

// Bus target names (bus::BusConfig::fx1()) and the platform-only finisher.
constexpr const char* kSimCtrlTarget = "SIM_CTRL";
const char* const kHartPort[FX1_NUM_HARTS] = {"CPU1", "CPU2"};

// Level-sensitive interrupt line -> cpu_base::set_irq(cause, level).
struct irq_forward : sc_core::sc_module {
    sc_core::sc_in<bool> in{"in"};
    cdc::cpu::cpu_base& cpu;
    unsigned cause;

    SC_HAS_PROCESS(irq_forward);
    irq_forward(sc_core::sc_module_name name, cdc::cpu::cpu_base& target, unsigned irq_cause)
        : sc_module(name), cpu(target), cause(irq_cause)
    {
        SC_METHOD(forward);
        sensitive << in;
        dont_initialize();
    }
    void forward() { cpu.set_irq(cause, in.read()); }
};

bus::BusConfig make_bus_config()
{
    auto cfg = bus::BusConfig::fx1();
    for (auto& target : cfg.targets)
        if (target.name == "SYS_DMA_CSR" || target.name == "ISP_CSR")
            target.enabled = true;  // instantiated below (G3, G4)
    cfg.targets.push_back({kSimCtrlTarget, FX1_SIM_CTRL_BASE, FX1_APB_SLOT_SIZE,
                           bus::TargetPath::Peribus0Apb, true});
    return cfg;
}

} // namespace

struct fx1_soc_top::impl : sc_core::sc_module {
    fx1_soc_options options;
    bus::BusSystem fabric;
    fx1::SparseRam bootrom;
    fx1::SparseRam ddr;
    fx1::Clint clint;
    fx1::Plic plic;
    // uart2_tlm always copies 4 bytes; the guard turns any other access into a
    // guest access fault before it can overrun the payload buffer.
    fx1::WordAccessGuard uart_guard;
    UartTLM uart;
    fx1::SimControl sim;
    // Plan C7: one monitor for the address space. Declared before the CPUs and
    // the guards that refer to it, so it outlives them.
    fx1::ExclusiveMonitor monitor;
    fx1::WriteGuard sys_dma_guard;
    fx1::WriteGuard isp_odma_guard;
    fx1::dma::Dma sys_dma;
    cdc::components::fx1_isp::fx1_isp_tlm isp;

    sc_core::sc_buffer<unsigned char> uart_tx{"uart_tx"};
    // PLIC source lines, index = source ID - 1.
    sc_core::sc_vector<sc_core::sc_signal<bool>> irq_line{"irq_line", FX1_PLIC_NUM_SOURCES - 1};
    sc_core::sc_vector<sc_core::sc_signal<bool>> msip{"msip", FX1_NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> mtip{"mtip", FX1_NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> meip{"meip", FX1_NUM_HARTS};
    // SYS_DMA side band. No peripheral handshake partner exists yet, so the
    // request inputs stay 0 (M2M only); each clear output drives its own signal.
    sc_core::sc_signal<bool> dma_reset_n{"dma_reset_n"};
    sc_core::sc_signal<std::uint32_t> dma_rx_request{"dma_rx_request"}, dma_tx_request{"dma_tx_request"};
    sc_core::sc_signal<std::uint32_t> dma_rx_clear{"dma_rx_clear"}, dma_tx_clear{"dma_tx_clear"};
    sc_core::sc_signal<bool> isp_rst_n{"isp_rst_n"};

    std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> cpus;
    std::vector<std::unique_ptr<fx1::CpuPortAdapter>> cpu_ports;
    std::vector<std::unique_ptr<irq_forward>> forwards;
    std::ofstream uart_log;

    SC_HAS_PROCESS(impl);
    impl(sc_core::sc_module_name name, const fx1_soc_options& opts)
        : sc_module(name)
        , options(opts)
        , fabric("fabric", make_bus_config(), opts.bus_trace)
        , bootrom("bootrom", FX1_BOOTROM_SIZE, sc_core::sc_time(10, sc_core::SC_NS), true)
        , ddr("ddr", FX1_DDR_SIZE, sc_core::sc_time(10, sc_core::SC_NS))
        , clint("clint", FX1_NUM_HARTS, FX1_MTIME_HZ)
        , plic("plic", FX1_PLIC_NUM_SOURCES, FX1_NUM_HARTS, FX1_PLIC_PRIORITY_MAX)
        , uart_guard("uart_guard")
        , uart("uart")
        , sim("sim_control", true, FX1_SIM_CTRL_IRQ_LINES)
        , monitor(FX1_NUM_HARTS, FX1_RESERVATION_GRANULE)
        , sys_dma_guard("sys_dma_guard", monitor, kMasterSysDma)
        , isp_odma_guard("isp_odma_guard", monitor, kMasterIspOdma)
        , sys_dma("sys_dma")
        , isp("isp")
    {
        if (options.firmware.empty())
            throw std::invalid_argument("fx1_soc: a firmware ELF is required");
        if (!options.uart_log.empty()) {
            uart_log.open(options.uart_log);
            if (!uart_log) throw std::runtime_error("fx1_soc: cannot open " + options.uart_log);
        }

        fabric.target("BootROM").bind(bootrom.socket);
        fabric.target("MEMCTL_DDR").bind(ddr.socket);
        fabric.target("CLINT").bind(clint.socket);
        fabric.target("PLIC").bind(plic.socket);
        fabric.target("UART").bind(uart_guard.target);
        uart_guard.out.bind(uart.bus);
        fabric.target(kSimCtrlTarget).bind(sim.socket);
        fabric.target("SYS_DMA_CSR").bind(sys_dma.target_socket);
        // Non-CPU writers reach the fabric through a write guard (plan C7);
        // with the monitor off the guard is still bound but never refuses.
        sys_dma.master_socket.bind(sys_dma_guard.target);
        sys_dma_guard.out.bind(fabric.initiator("SYS_DMA"));
        sys_dma.reset_n(dma_reset_n);
        sys_dma.rx_request(dma_rx_request);
        sys_dma.tx_request(dma_tx_request);
        sys_dma.rx_clear(dma_rx_clear);
        sys_dma.tx_clear(dma_tx_clear);
        // ISP: CSR on PERIBUS_1 (local offsets), two DMA masters, level IRQ.
        fabric.target("ISP_CSR").bind(isp.csr_socket);
        isp.idma_socket.bind(fabric.initiator("ISP_IDMA"));
        isp.odma_socket.bind(isp_odma_guard.target); // IDMA only reads
        isp_odma_guard.out.bind(fabric.initiator("ISP_ODMA"));
        isp.rst_n(isp_rst_n);
        uart.tx(uart_tx);

        // Interrupt sources (fx1_memory_map.h): 1 UART, 2 SYS_DMA, 3 ISP;
        // 4..7 are the VP-only test lines.
        for (unsigned id = 1; id < FX1_PLIC_NUM_SOURCES; ++id) plic.irq_in[id - 1](irq_line[id - 1]);
        uart.irq(irq_line[FX1_IRQ_UART - 1]);
        sys_dma.irq(irq_line[FX1_IRQ_SYS_DMA - 1]);
        isp.irq(irq_line[FX1_IRQ_ISP - 1]);
        for (unsigned i = 0; i < FX1_SIM_CTRL_IRQ_LINES; ++i)
            sim.test_irq[i](irq_line[FX1_SIM_CTRL_IRQ_FIRST_SOURCE - 1 + i]);

        // One shared lock for LR/SC and AMO: both harts share the address space.
        auto bus_lock = cdc::cpu::make_shared_bus_lock();
        cdc::cpu::riscv_vp_plusplus_options cpu_options;
        cpu_options.disable_extensions = "DV";  // FX1 CVA6 configuration: RV32IMAFC
        cpu_options.mtime_source = [this] { return clint.mtime(); };
        if (options.exclusive_monitor) cpu_options.exclusive_monitor = &monitor;
        for (unsigned hart = 0; hart < FX1_NUM_HARTS; ++hart) {
            cdc::cpu::cpu_config cfg;
            cfg.xlen = 32;
            cfg.hart_id = hart;
            // reset_pc left unspecified: every hart starts at the ELF entry.
            auto cpu = std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>(
                sc_core::sc_gen_unique_name("cpu"), cfg, cpu_options);
            cpu->attach_bus_lock(bus_lock);
            cpu->load_elf(options.firmware);
            // Unified I/D socket -> payload completion + guest-fault mapping -> bus port.
            auto port = std::make_unique<fx1::CpuPortAdapter>(sc_core::sc_gen_unique_name("cpu_port"));
            cpu->data_bus().bind(port->target);
            port->out.bind(fabric.initiator(kHartPort[hart]));
            cpu_ports.push_back(std::move(port));

            clint.msip_irq[hart](msip[hart]);
            clint.mtip_irq[hart](mtip[hart]);
            plic.eip[FX1_PLIC_CONTEXT_HART_M(hart)](meip[hart]);
            add_forward(*cpu, msip[hart], kCauseMachineSoftware);
            add_forward(*cpu, mtip[hart], kCauseMachineTimer);
            add_forward(*cpu, meip[hart], kCauseMachineExternal);
            cpus.push_back(std::move(cpu));
        }

        SC_METHOD(on_uart_tx);
        sensitive << uart_tx;
        dont_initialize();
        SC_THREAD(power_on_reset);
    }

    // Hold SYS_DMA and the ISP in reset for a short pulse after power-up,
    // then release them.
    void power_on_reset()
    {
        dma_reset_n.write(false);
        isp_rst_n.write(false);
        wait(kDmaResetPulse);
        dma_reset_n.write(true);
        isp_rst_n.write(true);
    }

    void add_forward(cdc::cpu::cpu_base& cpu, sc_core::sc_signal<bool>& line, unsigned cause)
    {
        auto forward = std::make_unique<irq_forward>(sc_core::sc_gen_unique_name("irq_fwd"), cpu, cause);
        forward->in(line);
        forwards.push_back(std::move(forward));
    }

    void on_uart_tx()
    {
        const char c = static_cast<char>(uart_tx.read());
        if (options.uart_stdout) std::cout << c << std::flush;
        if (uart_log.is_open()) uart_log << c << std::flush;
    }
};

fx1_soc_top::fx1_soc_top(sc_core::sc_module_name name, const fx1_soc_options& options)
    : sc_module(name)
    , impl_(std::make_unique<impl>("impl", options))
{
}

fx1_soc_top::~fx1_soc_top() = default;

fx1_soc_top::result fx1_soc_top::outcome() const
{
    switch (impl_->sim.result()) {
    case fx1::SimControl::Result::pass: return result::pass;
    case fx1::SimControl::Result::fail: return result::fail;
    default: return result::running;
    }
}

std::uint32_t fx1_soc_top::fail_code() const { return impl_->sim.fail_code(); }
unsigned fx1_soc_top::harts() const { return static_cast<unsigned>(impl_->cpus.size()); }
std::uint64_t fx1_soc_top::instret(unsigned hart) const { return impl_->cpus.at(hart)->get_instret(); }
std::uint64_t fx1_soc_top::pc(unsigned hart) const { return impl_->cpus.at(hart)->get_pc(); }

void fx1_soc_top::dump_ddr(std::uint64_t address, std::uint64_t length, const std::string& path) const
{
    if (address < FX1_DDR_BASE || !impl_->ddr.storage().contains(address - FX1_DDR_BASE, length))
        throw std::out_of_range("fx1_soc: dump range is outside DDR");
    std::vector<std::uint8_t> bytes(length);
    impl_->ddr.storage().read(address - FX1_DDR_BASE, bytes.data(), bytes.size());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("fx1_soc: cannot write " + path);
}

std::string fx1_soc_top::atomics_report() const
{
    if (!impl_->options.exclusive_monitor) return "off (upstream VP++ bus-lock model)";
    const auto& s = impl_->monitor.stats();
    std::ostringstream out;
    out << "granule " << impl_->monitor.granule() << " B, lr " << s.reservations << ", sc ok "
        << s.sc_succeeded << " fail " << s.sc_failed << ", cancelled by hart " << s.cancelled_by_cpu
        << " by device " << s.cancelled_by_device << ", device writes held "
        << impl_->sys_dma_guard.writes_held() + impl_->isp_odma_guard.writes_held()
        << ", atomics drained " << s.brackets_waited;
    return out.str();
}

} // namespace cdc::platforms::fx1_soc
