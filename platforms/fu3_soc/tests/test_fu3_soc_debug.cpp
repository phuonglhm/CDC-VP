// Plan T18: a debugger on the platform's JTAG2AXI bridge (VP JTAG profile,
// decision Q04-a) reaching the bus and, through the Debug Module at IBEX_DBG
// (one DM, hartsel 0 = CVA6, 1 = Ibex: Q04-c), both CPUs:
//   - IDCODE; JTAG -> bus write/read in DSRAM; a decode error is reported;
//   - JTAG releases CVA6 through CRM FCLK_SW, then halts it, reads and writes
//     a GPR with abstract commands, resumes it and halts it again;
//   - Ibex halt/resume on hartsel 1;
//   - interrupts reach the CPU inputs: with both harts halted, TIMER0/1/2 and
//     the PLIC (programmed over JTAG) set the expected mip bits, read back with
//     abstract commands (platform forwarding of the PLIC and INTMAP outputs);
//   - ndmreset resets the system but not DBG (DM state such as DATA0 kept);
//     havereset is reported and the platform records a debug reset;
//   - a watchdog reset includes DBG (decision Q05-e): DMCONTROL/DATA0 back to
//     reset values, the halt request released, the bridge status cleared.
// The CPUs run small loops; the CVA6 debug partition holds a park loop.

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include <systemc>

#include <fu3_crm/registers.h>
#include <fu3_cpu/cva6_config.h>
#include <fu3_cpu/ibex_config.h>
#include <fu3_dbg/registers.h>
#include <fu3_plic/registers.h>
#include <fu3_timer/registers.h>
#include <fu3_vp_abi.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

// addi t0,t0,1; j .-4
constexpr std::uint32_t kCountLoop[] = {0x00128293, 0xffdff06f};
// Ibex ROM stub at 0x80, standing in for the Boot ROM step of Common v2.1
// Ibex-Config #7-#9: with RLB = 1 the ROM replaces the reset staging entry of
// IBEX_ISRAM (RW) by the final W^X one (RX), which also makes the debug code
// partition 0x0006_0000 executable (MemoryMap #7: R/X in Ibex debug mode).
//   li t0, 0x9b9d9b8d; csrw pmpcfg1, t0   (entry 4 L|TOR|R|X, entries 5-7 kept)
//   1: wfi; j 1b
constexpr std::uint32_t kIbexRom[] = {0x9b9da2b7, 0xb8d28293, 0x3a129073, 0x10500073, 0xffdff06f};
constexpr std::uint32_t kPark = 0x0000006f;                      // j .
constexpr std::uint32_t kMtip = 1u << 7, kMeip = 1u << 11, kFast0 = 1u << 16;

