#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <systemc>

#include <fu3_bus/bus_system.h>
#include <fu3_crm/crm.h>
#include <fu3_dbg/jtag2axi.h>
#include <fu3_syscsr/syscsr.h>

#include <cva6_wrapper.h>

#include "cpu_debug_hart.h"
#include "fu3_memory.h"
#include "fu3_peripherals.h"
#include "gated_target.h"
#include "not_integrated_target.h"
#include "vp_abi_regs.h"

namespace cdc::platforms::fu3_soc {

// Board inputs (until the board control CLI, T20): levels the board drives
// into the Common v2.1 die pads 2 BOOT, 3 MODE_1, 4 MODE_0, 5 TESTMODE, 13
// TEST_MODE_SEL. The straps reach SYSCSR through the pads; SYSCSR latches
// boot/mode at reset release.
struct board_inputs {
    bool boot = false, mode_0 = false, mode_1 = false;
    bool test_mode = false, test_mode_sel = false;
    // DEBUG_EN pad (die 6): firmware reads it live in FU3_VP_ABI_1
    // BOOT_STATUS[8]; the Boot ROM waits while it is high.
    bool debug_en = false;
};

struct fu3_soc_options {
    // Print every routed bus transaction (fu3_bus trace).
    bool bus_trace = false;
    // Instantiate CPU0 (CVA6) and CPU1 (Ibex) on the CVA6 and IBEX initiator
    // ports. Off for the platform self-test, which uses the IBEX port itself.
    bool cpus = true;
    board_inputs board;
    // PRSTN (power-on reset) is held low this long from time 0.
    sc_core::sc_time por_duration{100, sc_core::SC_NS};
    // TLM global quantum, set before the CPUs exist. A CPU runs ahead of the
    // other processes by at most this much, so it bounds how late a CPU sees
    // an input change (interrupt, reset); WFI and resets wait on events and
    // are not delayed by it. VP choice, not a hardware value.
    sc_core::sc_time quantum{1, sc_core::SC_US};
    // UART whose board-side line prints complete lines to stdout ("[UARTn] ...");
    // -1: none. UART0 is the boot console (HAS: 19,200 bps at boot).
    int console_uart = 0;
    // Bind the DMA0 engine's master to the DMA0 initiator port. Off for benches
    // that drive that port themselves; the engine's accesses then fail.
    bool dma_master = true;
    // Instantiate JTAG2AXI on the DBG initiator port (off for benches that
    // drive that port themselves).
    bool jtag = true;
};

// One bus target of the platform. The address map is fu3_bus::BusConfig::fu3();
// the platform keeps no copy of it.
struct instance_info {
    std::string name;
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    fu3_bus::TargetPath path = fu3_bus::TargetPath::SysBus1Axi;
    bool integrated = false;
    // CRM clock gate and reset of the target, e.g. "CLK_GATE_1[5] SW_RST_N_1[5]".
    std::string domain;
};

// A register an integrated model answers with a known value after reset, for
// the self-test's decode check.
struct model_probe {
    std::uint32_t offset = 0;
    std::uint32_t value = 0;
};

const char* path_name(fu3_bus::TargetPath path);

// Why the system domain was reset (bits, = FU3_VP_RESET_*). Recorded by the
// platform for tests and in the FW-visible RESET_CAUSE register (FU3_VP_ABI_1,
// SYSCSR 0x108, POR-only state).
enum reset_cause : unsigned {
    reset_por = 1u << 0,        // PRSTN
    reset_global = 1u << 1,     // SYSCSR sys_ctrl.sw_global_rst_req (the CRM restarts too)
    reset_software = 1u << 2,   // SYSCSR sys_ctrl.sw_reset_req
    reset_watchdog = 1u << 3,   // WDT0/WDT1 (T09)
    reset_debug = 1u << 4,      // DBG ndmreset (T18)
};
struct reset_record {
    sc_core::sc_time time;
    unsigned causes = 0;
};

// FU3 SoC top. The FU3 bus with every enabled target bound: the on-chip
// memories (T04), the CRM and SYSCSR (T07), the peripherals (T08-T17, in
// fu3_peripherals) and the Debug Module (T18) are integrated; a target without
// a model would answer through a not_integrated_target. Disabled map entries
// (external IP for R2) decode nowhere.
//
// Clock and reset (T07): PRSTN -> CRM -> system domain (bus, memories,
// peripherals, SYSCSR), Ibex (released by the CRM startup), CVA6 (released by
// CRM FCLK_SW[0], which first closes the boot staging window: decision Q02-b)
// and DBG. Every target except the CRM sits behind a gated_target that applies
// its CRM clock gate and reset. Both CPUs run on the CRM CPU clock (FCLK), the
// bus fabric on the CRM bus clock. Interrupts (T08): AP_PLIC to CVA6, INTMAP
// to the Ibex fast interrupts, as fu3_peripherals wires them.
class fu3_soc_top : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(fu3_soc_top);
    fu3_soc_top(sc_core::sc_module_name name, const fu3_soc_options& options);

