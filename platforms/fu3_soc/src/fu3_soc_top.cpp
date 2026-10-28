#include "fu3_soc_top.h"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include <fu3_cpu/cva6_config.h>
#include <fu3_cpu/ibex_config.h>
#include <fu3_plic/registers.h>
#include <fu3_dbg/registers.h>

namespace cdc::platforms::fu3_soc {

namespace {

constexpr unsigned fu3_cpu_cause_software = 3, fu3_cpu_cause_timer = 7, fu3_cpu_cause_external = 11;

// On-chip memories integrated by T04. ROM is read-only for functional masters
// (its image is loaded through the debug path); access rules are the bus's.
struct memory_spec {
    const char* target;
    bool read_only;
};
constexpr memory_spec kMemories[] = {
    {"ROM", true},
    {"CVA6_ISRAM", false},
    {"IBEX_ISRAM", false},
    {"DSRAM", false},
};

// CRM clock gate and software reset of each target (CSR_CRM_fu3_v2; CLK_GATE_0
// and SW_RST_N_0 order their bits differently, CLK_GATE_1 and SW_RST_N_1 share
// one order). Every target is also in the system reset domain, IBEX_DBG in the
// DBG one instead. VP bindings: QSPI_XIP and AP_QSPIC share the qspi bits;
// UART3-UART7 use bits 25-29 of CLK_GATE_1/SW_RST_N_1 (FU3_VP_ABI_1, Q05-d;
// not in CSR v2). The CRM is not gated: its own reset is PRSTN and the global
// request. A target missing here fails the construction.
struct domain_spec {
    const char* target;
    int gate_reg, gate_bit, rst_reg, rst_bit;
    bool dbg_domain;
};
constexpr int kNone = -1;
constexpr domain_spec kDomains[] = {
    {"ROM", 0, 0, 0, 0, false},
    {"CVA6_ISRAM", 0, 1, 0, 1, false},
    {"IBEX_ISRAM", 0, 1, 0, 1, false},
    {"DSRAM", 0, 2, 0, 2, false},
    {"AES", 0, 3, 0, 3, false},
    {"QSPI_XIP", 0, 4, 0, 4, false},
    {"AP_QSPIC", 0, 4, 0, 4, false},
    {"HYPERBUS", 0, 5, 0, 7, false},
    {"EDHOC_OSCORE", 0, 6, 0, 6, false},
    {"AP_DMA", 0, 7, 0, 5, false},
    {"IBEX_DBG", 0, 8, kNone, 0, true},
    {"WDT0", 1, 1, 1, 1, false},
    {"WDT1", 1, 2, 1, 2, false},
    {"ADC", 1, 3, 1, 3, false},
    {"RNG", 1, 4, 1, 4, false},
    {"SYSCSR", 1, 5, 1, 5, false},
    {"AP_PLIC", 1, 6, 1, 6, false},
    {"GPIOC", 1, 7, 1, 7, false},
    {"GPIOB", 1, 8, 1, 8, false},
    {"GPIOA", 1, 9, 1, 9, false},
    {"I2C", 1, 10, 1, 10, false},
    {"UART0", 1, 11, 1, 11, false},
    {"UART1", 1, 12, 1, 12, false},
    {"UART2", 1, 13, 1, 13, false},
    {"SPI0", 1, 14, 1, 14, false},
    {"SPI1", 1, 15, 1, 15, false},
    {"SPI2", 1, 16, 1, 16, false},
    {"SPI3", 1, 17, 1, 17, false},
    {"TIMER0", 1, 18, 1, 18, false},
    {"TIMER1", 1, 19, 1, 19, false},
    {"TIMER2", 1, 20, 1, 20, false},
    {"TIMER3", 1, 21, 1, 21, false},
    {"MAILBOX_IBEX", 1, 22, 1, 22, false},
    {"MAILBOX_CVA6", 1, 23, 1, 23, false},
    {"AP_HBCTRL", 1, 24, 1, 24, false},
    // FU3_VP_ABI_1 (Q05-d): CLK_GATE_1 / SW_RST_N_1 bits 25-29.
    {"UART3", 1, 25, 1, 25, false},
    {"UART4", 1, 26, 1, 26, false},
    {"UART5", 1, 27, 1, 27, false},
    {"UART6", 1, 28, 1, 28, false},
    {"UART7", 1, 29, 1, 29, false},
};

const domain_spec& domain_of(const std::string& target)
{
    for (const auto& d : kDomains)
        if (target == d.target)
            return d;
    throw std::logic_error("fu3_soc: no CRM clock/reset domain for bus target " + target);
}

std::string describe(const domain_spec& d)
{
    std::ostringstream out;
    if (d.gate_reg != kNone)
        out << "CLK_GATE_" << d.gate_reg << '[' << d.gate_bit << "] ";
    if (d.rst_reg != kNone)
        out << "SW_RST_N_" << d.rst_reg << '[' << d.rst_bit << "] ";
    out << (d.dbg_domain ? "DBG reset" : "system reset");
    if (d.gate_reg == kNone && !d.dbg_domain)
        out << " only (gate/reset bits pending, Q05-d)";
    return out.str();
}

// A model register with a known value after reset (self-test decode check).
constexpr std::uint32_t kCrmVersion = 0x0001'0000;    // CRM VERSION, offset 0x0000
constexpr std::uint32_t kSyscsrOscPad = 0x13;         // SYSCSR osc_pad_ctrl reset, offset 0x0080

} // namespace

// The FU3 map (fu3_bus::BusConfig::fu3()) with the endpoint contracts the
// integrated models require: HyperRAM takes whole bursts (configure_bus).
fu3_bus::BusConfig platform_bus_config()
{
    auto config = fu3_bus::BusConfig::fu3();
    fu3_hyperram::Controller::configure_bus(config);
    // FU3_VP_ABI_1 access profile (approved 2026-10-26, decision Q02-d):
    // DMA0 reaches data memories and the FIFO data registers of the endpoints
    // DMA0 serves, in their direction, and nothing else (no ROM, ISRAM, debug
    // partitions or control CSRs). CVA6 does not execute from QSPI XIP
    // (CVA6-Config #54: executable region is CVA6_ISRAM only).
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> uart_data{{0x0, 0x1}};   // THR/RBR
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> spi_tx{{0x0c, 0x10}}, spi_rx{{0x10, 0x14}};
    for (auto& initiator : config.initiators) {
        if (initiator.name == "DMA0") {
            initiator.grants_only = true;
            initiator.grants = {
                {"DSRAM", true, true, false, {}},     {"HYPERBUS", true, true, false, {}},
                {"QSPI_XIP", true, false, false, {}}, {"UART1", true, true, false, uart_data},
                {"UART2", true, true, false, uart_data}, {"SPI0", false, true, false, spi_tx},
                {"SPI0", true, false, false, spi_rx},  {"SPI1", false, true, false, spi_tx},
                {"SPI1", true, false, false, spi_rx},
            };
        }
        if (initiator.name == "CVA6")
            initiator.no_execute = {"QSPI_XIP"};
    }
    return config;
}

const char* path_name(fu3_bus::TargetPath path)
{
    switch (path) {
    case fu3_bus::TargetPath::SysBus1Axi:
        return "SYSBUS_1";
    case fu3_bus::TargetPath::SysBus0Axi:
        return "SYSBUS_0";
    case fu3_bus::TargetPath::Peribus0Apb:
        return "PERIBUS_0";
    case fu3_bus::TargetPath::Peribus1Apb:
        return "PERIBUS_1";
    }
    return "?";
}

fu3_soc_top::fu3_soc_top(sc_core::sc_module_name name, const fu3_soc_options& options)
    : sc_core::sc_module(name), board_(options.board), por_duration_(options.por_duration),
      fabric_("fabric", platform_bus_config(), options.bus_trace), crm_("crm"), syscsr_("syscsr")
{
    // Every target except the CRM gets its gate and its reset/clock signals first,
    // so the peripheral models can take them at construction.
    for (const auto& target : fabric_.configuration().targets) {
        if (!target.enabled || target.name == "CRM")
            continue;
        const auto& d = domain_of(target.name);
        auto& block = gates_[target.name];
        block.gate = std::make_unique<gated_target>(("gate_" + target.name).c_str(), target.name);
        block.reset_n = std::make_unique<sc_core::sc_signal<bool>>(("rst_n_" + target.name).c_str());
        block.clock_on = std::make_unique<sc_core::sc_signal<bool>>(("clk_on_" + target.name).c_str());
        block.gate_reg = d.gate_reg;
        block.gate_bit = d.gate_bit;
        block.rst_reg = d.rst_reg;
        block.rst_bit = d.rst_bit;
        block.dbg_domain = d.dbg_domain;
    }
    peripherals_ = std::make_unique<fu3_peripherals>("peripherals", [this](const std::string& target) {
        auto& block = gates_.at(target);
        return domain_signals{block.reset_n.get(), block.clock_on.get()};
    });

    tlm::tlm_global_quantum::instance().set(options.quantum);
    if (options.cpus) {
        namespace fu3 = cdc::cpu::fu3;
        cva6_ = std::make_unique<cdc::cpu::cva6_wrapper>("cva6", fu3::cva6_cpu_config(),
                                                         fu3::kCva6DisableExtensions, fu3::cva6_core_options_fu3());
        ibex_ = std::make_unique<cdc::cpu::cva6_wrapper>("ibex", fu3::ibex_cpu_config(),
                                                         fu3::kIbexDisableExtensions, fu3::ibex_core_options());
        cva6_->data_bus().bind(fabric_.initiator("CVA6"));
        ibex_->data_bus().bind(fabric_.initiator("IBEX"));
        cva6_->set_reset(true);   // released by the CRM (Ibex) and the handoff (CVA6)
        ibex_->set_reset(true);
        cva6_debug_ = std::make_unique<cpu_debug_hart>(*cva6_);
        ibex_debug_ = std::make_unique<cpu_debug_hart>(*ibex_);
    }

    // T18: one Debug Module for both harts (decision Q04-c: hartsel 0 = CVA6,
    // 1 = Ibex) on IBEX_DBG; JTAG2AXI (VP JTAG profile, Q04-a) masters the DBG port.
    dm_ = std::make_unique<fu3_dbg::DebugModule>("DM", debug_harts());
    dm_->ndmreset(dm_ndmreset_);
    // The DM and the bridge registers are in the DBG domain: watchdog, global
    // and POR resets clear them, ndmreset does not (decision Q05-e).
    dm_->reset_n(*gates_.at("IBEX_DBG").reset_n);
    if (options.jtag) {
        jtag_ = std::make_unique<fu3_dbg::Jtag2Axi>("JTAG");
        jtag_->axi.bind(fabric_.initiator("DBG"));
    }

    // BusSystem requires every enabled target to be bound before sc_start.
    for (const auto& target : fabric_.configuration().targets) {
        if (!target.enabled)
            continue;
        if (target.name == "CRM") {
            fabric_.target(target.name).bind(crm_.socket);
            probes_[target.name] = {FU3_CRM_VERSION_OFFSET, kCrmVersion};
            instances_.push_back({target.name, target.base, target.size, target.path, true,
                                  "own reset: PRSTN, global request"});
            continue;
        }
        const auto& d = domain_of(target.name);
        auto& block = gates_[target.name];
        fabric_.target(target.name).bind(block.gate->socket);

        bool integrated = false;
        for (const auto& m : kMemories) {
            if (target.name != m.target)
                continue;
            auto mem = std::make_unique<fu3_memory>(target.name.c_str(), target.size, m.read_only);
            block.gate->out.bind(mem->socket);
            memories_.emplace(target.name, std::move(mem));
            integrated = true;
        }
        if (target.name == "SYSCSR") {
            // FU3_VP_ABI_1 registers (0x100-0x123) in front of the SYSCSR v1 model.
            vp_abi_ = std::make_unique<vp_abi_regs>("vp_abi", peripherals_->intmap());
            block.gate->out.bind(vp_abi_->socket);
            vp_abi_->out.bind(syscsr_.socket);
            probes_[target.name] = {fu3_syscsr::kOscPadCtrl, kSyscsrOscPad};
            integrated = true;
        }
        if (target.name == "IBEX_DBG") {
            block.gate->out.bind(dm_->socket);
            probes_[target.name] = {FU3_DBG_DM_OFFSET(FU3_DBG_HARTINFO), 0};
            integrated = true;
        }
        if (auto* model = peripherals_->socket(target.name)) {
            block.gate->out.bind(*model);
            const auto& p = peripherals_->probes();
            if (const auto it = p.find(target.name); it != p.end())
                probes_[target.name] = {it->second.offset, it->second.value};
            integrated = true;
        }
        if (!integrated) {
            responders_.push_back(
                std::make_unique<not_integrated_target>(("ni_" + target.name).c_str(), target.name));
            block.gate->out.bind(responders_.back()->socket);
        }
        instances_.push_back({target.name, target.base, target.size, target.path, integrated, describe(d)});
    }

    crm_.prst_n(prst_n_);
    crm_.wdt_reset_req(wdt_reset_req_);
    crm_.cpu_reset_req(ndmreset_);
    crm_.sw_reset_req(sw_reset_req_);
    crm_.sw_global_rst_req(sw_global_rst_req_);
    crm_.sys_rst_n(sys_rst_n_);
    crm_.ibex_rst_n(ibex_rst_n_);
    crm_.cva6_rst_n(cva6_rst_n_);
    crm_.dbg_rst_n(dbg_rst_n_);
    crm_.irq(peripherals_->plic_source[FU3_PLIC_ID_CRM - 1]);   // PLIC ID 1; INTMAP via forward below
    crm_.clk_gate_0(clk_gate_0_);
    crm_.clk_gate_1(clk_gate_1_);
    crm_.sw_rst_n_0(sw_rst_n_0_);
    crm_.sw_rst_n_1(sw_rst_n_1_);

    syscsr_.reset_n(*gates_.at("SYSCSR").reset_n);
    syscsr_.boot(peripherals_->strap_boot);
    syscsr_.mode_0(peripherals_->strap_mode_0);
    syscsr_.mode_1(peripherals_->strap_mode_1);
    syscsr_.test_mode(peripherals_->test_mode);
    syscsr_.test_mode_sel(peripherals_->test_select);
    syscsr_.sw_reset_req(sw_reset_req_);
    vp_abi_->reset_n(*gates_.at("SYSCSR").reset_n);
    vp_abi_->boot(peripherals_->strap_boot);
    vp_abi_->mode_0(peripherals_->strap_mode_0);
    vp_abi_->mode_1(peripherals_->strap_mode_1);
    vp_abi_->test_mode(peripherals_->test_mode);
    vp_abi_->test_select(peripherals_->test_select);
    vp_abi_->debug_en(peripherals_->debug_en);
    vp_abi_->cva6_software(peripherals_->cva6_software);
    vp_abi_->ibex_software(peripherals_->ibex_software);
    syscsr_.sw_global_rst_req(sw_global_rst_req_);

    if (options.console_uart >= 0 && options.console_uart < int(fu3_peripherals::kUarts))
        peripherals_->uart_peer(unsigned(options.console_uart))
            .set_echo(&std::cout, "[UART" + std::to_string(options.console_uart) + "] ");
    if (options.dma_master) {
        peripherals_->dma_master().bind(fabric_.initiator("DMA0"));   // T17: DMA0 master
    } else {
        responders_.push_back(std::make_unique<not_integrated_target>("ni_DMA0_master", "DMA0 master"));
        peripherals_->dma_master().bind(responders_.back()->socket);
    }
    SC_THREAD(power_on_reset);
    SC_METHOD(apply_domains);
    sensitive << sys_rst_n_ << dbg_rst_n_ << clk_gate_0_ << clk_gate_1_ << sw_rst_n_0_ << sw_rst_n_1_ << reset_sync_;
    SC_METHOD(reset_jtag_registers);
    sensitive << dbg_rst_n_.negedge_event();
    dont_initialize();
    SC_METHOD(apply_clock);
    sensitive << crm_.changed_event();
    SC_METHOD(apply_ibex_reset);
    sensitive << ibex_rst_n_ << system_ready_event_;
    dont_initialize();
    SC_THREAD(cva6_release);
    SC_THREAD(system_reset);
    SC_METHOD(combine_ndmreset);
    sensitive << dm_ndmreset_ << ndm_changed_;
    SC_METHOD(combine_wdt_requests);
    sensitive << peripherals_->wdt_reset_request << wdt_changed_;
    SC_METHOD(forward_cva6_irqs);
    sensitive << peripherals_->cva6_external << peripherals_->cva6_timer << peripherals_->cva6_software;
    dont_initialize();
    SC_METHOD(forward_ibex_irqs);
    for (auto& s : peripherals_->ibex_irq)
        sensitive << s;
    dont_initialize();
    SC_METHOD(note_requests);
    sensitive << wdt_reset_req_ << ndmreset_ << sw_reset_req_ << sw_global_rst_req_;
    dont_initialize();
}

void fu3_soc_top::note(const std::string& what)
{
    events_.emplace_back(sc_core::sc_time_stamp(), what);
}

void fu3_soc_top::power_on_reset()
{
    set_straps(board_.boot, board_.mode_0, board_.mode_1);
    peripherals_->set_board_pad(5, board_.test_mode);
    peripherals_->set_board_pad(13, board_.test_mode_sel);   // die 13 TEST_MODE_SEL (Common v2.1)
    set_debug_en(board_.debug_en);
    prst_n_.write(false);
    reset_history_.push_back({sc_core::sc_time_stamp(), reset_por});
    vp_abi_->power_on();
    pending_causes_ = 0;
    note("PRSTN asserted");
    wait(por_duration_);
    prst_n_.write(true);
    note("PRSTN released");
}

void fu3_soc_top::set_debug_en(bool level)
{
    peripherals_->set_board_pad(6, level);   // die 6 DEBUG_EN (Common v2.1)
}

void fu3_soc_top::set_straps(bool boot, bool mode_0, bool mode_1)
{
    peripherals_->set_board_pad(2, boot);
    peripherals_->set_board_pad(3, mode_1);
    peripherals_->set_board_pad(4, mode_0);
}

void fu3_soc_top::apply_domains()
{
    const std::uint32_t gate[2] = {clk_gate_0_.read(), clk_gate_1_.read()};
    const std::uint32_t rst[2] = {sw_rst_n_0_.read(), sw_rst_n_1_.read()};
    sc_core::sc_time next_sync = sc_core::SC_ZERO_TIME;   // earliest pending synchronizer release
    for (auto& [name, block] : gates_) {
        const bool domain_n = block.dbg_domain ? dbg_rst_n_.read() : sys_rst_n_.read();
        bool reset_n = domain_n, clock_on = true, software_n = true;
        if (block.gate_reg != kNone)
            clock_on = (gate[block.gate_reg] >> block.gate_bit) & 1;
        if (block.rst_reg != kNone)
            software_n = (rst[block.rst_reg] >> block.rst_bit) & 1;
        reset_n = reset_n && software_n;
        // CRM Spec V1.1 3.1.2: assertion is asynchronous, release is
        // synchronized to the destination clock. A destination whose release
        // comes while its clock is gated stays in reset until the clock runs
        // and then for two more of its edges (the synchronizer), so even a
        // synchronous-reset IP sees the reset. A release with the clock running
        // takes effect at once (synchronizer latency not modelled there).
        const auto now = sc_core::sc_time_stamp();
        if (!reset_n) {
            block.held_for_clock = false;
            block.release_at = sc_core::SC_ZERO_TIME;
        } else if (!block.reset_n->read() && (!clock_on || block.held_for_clock)) {
            if (!clock_on) {
                block.held_for_clock = true;
                block.release_at = sc_core::SC_ZERO_TIME;
                reset_n = false;
            } else if (block.release_at == sc_core::SC_ZERO_TIME) {
                block.release_at = now + bus_period_ * 2.0;
                reset_n = false;
            } else if (now < block.release_at) {
                reset_n = false;
            } else {
                block.held_for_clock = false;
                block.release_at = sc_core::SC_ZERO_TIME;
            }
        }
        block.reset_n->write(reset_n);
        if (block.release_at > now && (next_sync == sc_core::SC_ZERO_TIME || block.release_at < next_sync))
            next_sync = block.release_at;
        block.clock_on->write(clock_on);
        // Refusal reason (gated_target): the domain reset aborts accesses; a
        // gate or a SW_RST_N bit is software's doing; what is left is the
        // release synchronizer, still part of the reset.
        using gs = gated_target::state;
        block.gate->set_state(reset_n && clock_on ? gs::available
                              : !domain_n         ? gs::domain_reset
                              : !software_n       ? gs::software_reset
                              : !clock_on         ? gs::clock_gated
                                                  : gs::domain_reset);
    }
    if (next_sync != sc_core::SC_ZERO_TIME)
        reset_sync_.notify(next_sync - sc_core::sc_time_stamp());
}

void fu3_soc_top::apply_clock()
{
    // The bus fabric runs on the CRM bus clock (HAS 5.1: one 200 MHz clock for
    // the system and peripheral buses, CRM CLK.div): every AXI stage, APB cycle
    // and router decode takes one bus period. At 0 Hz the fabric keeps its last
    // period; the CPUs, on the same clock branch, are stopped then.
    if (const std::uint64_t bus_hz = crm_.bus_clock_hz()) {
        const sc_core::sc_time period(1.0 / static_cast<double>(bus_hz), sc_core::SC_SEC);
        if (period != bus_period_) {
            bus_period_ = period;
            fabric_.set_clock(period, period, period);
            peripherals_->set_bus_clock(period);
            note("bus clock " + std::to_string(bus_hz) + " Hz");
        }
    }
    // Both CPUs run on FCLK, the CRM CPU clock (decision Q01-g): 20 MHz OSC
    // after reset, 400 MHz once software selects the PLL configured for it.
    const std::uint64_t hz = crm_.cpu_clock_hz();
    const sc_core::sc_time period = hz ? sc_core::sc_time(1.0 / static_cast<double>(hz), sc_core::SC_SEC)
                                       : sc_core::SC_ZERO_TIME;
    if (period == cpu_period_)
        return;
    cpu_period_ = period;
    if (cva6_) {
        cva6_->set_clock_period(period);
        ibex_->set_clock_period(period);
    }
    note("CPU clock " + (hz ? std::to_string(hz) + " Hz" : std::string("stopped")));
}

void fu3_soc_top::apply_ibex_reset()
{
    // The CRM startup releases Ibex; a new boot epoch also needs the bus reset
    // (staging reopened) that follows every system reset.
    if (!ibex_)
        return;
    const bool run = ibex_rst_n_.read() && system_ready_;
    if (run == !ibex_->in_reset())
        return;
    ibex_->set_reset(!run);
    if (!run) {
        dm_->notify_hart_reset(1);   // DM havereset (Q04-c)
        ibex_debug_->notify_reset();
    }
    note(run ? "Ibex released" : "Ibex held in reset");
}

void fu3_soc_top::reset_jtag_registers()
{
    if (jtag_)
        jtag_->reset_registers();
}

void fu3_soc_top::combine_ndmreset()
{
    ndmreset_.write(dm_ndmreset_.read() || forced_ndm_);
}

void fu3_soc_top::combine_wdt_requests()
{
    wdt_reset_req_.write(peripherals_->wdt_reset_request.read() || forced_wdt_);
}

void fu3_soc_top::forward_cva6_irqs()
{
    // Common v2.1: PLIC target 0 -> irq_i[0] (cause 11), TIMER0 -> time_irq_i
    // (7), SYSCSR software IRQ -> ipi_i (3, FU3_VP_ABI_1 SWIRQ bit 0).
    if (!cva6_)
        return;
    cva6_->set_irq(fu3_cpu_cause_external, peripherals_->cva6_external.read());
    cva6_->set_irq(fu3_cpu_cause_timer, peripherals_->cva6_timer.read());
    cva6_->set_irq(fu3_cpu_cause_software, peripherals_->cva6_software.read());
}

void fu3_soc_top::forward_ibex_irqs()
{
    // INTMAP outputs -> irq_fast_i[14:0] (cause 16 + k), irq_external_i (11),
    // irq_software_i (3), irq_timer_i (7); irq_nm_i has no source (tied low).
    if (!ibex_)
        return;
    for (unsigned o = 0; o < fu3_plic::kOutNmi; ++o)
        ibex_->set_irq(fu3_plic::kIbexCause[o], peripherals_->ibex_irq[o].read());
}

void fu3_soc_top::cva6_release()
{
    if (!cva6_)
        return;
    while (true) {
        wait(cva6_rst_n_.value_changed_event());
        if (!cva6_rst_n_.read()) {
            cva6_->set_reset(true);
            dm_->notify_hart_reset(0);
            cva6_debug_->notify_reset();
            note("CVA6 held in reset");
            continue;
        }
        // Decision Q02-b: the CRM FCLK_SW[0] write releasing CVA6 closes the
        // staging window first. Later requests see the final policy at once;
        // the IBEX requests accepted before it (the FCLK_SW write among them)
        // complete before CVA6 runs. A local CVA6 reset does not reopen it.
        if (fabric_.staging()) {
            fabric_.close_staging();
            note("boot staging closed");
        }
        if (cva6_rst_n_.read() && !fabric_.staging()) {
            cva6_->set_reset(false);
            note("CVA6 released");
        }
    }
}

void fu3_soc_top::system_reset()
{
    // Every system reset (watchdog, software, debug, global) starts a new boot
    // epoch: the masters are held by their resets, the bus drains and reopens
    // staging, then Ibex may be released again.
    while (true) {
        wait(sys_rst_n_.negedge_event());
        system_ready_ = false;
        reset_history_.push_back({sc_core::sc_time_stamp(), pending_causes_});
        vp_abi_->record_reset(pending_causes_);   // FU3_VP_ABI_1 RESET_CAUSE
        pending_causes_ = 0;
        note("system reset");
        fabric_.quiesce();
        fabric_.reset();
        fabric_.resume();
        system_ready_ = true;
        system_ready_event_.notify(sc_core::SC_ZERO_TIME);
    }
}

void fu3_soc_top::note_requests()
{
    const bool now[4] = {sw_global_rst_req_.read(), sw_reset_req_.read(), wdt_reset_req_.read(), ndmreset_.read()};
    constexpr unsigned cause[4] = {reset_global, reset_software, reset_watchdog, reset_debug};
    for (unsigned i = 0; i < 4; ++i) {
        if (now[i] && !last_requests_[i])
            pending_causes_ |= cause[i];
        last_requests_[i] = now[i];
    }
}

std::vector<fu3_dbg::DebugHart*> fu3_soc_top::debug_harts()
{
    if (!cva6_debug_)
        return {};
    return {cva6_debug_.get(), ibex_debug_.get()};
}

const not_integrated_target* fu3_soc_top::responder(const std::string& target) const
{
    for (const auto& r : responders_)
        if (r->block() == target)
            return r.get();
    return nullptr;
}

fu3_memory* fu3_soc_top::memory(const std::string& target)
{
    const auto it = memories_.find(target);
    return it == memories_.end() ? nullptr : it->second.get();
}

const gated_target* fu3_soc_top::gate(const std::string& target) const
{
    const auto it = gates_.find(target);
    return it == gates_.end() ? nullptr : it->second.gate.get();
}

const model_probe* fu3_soc_top::probe(const std::string& target) const
{
    const auto it = probes_.find(target);
    return it == probes_.end() ? nullptr : &it->second;
}

std::vector<const fu3_bus::TargetConfig*> fu3_soc_top::disabled_targets() const
{
    std::vector<const fu3_bus::TargetConfig*> out;
    for (const auto& target : fabric_.configuration().targets)
        if (!target.enabled)
            out.push_back(&target);
    return out;
}

const std::vector<std::string>& fu3_soc_top::initiator_names()
{
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (const auto& initiator : fu3_bus::BusConfig::fu3().initiators)
            out.push_back(initiator.name);
        return out;
    }();
    return names;
}