fu3_soc_options debug_options()
{
    return fu3_soc_options{};
}

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    fu3_dbg::Jtag2Axi& jtag;
    unsigned checks = 0, failures = 0;
    bool finished = false;   // run() reached its end (not a starved or stopped simulation)

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", debug_options()), jtag(*top.jtag())
    {
        auto poke = [&](const char* mem, std::uint64_t offset, std::uint32_t word) {
            for (unsigned i = 0; i < 4; ++i)
                top.memory(mem)->poke(offset + i, (word >> (8 * i)) & 0xff);
        };
        for (unsigned i = 0; i < 5; ++i)
            poke("ROM", 0x80 + 4 * i, kIbexRom[i]);
        poke("CVA6_ISRAM", 0, kCountLoop[0]);
        poke("CVA6_ISRAM", 4, kCountLoop[1]);
        // CVA6 debug code (HaltAddress 0x0004_0000 and exception entry +8): park.
        poke("CVA6_ISRAM", 0x30000, kPark);
        poke("CVA6_ISRAM", 0x30008, kPark);
        // Ibex debug code (HaltAddress 0x0006_0000, last 4 KiB of IBEX_ISRAM): park.
        poke("IBEX_ISRAM", 0x10000, kPark);
        poke("IBEX_ISRAM", 0x10008, kPark);
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << what << '\n';
        }
    }

    // JTAG transactions (IR then DR scans) as a debugger issues them.
    std::uint32_t status()
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_STATUS);
        return static_cast<std::uint32_t>(jtag.scan_dr(0));
    }
    void clear_status()
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_STATUS);
        jtag.scan_dr(FU3_DBG_STATUS_ERRORS);
    }
    void bus_write(std::uint32_t address, std::uint32_t value)
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(address);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_WRITE | value);
    }
    std::uint32_t bus_read(std::uint32_t address)
    {
        jtag.scan_ir(FU3_DBG_IR_CMD_ADDR);
        jtag.scan_dr(address);
        jtag.scan_ir(FU3_DBG_IR_CMD_DATA);
        jtag.scan_dr(FU3_DBG_DATA_READ);
        return static_cast<std::uint32_t>(jtag.scan_dr(0));   // capture of the read
    }
    void dm_write(unsigned reg, std::uint32_t value) { bus_write(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(reg), value); }
    std::uint32_t dm_read(unsigned reg) { return bus_read(FU3_DBG_DM_BASE + FU3_DBG_DM_OFFSET(reg)); }
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(200, sc_core::SC_NS);
        return cond();
    }
    void select(unsigned hart) { dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart)); }
    std::uint32_t dmstatus() { return dm_read(FU3_DBG_DMSTATUS); }
    bool halted() { return (dmstatus() >> 9) & 1; }    // allhalted
    void halt(unsigned hart)
    {
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)),
              "DM: hart " + std::to_string(hart) + " halted");
        select(hart);
    }
    void resume(unsigned hart)
    {
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart) | FU3_DBG_DMCONTROL_RESUMEREQ);
        check(wait_for([this] { return running(); }, sc_core::sc_time(20, sc_core::SC_US)),
              "DM: hart " + std::to_string(hart) + " resumed");
    }
    std::uint32_t mip(unsigned hart)
    {
        select(hart);
        std::uint32_t value = 0;
        check(access_register(0x344, false, value), "DM: abstract read of mip, hart " + std::to_string(hart));
        return value;
    }
    bool running() { return (dmstatus() >> 11) & 1; }  // allrunning
    // Abstract command: access register (32-bit), transfer, optional write.
    bool access_register(std::uint16_t regno, bool write, std::uint32_t& value)
    {
        if (write)
            dm_write(FU3_DBG_DATA0, value);
        dm_write(FU3_DBG_COMMAND, (2u << 20) | (1u << 17) | (write ? 1u << 16 : 0) | regno);
        const auto abstractcs = dm_read(FU3_DBG_ABSTRACTCS);
        if ((abstractcs >> 8) & 7) {
            dm_write(FU3_DBG_ABSTRACTCS, 7u << 8);   // clear cmderr
            return false;
        }
        if (!write)
            value = dm_read(FU3_DBG_DATA0);
        return true;
    }

    void run()
    {
        check(wait_for([this] { return top.system_ready(); }, sc_core::sc_time(50, sc_core::SC_US)), "system out of reset");
        wait(2, sc_core::SC_US);

        jtag.test_logic_reset();
        check(jtag.scan_dr(0) == FU3_DBG_IDCODE, "JTAG: IDCODE after Test-Logic-Reset");

        // JTAG -> bus.
        bus_write(0x01000400, 0xa5a55a5a);
        check(bus_read(0x01000400) == 0xa5a55a5a && !(status() & FU3_DBG_STATUS_ERRORS), "JTAG -> bus: DSRAM write/read");
        bus_read(0x08000000);
        check(status() & FU3_DBG_STATUS_DECERR, "JTAG -> bus: unmapped address reports DECERR");
        clear_status();
        // FU3_VP_ABI_1 access profile: DBG reads the ROM, never writes it.
        check(bus_read(0x80) == kIbexRom[0] && !(status() & FU3_DBG_STATUS_ERRORS), "JTAG -> bus: ROM readable by DBG");
        bus_write(0x80, 0);
        check((status() & FU3_DBG_STATUS_SLVERR) && bus_read(0x80) == kIbexRom[0], "JTAG -> bus: ROM write by DBG refused");
        clear_status();
        check(!(status() & FU3_DBG_STATUS_ERRORS), "JTAG: status errors W1C");

        // JTAG releases CVA6 (CRM FCLK_SW), then debugs it through the DM.
        // Q04-c: a hart held in reset is unavailable; a halt request set then
        // halts it on its first instruction once released.
        select(0);
        check((dmstatus() & FU3_DBG_DMSTATUS_ALLUNAVAIL) && !running() && !halted(),
              "DM: CVA6 held in reset reported unavailable");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_HALTREQ);
        bus_write(FU3_CRM_BASE + FU3_CRM_FCLK_SW_OFFSET, 1);
        check(wait_for([this] { return !top.cva6()->in_reset(); }, sc_core::sc_time(20, sc_core::SC_US)),
              "JTAG -> CRM: CVA6 released");
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)),
              "DM: halt request set while unavailable halts CVA6 once released");
        select(0);
        std::uint32_t reset_dpc = 0;
        check(access_register(0x7b1, false, reset_dpc) && reset_dpc == cdc::cpu::fu3::kCva6ResetPc,
              "DM: halted before its first instruction (dpc = reset PC)");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_RESUMEREQ);
        wait(5, sc_core::SC_US);
        select(0);
        check(running(), "DM: hart 0 (CVA6) running");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM: haltreq halts CVA6");
        wait(1, sc_core::SC_US);
        check(top.cva6()->get_pc() == cdc::cpu::fu3::kCva6HaltAddress,
              "DM: halted CVA6 executes its debug code (park loop at HaltAddress, no fetch fault)");
        select(0);
        std::uint32_t t0 = 0, dpc = 0;
        check(access_register(0x1005, false, t0) && t0 > 0, "DM: abstract read of t0 (counted before the halt)");
        check(access_register(0x7b1, false, dpc) && (dpc == cdc::cpu::fu3::kCva6ResetPc || dpc == cdc::cpu::fu3::kCva6ResetPc + 4),
              "DM: dpc in the counting loop");
        std::uint32_t value = 0x1000;
        check(access_register(0x1005, true, value), "DM: abstract write of t0");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_RESUMEREQ);
        check(wait_for([this] { return running(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM: resumereq resumes CVA6");
        wait(5, sc_core::SC_US);
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM: halted again");
        select(0);
        check(access_register(0x1005, false, t0) && t0 > 0x1000, "DM: CVA6 counted on from the written t0");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(0) | FU3_DBG_DMCONTROL_RESUMEREQ);

        // Ibex on hartsel 1.
        select(1);
        check(running(), "DM: hart 1 (Ibex) running");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM: haltreq halts Ibex");
        wait(1, sc_core::SC_US);
        check(top.ibex()->get_pc() == cdc::cpu::fu3::kIbexDmHaltAddr,
              "DM: halted Ibex executes its debug code (park loop at 0x0006_0000) once the ROM applied the final PMP");
        select(1);
        std::uint32_t misa = 0;
        check(access_register(0x301, false, misa) && (misa >> 30) == 1, "DM: Ibex misa (RV32) by abstract command");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | FU3_DBG_DMCONTROL_RESUMEREQ);
        check(wait_for([this] { return running(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM: Ibex resumed");

        // Interrupts at the CPU inputs (both harts halted, so neither takes them).
        halt(0);
        halt(1);
        check(!(mip(0) & (kMtip | kMeip)) && !(mip(1) & (kMtip | kMeip | kFast0)), "IRQ: no input pending at first");
        // INTMAP routes through the FU3_VP_ABI_1 enable CSR, as firmware does.
        bus_write(FU3_VP_ABI_SYSCSR_BASE + FU3_VP_INTMAP_EN0_OFFSET, ~0u);
        bus_write(FU3_VP_ABI_SYSCSR_BASE + FU3_VP_INTMAP_EN1_OFFSET, FU3_VP_INTMAP_EN1_MASK);
        check(bus_read(FU3_VP_ABI_SYSCSR_BASE + FU3_VP_INTMAP_EN1_OFFSET) == FU3_VP_INTMAP_EN1_MASK,
              "ABI: INTMAP_EN1 reads back its two sources");
        bus_write(FU3_PLIC_BASE + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1), 1);
        bus_write(FU3_PLIC_BASE + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1));
        for (const std::uint32_t base : {FU3_TIMER0_BASE, FU3_TIMER1_BASE, FU3_TIMER2_BASE}) {
            bus_write(base + FU3_TIMER_COMPARE_OFFSET, 8);
            bus_write(base + FU3_TIMER_CONTROL_OFFSET, FU3_TIMER_ENABLE);
        }
        wait(2, sc_core::SC_US);
        const auto cva6_mip = mip(0), ibex_mip = mip(1);
        check((cva6_mip & kMtip) && (cva6_mip & kMeip),
              "IRQ: CVA6 mip.MTIP (TIMER0 time_irq_i) and mip.MEIP (TIMER1 -> PLIC ID6 -> irq_i[0])");
        check((ibex_mip & kMtip) && (ibex_mip & kMeip) && !(ibex_mip & kFast0),
              "IRQ: Ibex mip.MTIP (TIMER1 -> irq_timer_i), mip.MEIP (TIMER2 -> irq_external_i), no fast[0]");
        for (const std::uint32_t base : {FU3_TIMER0_BASE, FU3_TIMER1_BASE, FU3_TIMER2_BASE}) {
            bus_write(base + FU3_TIMER_CONTROL_OFFSET, 0);
            bus_write(base + FU3_TIMER_COMPARE_OFFSET, 8);   // acknowledges the latched match (Q03-d)
        }
        check(bus_read(FU3_PLIC_BASE + FU3_PLIC_CLAIM_OFFSET) == FU3_PLIC_ID_TIMER1, "IRQ: PLIC claim returns TIMER1");
        wait(1, sc_core::SC_US);
        check(!(mip(0) & (kMtip | kMeip)) && !(mip(1) & (kMtip | kMeip)), "IRQ: inputs drop after acknowledge and claim");
        bus_write(FU3_PLIC_BASE + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_TIMER1);   // complete
        bus_write(FU3_PLIC_BASE + FU3_PLIC_ENABLE_OFFSET, 0);
        resume(0);
        resume(1);

        // ndmreset: system reset, DBG kept (HAS 6.5.3), havereset reported.
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_ACKHAVERESET);
        check(!(dmstatus() & FU3_DBG_DMSTATUS_ANYHAVERESET), "DM: havereset acknowledged");
        dm_write(FU3_DBG_DATA0, 0x5a5a1234);
        const auto resets = top.reset_history().size();
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_NDMRESET);
        wait(500, sc_core::SC_NS);
        check(top.cva6()->in_reset() && top.ibex()->in_reset(), "ndmreset: both CPUs held");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_NDMRESET | FU3_DBG_DMCONTROL_HARTSEL(1));
        const bool ibex_unavail = dmstatus() & FU3_DBG_DMSTATUS_ALLUNAVAIL;
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_NDMRESET | FU3_DBG_DMCONTROL_HARTSEL(0));
        check(ibex_unavail && (dmstatus() & FU3_DBG_DMSTATUS_ALLUNAVAIL), "ndmreset: both harts reported unavailable");
        check(dm_read(FU3_DBG_DMCONTROL) & FU3_DBG_DMCONTROL_DMACTIVE, "ndmreset: DBG keeps running (DM reachable)");
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE);
        check(wait_for([this] { return top.system_ready() && !top.ibex()->in_reset(); }, sc_core::sc_time(50, sc_core::SC_US)),
              "ndmreset released: system and Ibex back");
        check(top.reset_history().size() == resets + 1 && top.reset_history().back().causes == reset_debug,
              "reset cause: debug (ndmreset)");
        check(dmstatus() & FU3_DBG_DMSTATUS_ANYHAVERESET, "DM: havereset after the reset");
        check(dm_read(FU3_DBG_DATA0) == 0x5a5a1234, "ndmreset: DM state (DATA0) kept");

        // Watchdog reset: the DBG domain resets too (Q05-e), unlike ndmreset.
        halt(1);
        dm_write(FU3_DBG_DATA0, 0xdead1234);
        bus_read(0x08000000);   // leaves DECERR in the bridge status
        check((status() & FU3_DBG_STATUS_DECERR) && dm_read(FU3_DBG_DATA0) == 0xdead1234, "before WDT reset: DM/bridge state set");
        const auto before = top.reset_history().size();
        top.drive_watchdog_request(true);
        check(wait_for([&] { return top.reset_history().size() > before; }, sc_core::sc_time(5, sc_core::SC_US)),
              "watchdog request resets the system");
        top.drive_watchdog_request(false);
        check(wait_for([this] { return top.system_ready() && !top.ibex()->in_reset(); }, sc_core::sc_time(50, sc_core::SC_US)),
              "watchdog reset released");
        check(top.reset_history().back().causes == reset_watchdog, "reset cause: watchdog");
        check(!(status() & FU3_DBG_STATUS_ERRORS), "WDT reset: JTAG2AXI status cleared");
        check(dm_read(FU3_DBG_DMCONTROL) == 0 && dm_read(FU3_DBG_DATA0) == 0, "WDT reset: DMCONTROL and DATA0 reset");
        select(1);
        wait(2, sc_core::SC_US);
        check(running() && (dmstatus() & FU3_DBG_DMSTATUS_ANYHAVERESET), "WDT reset: halt request released, Ibex runs, havereset");

        std::cout << "fu3_soc debug: " << checks << " checks, " << failures << " failures -> "
                  << (failures ? "FAIL" : "PASS") << '\n';
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("fu3_soc/gated", sc_core::SC_DO_NOTHING);
    bench b("bench");
    sc_core::sc_start(50, sc_core::SC_MS);
    if (!b.finished)
        std::cerr << "FAIL: the bench did not run to its end (simulation starved or timed out at "
                  << sc_core::sc_time_stamp() << ")\n";
    return b.failures || !b.checks || !b.finished ? 1 : 0;
}