    fu3_bus::BusSystem& fabric() { return fabric_; }
    fu3_crm::Crm& crm() { return crm_; }
    fu3_peripherals& peripherals() { return *peripherals_; }
    fu3_syscsr::Syscsr& syscsr() { return syscsr_; }
    // FU3_VP_ABI_1 registers of the SYSCSR aperture (include/fu3_vp_abi.h).
    vp_abi_regs& vp_abi() { return *vp_abi_; }
    // CPU models, or nullptr when built without CPUs.
    cdc::cpu::cva6_wrapper* cva6() { return cva6_.get(); }
    cdc::cpu::cva6_wrapper* ibex() { return ibex_.get(); }
    // Debug Module view of hart 0 (CVA6) and hart 1 (Ibex); empty without CPUs.
    std::vector<fu3_dbg::DebugHart*> debug_harts();
    // Hart control shared by the Debug Module and the GDB stub (0 = CVA6,
    // 1 = Ibex), or nullptr without CPUs.
    cpu_debug_hart* debug_hart(unsigned hart)
    {
        return hart == 0 ? cva6_debug_.get() : hart == 1 ? ibex_debug_.get() : nullptr;
    }
    const std::vector<instance_info>& instances() const { return instances_; }
    // Responder bound to `target`, or nullptr when the target is integrated or unknown.
    const not_integrated_target* responder(const std::string& target) const;
    // On-chip memory bound to `target` (ROM, CVA6_ISRAM, IBEX_ISRAM, DSRAM), or nullptr.
    fu3_memory* memory(const std::string& target);
    // Gate in front of `target`, or nullptr (the CRM has none: own reset domain).
    const gated_target* gate(const std::string& target) const;
    // Known register value of an integrated model (CRM, SYSCSR), or nullptr.
    const model_probe* probe(const std::string& target) const;
    // Map entries that are present but disabled (external IP delivered for R2).
    std::vector<const fu3_bus::TargetConfig*> disabled_targets() const;
    // Initiator ports a CPU or DMA model will take; unbound ports stay unused.
    static const std::vector<std::string>& initiator_names();

    // System domain out of reset and the bus open for a new boot epoch.
    bool system_ready() const { return system_ready_ && sys_rst_n_.read(); }
    // Reset of a bus target's domain as the CRM drives it (after the release
    // synchronization to its clock); throws for a target without a domain.
    bool domain_reset_n(const std::string& target) const { return gates_.at(target).reset_n->read(); }
    const sc_core::sc_event& system_ready_event() const { return system_ready_event_; }
    // System resets since time 0, oldest first (the power-on reset included).
    const std::vector<reset_record>& reset_history() const { return reset_history_; }
    // Platform events with their time ("CVA6 released", ...), for order checks.
    const std::vector<std::pair<sc_core::sc_time, std::string>>& events() const { return events_; }
    // CPU clock period both cores run on (CRM FCLK); SC_ZERO_TIME: stopped.
    sc_core::sc_time cpu_clock_period() const { return cpu_period_; }
    // Bus clock period the fabric runs on (CRM bus clock, AXI and APB alike).
    sc_core::sc_time bus_clock_period() const { return bus_period_; }

    // Board inputs while running (from a SystemC process).
    void set_straps(bool boot, bool mode_0, bool mode_1);
    // DEBUG_EN board level (die 6) while running.
    void set_debug_en(bool level);
    // Test stand-ins: a watchdog reset request ORed with the WDT0/WDT1 ones,
    // and a DBG ndmreset ORed with the Debug Module's.
    void drive_watchdog_request(bool level)
    {
        forced_wdt_ = level;
        wdt_changed_.notify(sc_core::SC_ZERO_TIME);
    }
    void drive_ndmreset(bool level)
    {
        forced_ndm_ = level;
        ndm_changed_.notify(sc_core::SC_ZERO_TIME);
    }
    // T18: the Debug Module and the JTAG2AXI bridge (nullptr with jtag off).
    fu3_dbg::DebugModule& dm() { return *dm_; }
    fu3_dbg::Jtag2Axi* jtag() { return jtag_.get(); }

