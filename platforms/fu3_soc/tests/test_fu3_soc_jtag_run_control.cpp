// Plan T22 (R1 matrix, Debug row): run control of both harts through the DBG
// path a debugger without GDB uses: JTAG2AXI -> bus -> Debug Module, abstract
// register access only (no program buffer; Q04-a/b).
// For CVA6 (released by JTAG through CRM FCLK_SW) and Ibex, each running a
// counting loop (addi t0, t0, 1; j .-4):
//   - breakpoint: an execute trigger (tselect/tdata1/tdata2 written by
//     abstract CSR access) on the addi, resume -> halted there, dcsr.cause
//     trigger (2), stopped the first time it reaches it (t0 unchanged);
//   - single step: dcsr.step, resume -> halted after one instruction,
//     dcsr.cause step (4), dpc on the jump, t0 + 1;
//   - trigger cleared and dcsr.step off: the hart runs freely again.

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include <systemc>

#include <fu3_crm/registers.h>
#include <fu3_dbg/registers.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

// Ibex ROM at 0x80: final IBEX_ISRAM PMP entry (RX, makes the debug partition
// executable, as in test_fu3_soc_debug), then the counting loop at 0x8c.
constexpr std::uint32_t kIbexRom[] = {0x9b9da2b7, 0xb8d28293, 0x3a129073, 0x00128293, 0xffdff06f};
constexpr std::uint32_t kIbexLoop = 0x8c;
// CVA6 counting loop at its reset vector 0x10000.
constexpr std::uint32_t kCountLoop[] = {0x00128293, 0xffdff06f};
constexpr std::uint32_t kCva6Loop = 0x10000;
constexpr std::uint32_t kPark = 0x0000006f;   // j . (debug code)