void fu3_soc_top::print_instances(std::ostream& out) const
{
    // Revision first, so a copied table always says which map it follows.
    out << "FU3 SoC bus map: " << fabric_.configuration().revision << " (fu3_bus::BusConfig::fu3())\n"
        << "  " << std::left << std::setw(14) << "target" << std::setw(13) << "base" << std::setw(13) << "size"
        << std::setw(11) << "path" << std::setw(60) << "state" << "clock gate / reset\n";
    auto row = [&out](const std::string& name, std::uint64_t base, std::uint64_t size, fu3_bus::TargetPath path) {
        out << "  " << std::left << std::setw(14) << name << "0x" << std::right << std::hex << std::setw(8)
            << std::setfill('0') << base << "   0x" << std::setw(8) << size << std::setfill(' ') << std::dec
            << "   " << std::left << std::setw(11) << path_name(path);
    };
    for (const auto& i : instances_) {
        row(i.name, i.base, i.size, i.path);
        out << std::setw(60) << (i.integrated ? "integrated" : "NOT INTEGRATED (access -> TLM_GENERIC_ERROR_RESPONSE)")
            << i.domain << '\n';
    }
    for (const auto* t : disabled_targets()) {
        row(t->name, t->base, t->size, t->path);
        out << "DISABLED, external IP for R2 (access -> TLM_ADDRESS_ERROR_RESPONSE)\n";
    }
    out << "Initiators: " << (cva6_ ? "CVA6 (RV32IMFC, hart 0), IBEX (RV32IMC+Zcb, hart 1)" : "no CPU model")
        << "; DMA0 (AP_DMA engine); DBG (JTAG2AXI)\n"
        << "Clock and reset: CRM (PRSTN, system/Ibex/CVA6/DBG resets, CPU clock, gates); CVA6 released by "
           "CRM FCLK_SW[0] after the staging window closes\n"
        << "Interrupts: AP_PLIC (31 sources, Common v2.1) -> CVA6 external; INTMAP -> Ibex fast IRQs; TIMER0 -> "
           "CVA6 timer; WDT0 warning -> Ibex\n"
        << "Reset requests: WDT0/WDT1 -> CRM watchdog reset; DM ndmreset -> CRM debug reset (DBG kept)\n"
        << std::right;
}

void fu3_soc_top::print_access_summary(std::ostream& out) const
{
    bool any = false;
    for (const auto& r : responders_) {
        if (!r->accesses() && !r->debug_accesses())
            continue;
        if (!any)
            out << "Accesses to not-integrated blocks:\n";
        any = true;
        out << "  " << r->block() << ": " << r->accesses() << " bus, " << r->debug_accesses() << " debug\n";
    }
    bool gated = false;
    for (const auto& [name, block] : gates_) {
        if (!gated && (block.gate->refused() || block.gate->aborted_by_reset()))
            out << "Gate refusals (functional accesses answered with an error):\n";
        gated = gated || block.gate->refused() || block.gate->aborted_by_reset();
        if (block.gate->refused())
            out << "  " << name << ": " << block.gate->refused() << " refused while gated or held by SW_RST_N\n";
        if (block.gate->aborted_by_reset())
            out << "  " << name << ": " << block.gate->aborted_by_reset() << " aborted by a domain reset\n";
    }
}

} // namespace cdc::platforms::fu3_soc