    void print_instances(std::ostream& out) const;
    // Accesses that reached a not-integrated responder or a gated target during the run.
    void print_access_summary(std::ostream& out) const;

private:
    struct gated_block {
        std::unique_ptr<gated_target> gate;
        // reset_n: system/DBG reset AND SW_RST_N bit; clock_on: CLK_GATE bit.
        std::unique_ptr<sc_core::sc_signal<bool>> reset_n, clock_on;
        int gate_reg = -1, gate_bit = 0;   // CLK_GATE_<reg>[bit], -1: none
        int rst_reg = -1, rst_bit = 0;     // SW_RST_N_<reg>[bit], -1: none
        bool dbg_domain = false;           // DBG reset instead of the system reset
        // Release waiting for the destination clock (CRM 3.1.2): time the
        // synchronizer completes once the clock runs (zero: not scheduled).
        bool held_for_clock = false;
        sc_core::sc_time release_at = sc_core::SC_ZERO_TIME;
    };

    void power_on_reset();
    void apply_domains();
    void forward_cva6_irqs();
    void combine_wdt_requests();
    void combine_ndmreset();
    void reset_jtag_registers();   // DBG-domain reset of the JTAG2AXI AXI-side registers
    void forward_ibex_irqs();
    void apply_clock();
    void apply_ibex_reset();
    void cva6_release();
    void system_reset();
    void note_requests();
    void note(const std::string& what);

    board_inputs board_;
    bool forced_wdt_ = false, forced_ndm_ = false;
    sc_core::sc_event ndm_changed_;
    sc_core::sc_event wdt_changed_;
    sc_core::sc_time por_duration_;
    fu3_bus::BusSystem fabric_;
    fu3_crm::Crm crm_;
    fu3_syscsr::Syscsr syscsr_;
    std::unique_ptr<vp_abi_regs> vp_abi_;
    std::unique_ptr<cdc::cpu::cva6_wrapper> cva6_, ibex_;
    std::unique_ptr<cpu_debug_hart> cva6_debug_, ibex_debug_;
    std::unique_ptr<fu3_dbg::DebugModule> dm_;
    std::unique_ptr<fu3_dbg::Jtag2Axi> jtag_;
    sc_core::sc_signal<bool> dm_ndmreset_{"dm_ndmreset"};
    std::vector<instance_info> instances_;
    std::map<std::string, std::unique_ptr<fu3_memory>> memories_;
    std::vector<std::unique_ptr<not_integrated_target>> responders_;
    std::map<std::string, gated_block> gates_;
    sc_core::sc_event reset_sync_;   // a held destination's synchronizer completes
    std::unique_ptr<fu3_peripherals> peripherals_;
    std::map<std::string, model_probe> probes_;

    // Board inputs and stand-in requests may be driven from any process.
    using board_signal = sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS>;
    sc_core::sc_signal<bool> prst_n_{"prst_n"};
    board_signal wdt_reset_req_{"wdt_reset_req"}, ndmreset_{"ndmreset"};
    sc_core::sc_signal<bool> sw_reset_req_{"sw_reset_req"}, sw_global_rst_req_{"sw_global_rst_req"};
    sc_core::sc_signal<bool> sys_rst_n_{"sys_rst_n"}, ibex_rst_n_{"ibex_rst_n"}, cva6_rst_n_{"cva6_rst_n"};
    sc_core::sc_signal<bool> dbg_rst_n_{"dbg_rst_n"};
    sc_core::sc_signal<std::uint32_t> clk_gate_0_{"clk_gate_0"}, clk_gate_1_{"clk_gate_1"};
    sc_core::sc_signal<std::uint32_t> sw_rst_n_0_{"sw_rst_n_0"}, sw_rst_n_1_{"sw_rst_n_1"};

    bool system_ready_ = true;   // bus in a boot epoch (staging open) since elaboration
    sc_core::sc_event system_ready_event_;
    unsigned pending_causes_ = reset_por;
    bool last_requests_[4] = {};
    std::vector<reset_record> reset_history_;
    std::vector<std::pair<sc_core::sc_time, std::string>> events_;
    sc_core::sc_time cpu_period_, bus_period_;
};

} // namespace cdc::platforms::fu3_soc