constexpr std::uint16_t kTselect = 0x7a0, kTdata1 = 0x7a1, kTdata2 = 0x7a2, kDcsr = 0x7b0, kDpc = 0x7b1, kT0 = 0x1005;
constexpr std::uint32_t kMcontrolExecuteM = (1u << 2) | (1u << 6), kDcsrStep = 1u << 2;

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    fu3_dbg::Jtag2Axi& jtag;
    unsigned checks = 0, failures = 0;
    bool finished = false;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", fu3_soc_options{}), jtag(*top.jtag())
    {
        auto poke = [&](const char* mem, std::uint64_t offset, std::uint32_t word) {
            for (unsigned i = 0; i < 4; ++i)
                top.memory(mem)->poke(offset + i, (word >> (8 * i)) & 0xff);
        };
        for (unsigned i = 0; i < 5; ++i)
            poke("ROM", 0x80 + 4 * i, kIbexRom[i]);
        poke("CVA6_ISRAM", 0, kCountLoop[0]);
        poke("CVA6_ISRAM", 4, kCountLoop[1]);
        poke("CVA6_ISRAM", 0x30000, kPark);
        poke("CVA6_ISRAM", 0x30008, kPark);
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
        return static_cast<std::uint32_t>(jtag.scan_dr(0));
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
    void control(unsigned hart, std::uint32_t bits)
    {
        dm_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(hart) | bits);
    }
    bool halted() { return dm_read(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLHALTED; }
    bool running() { return dm_read(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ALLRUNNING; }
    std::uint32_t reg(std::uint16_t regno)
    {
        dm_write(FU3_DBG_COMMAND, (2u << 20) | (1u << 17) | regno);
        check(((dm_read(FU3_DBG_ABSTRACTCS) >> 8) & 7) == 0, "abstract read of " + std::to_string(regno));
        return dm_read(FU3_DBG_DATA0);
    }
    void set_reg(std::uint16_t regno, std::uint32_t value)
    {
        dm_write(FU3_DBG_DATA0, value);
        dm_write(FU3_DBG_COMMAND, (2u << 20) | (1u << 17) | (1u << 16) | regno);
        check(((dm_read(FU3_DBG_ABSTRACTCS) >> 8) & 7) == 0, "abstract write of " + std::to_string(regno));
    }
    // Resume and wait for the next halt (breakpoint or step).
    bool resume_until_halt(unsigned hart)
    {
        control(hart, FU3_DBG_DMCONTROL_RESUMEREQ);
        control(hart, 0);
        return wait_for([this] { return halted(); }, sc_core::sc_time(50, sc_core::SC_US));
    }

    void hart_run_control(unsigned hart, std::uint32_t loop)
    {
        const std::string h = hart ? "Ibex: " : "CVA6: ";
        control(hart, FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)), h + "halted by the DM");
        control(hart, 0);

        // Breakpoint: execute trigger 0 on the addi.
        set_reg(kTselect, 0);
        set_reg(kTdata2, loop);
        set_reg(kTdata1, kMcontrolExecuteM);
        check(reg(kTdata2) == loop && (reg(kTdata1) & kMcontrolExecuteM) == kMcontrolExecuteM, h + "trigger 0 programmed");
        const auto t0_before = reg(kT0);
        check(resume_until_halt(hart), h + "resumed, halted again");
        check(reg(kDpc) == loop && ((reg(kDcsr) >> 6) & 7) == 2, h + "halted by the trigger on the addi (dcsr.cause 2)");
        const auto t0_at_break = reg(kT0);
        // An execute trigger fires before its instruction: the hart stops the
        // first time it reaches the addi, which has not run (t0 unchanged).
        check(t0_at_break == t0_before, h + "stopped before the addi executed (t0 unchanged)");

        // Single step from the breakpoint (trigger off first, as a debugger
        // steps off a breakpoint).
        set_reg(kTdata1, 0);
        set_reg(kDcsr, reg(kDcsr) | kDcsrStep);
        check(resume_until_halt(hart), h + "step: halted again");
        check(reg(kDpc) == loop + 4 && ((reg(kDcsr) >> 6) & 7) == 4 && reg(kT0) == t0_at_break + 1,
              h + "one instruction stepped (dpc on the jump, dcsr.cause 4, t0 + 1)");
        check(resume_until_halt(hart) && reg(kDpc) == loop && reg(kT0) == t0_at_break + 1,
              h + "second step: the jump, back on the addi");

        // Free running again.
        set_reg(kDcsr, reg(kDcsr) & ~kDcsrStep);
        control(hart, FU3_DBG_DMCONTROL_RESUMEREQ);
        control(hart, 0);
        check(wait_for([this] { return running(); }, sc_core::sc_time(20, sc_core::SC_US)), h + "runs again");
        wait(20, sc_core::SC_US);
        check(running(), h + "no trigger or step left: still running");
        control(hart, FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([this] { return halted(); }, sc_core::sc_time(20, sc_core::SC_US)), h + "halted to check progress");
        control(hart, 0);
        check(reg(kT0) > t0_at_break + 10, h + "loop progressed freely");
        control(hart, FU3_DBG_DMCONTROL_RESUMEREQ);
        control(hart, 0);
    }

    void run()
    {
        check(wait_for([this] { return top.system_ready(); }, sc_core::sc_time(50, sc_core::SC_US)), "system out of reset");
        wait(20, sc_core::SC_US);   // Ibex through its ROM stub into the loop
        jtag.test_logic_reset();
        bus_write(FU3_CRM_BASE + FU3_CRM_FCLK_SW_OFFSET, 1);   // JTAG releases CVA6
        wait(20, sc_core::SC_US);
        hart_run_control(0, kCva6Loop);
        hart_run_control(1, kIbexLoop);
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    bench b("bench");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_SEC));
    b.check(b.finished, "bench reached its end");
    std::cout << "fu3_soc jtag run control: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.failures ? "FAIL" : "PASS") << '\n';
    return b.failures ? 1 : 0;
}
