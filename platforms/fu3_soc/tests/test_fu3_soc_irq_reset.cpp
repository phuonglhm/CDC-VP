// Plan T21: no spurious interrupt across a system reset, a clock gate or a
// peripheral reset. The bench takes the IBEX port (CPUs off) and watches the
// CPU interrupt nets (CVA6 irq_i[0]/time_irq_i/ipi_i, every INTMAP output).
//   - system reset with every kind of source asserted (TIMER0-3 latches,
//     UART0 THRE, SWIRQ both CPUs, CRM.int, PLIC enabled, INTMAP routes on):
//     every CPU net falls with the reset and none rises again afterwards;
//     PLIC enables/priorities/claim, INTMAP enables, SWIRQ and the latches are
//     at their reset values; a source still asserted (CRM.int: the CRM keeps
//     its registers over a watchdog reset) is pending at the PLIC only;
//   - clock gate of a source whose interrupt is asserted: the level holds,
//     no edge while gated; a timer gated before its match raises nothing
//     until its clock returns;
//   - peripheral reset (SW_RST_N) of an asserted source: its line falls; the
//     PLIC keeps the pending bit it captured (RISC-V PLIC gateway: claim
//     returns the ID once) and does not capture it again.

#include <cstdint>
#include <functional>
#include <iostream>
#include <regex>
#include <string>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <fu3_bus/access.h>
#include <fu3_crm/registers.h>
#include <fu3_plic/registers.h>
#include <fu3_timer/registers.h>
#include <fu3_uart/registers.h>
#include <fu3_vp_abi.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

constexpr std::uint64_t kCrm = FU3_CRM_BASE, kPlic = FU3_PLIC_BASE, kAbi = FU3_VP_ABI_SYSCSR_BASE;
constexpr std::uint64_t kTimers[4] = {FU3_TIMER0_BASE, FU3_TIMER1_BASE, FU3_TIMER2_BASE, FU3_TIMER3_BASE};

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    fu3_peripherals& p;
    tlm_utils::simple_initiator_socket<bench> master{"master"};
    unsigned checks = 0, failures = 0;
    bool finished = false;
    bool armed = false;      // count CPU interrupt edges
    unsigned rises = 0;
    std::string first_rise;

    static fu3_soc_options options()
    {
        fu3_soc_options o;
        o.cpus = false;
        return o;
    }

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", options()), p(top.peripherals())
    {
        master.bind(top.fabric().initiator("IBEX"));
        SC_THREAD(run);
        SC_METHOD(edges);
        sensitive << p.cva6_external.posedge_event() << p.cva6_timer.posedge_event()
                  << p.cva6_software.posedge_event();
        for (auto& s : p.ibex_irq)
            sensitive << s.posedge_event();
        dont_initialize();
    }

    void edges()
    {
        if (!armed)
            return;
        ++rises;
        if (first_rise.empty())
            first_rise = nets_high() + " @" + sc_core::sc_time_stamp().to_string();
    }
    std::string nets_high() const
    {
        std::string out;
        if (p.cva6_external.read())
            out += " cva6_external";
        if (p.cva6_timer.read())
            out += " cva6_timer";
        if (p.cva6_software.read())
            out += " cva6_software";
        for (unsigned o = 0; o < p.ibex_irq.size(); ++o)
            if (p.ibex_irq[o].read())
                out += " ibex_irq[" + std::to_string(o) + "]";
        return out;
    }
    bool any_cpu_irq() const { return !nets_high().empty(); }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << what << '\n';
        }
    }
    tlm::tlm_response_status access(bool write, std::uint64_t address, std::uint32_t& value, unsigned width = 4)
    {
        unsigned char data[4] = {};
        for (unsigned i = 0; i < width; ++i)
            data[i] = (value >> (8 * i)) & 0xff;
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(width);
        tx.set_streaming_width(width);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess scoped(tx, width);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        master->b_transport(tx, delay);
        wait(delay);
        value = data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
        return tx.get_response_status();
    }
    void write(std::uint64_t a, std::uint32_t v, unsigned width = 4) { access(true, a, v, width); }
    std::uint32_t read(std::uint64_t a, unsigned width = 4)
    {
        std::uint32_t v = 0;
        access(false, a, v, width);
        return v;
    }
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(10, sc_core::SC_NS);
        return cond();
    }
    // CRM CLK_GATE / SW_RST_N register and bit of a target ("CLK_GATE_1[19] SW_RST_N_1[19]").
    struct crm_bit {
        unsigned reg = 0, bit = 0;
    };
    crm_bit gate_of(const std::string& target, bool reset_bit)
    {
        for (const auto& i : top.instances()) {
            std::smatch m;
            if (i.name == target &&
                std::regex_search(i.domain, m, std::regex(reset_bit ? "SW_RST_N_(\\d)\\[(\\d+)\\]" : "CLK_GATE_(\\d)\\[(\\d+)\\]")))
                return {unsigned(std::stoul(m[1])), unsigned(std::stoul(m[2]))};
        }
        throw std::runtime_error("no CRM bit for " + target);
    }
    void set_crm(const std::string& target, bool reset_bit, bool on)
    {
        const auto b = gate_of(target, reset_bit);
        const std::uint64_t a = kCrm + (reset_bit ? (b.reg ? FU3_CRM_SW_RST_N_1_OFFSET : FU3_CRM_SW_RST_N_0_OFFSET)
                                                  : (b.reg ? FU3_CRM_CLK_GATE_1_OFFSET : FU3_CRM_CLK_GATE_0_OFFSET));
        const auto v = read(a);
        write(a, on ? v | (1u << b.bit) : v & ~(1u << b.bit));
    }
    void plic_enable_all()
    {
        for (unsigned id = 1; id <= FU3_PLIC_SOURCES; ++id)
            write(kPlic + FU3_PLIC_PRIORITY_OFFSET(id), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_ENABLE_MASK);
    }
    void timer_match(unsigned i)
    {
        write(kTimers[i] + FU3_TIMER_COMPARE_OFFSET, read(kTimers[i] + FU3_TIMER_COUNT_OFFSET) + 8);
        write(kTimers[i] + FU3_TIMER_CONTROL_OFFSET, FU3_TIMER_ENABLE);
    }

    void run()
    {
        const auto bus = [&] { return top.bus_clock_period(); };
        check(wait_for([&] { return top.system_ready(); }, sc_core::sc_time(100, sc_core::SC_US)), "power-on reset ends");

        // 1. Every kind of source asserted, every route enabled.
        for (unsigned i = 0; i < 4; ++i)
            timer_match(i);
        write(FU3_UART0_BASE + FU3_UART_IER, FU3_UART_IER_TX, 1);   // THR empty -> THRE interrupt
        write(kAbi + FU3_VP_SWIRQ_SET_OFFSET, FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX);
        write(kCrm + FU3_CRM_CRM_OFFSET, 1);                          // CRM.int
        write(kAbi + FU3_VP_INTMAP_EN0_OFFSET, ~0u);
        write(kAbi + FU3_VP_INTMAP_EN1_OFFSET, ~0u);
        plic_enable_all();
        wait(bus() * 40);
        check(p.cva6_external.read() && p.cva6_timer.read() && p.cva6_software.read(),
              "before the reset: CVA6 external, timer and software interrupts asserted");
        check(p.ibex_irq[fu3_plic::kOutTimer].read() && p.ibex_irq[fu3_plic::kOutSoftware].read(),
              "before the reset: Ibex timer and software interrupts asserted");

        // 2. System reset: every CPU net falls, none rises again.
        armed = true;
        rises = 0;
        top.drive_watchdog_request(true);
        wait(1, sc_core::SC_US);
        top.drive_watchdog_request(false);
        check(!any_cpu_irq(), "during the reset: no CPU interrupt net high (" + nets_high() + ")");
        check(wait_for([&] { return top.system_ready(); }, sc_core::sc_time(50, sc_core::SC_US)), "system ready again");
        wait(50, sc_core::SC_US);
        check(rises == 0, "no CPU interrupt edge after the reset (" + std::to_string(rises) + ", first:" + first_rise + ")");
        check(!any_cpu_irq(), "after the reset: no CPU interrupt net high (" + nets_high() + ")");
        check(read(kPlic + FU3_PLIC_ENABLE_OFFSET) == 0 && read(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1)) == 0 &&
                  read(kPlic + FU3_PLIC_CLAIM_OFFSET) == 0,
              "PLIC enables, priorities at reset; claim returns 0");
        check(read(kAbi + FU3_VP_INTMAP_EN0_OFFSET) == 0 && read(kAbi + FU3_VP_INTMAP_EN1_OFFSET) == 0 &&
                  read(kAbi + FU3_VP_SWIRQ_PENDING_OFFSET) == 0,
              "INTMAP enables and SWIRQ at reset");
        bool latches = true;
        for (unsigned i = 1; i < 4; ++i)
            latches = latches && !p.plic_source[(i == 1 ? FU3_PLIC_ID_TIMER1 : i == 2 ? FU3_PLIC_ID_TIMER2 : FU3_PLIC_ID_TIMER3) - 1].read();
        check(latches && !p.cva6_timer.read() && !p.plic_source[FU3_PLIC_ID_UART0 - 1].read(),
              "timer latches and UART0 interrupt cleared by the reset");
        const bool crm_kept = read(kCrm + FU3_CRM_CRM_OFFSET) & 1;
        check(!crm_kept || (p.plic().pending() & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_CRM)),
              "a source still asserted after the reset (CRM.int) is pending at the PLIC, enabled nowhere");
        std::cout << "CRM.int " << (crm_kept ? "kept" : "cleared") << " by the watchdog reset\n";
        write(kCrm + FU3_CRM_CRM_OFFSET, 0);
        armed = false;

        // 3. Clock gate of an asserted source: level held, no edge.
        write(kAbi + FU3_VP_INTMAP_EN0_OFFSET, ~0u);
        write(kAbi + FU3_VP_INTMAP_EN1_OFFSET, ~0u);
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1));
        timer_match(1);
        check(wait_for([&] { return p.cva6_external.read() && p.ibex_irq[fu3_plic::kOutTimer].read(); },
                       sc_core::sc_time(5, sc_core::SC_US)),
              "TIMER1 interrupt at CVA6 (PLIC) and Ibex (INTMAP): source " +
                  std::to_string(p.plic_source[FU3_PLIC_ID_TIMER1 - 1].read()) + " cva6 " +
                  std::to_string(p.cva6_external.read()) + " ibex" + nets_high() + " count " +
                  std::to_string(read(FU3_TIMER1_BASE + FU3_TIMER_COUNT_OFFSET)) + " cmp " +
                  std::to_string(read(FU3_TIMER1_BASE + FU3_TIMER_COMPARE_OFFSET)) + " ctrl " +
                  std::to_string(read(FU3_TIMER1_BASE + FU3_TIMER_CONTROL_OFFSET)) + " gate1 " +
                  std::to_string(read(kCrm + FU3_CRM_CLK_GATE_1_OFFSET)));
        armed = true;
        rises = 0;
        first_rise.clear();
        unsigned falls = 0;
        set_crm("TIMER1", false, false);
        for (unsigned n = 0; n < 1000; ++n) {
            wait(10, sc_core::SC_NS);
            falls += !p.plic_source[FU3_PLIC_ID_TIMER1 - 1].read();
        }
        check(falls == 0 && rises == 0 && p.cva6_external.read(),
              "TIMER1 gated with its interrupt asserted: level held, no edge (" + std::to_string(rises) + ")");
        set_crm("TIMER1", false, true);
        write(FU3_TIMER1_BASE + FU3_TIMER_CONTROL_OFFSET, 0);
        write(FU3_TIMER1_BASE + FU3_TIMER_COMPARE_OFFSET, 8);   // acknowledge (Q03-d)
        check(read(kPlic + FU3_PLIC_CLAIM_OFFSET) == FU3_PLIC_ID_TIMER1, "TIMER1 claimed once");
        write(kPlic + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_TIMER1);
        wait(bus() * 4);
        check(!p.cva6_external.read() && !p.ibex_irq[fu3_plic::kOutTimer].read(),
              "acknowledged and completed: no interrupt left");
        armed = false;

        // A timer gated before its match raises nothing until its clock returns.
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER2), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER2));
        write(FU3_TIMER2_BASE + FU3_TIMER_COMPARE_OFFSET, read(FU3_TIMER2_BASE + FU3_TIMER_COUNT_OFFSET) + 50);
        write(FU3_TIMER2_BASE + FU3_TIMER_CONTROL_OFFSET, FU3_TIMER_ENABLE);
        set_crm("TIMER2", false, false);
        wait(bus() * 500);
        check(!p.plic_source[FU3_PLIC_ID_TIMER2 - 1].read() && !p.cva6_external.read(),
              "TIMER2 gated before its match: no interrupt while gated");
        set_crm("TIMER2", false, true);
        check(wait_for([&] { return p.cva6_external.read(); }, bus() * 200), "TIMER2 match after its clock returns");
        write(FU3_TIMER2_BASE + FU3_TIMER_CONTROL_OFFSET, 0);
        write(FU3_TIMER2_BASE + FU3_TIMER_COMPARE_OFFSET, 8);
        read(kPlic + FU3_PLIC_CLAIM_OFFSET);
        write(kPlic + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_TIMER2);

        // 4. Peripheral reset of an asserted source: line falls; the PLIC keeps
        //    the pending bit it captured once.
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_UART0), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, 0);   // captured, not delivered
        write(FU3_UART0_BASE + FU3_UART_IER, FU3_UART_IER_TX, 1);
        check(wait_for([&] { return p.plic().pending() & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0); }, bus() * 40),
              "UART0 THRE interrupt pending at the PLIC");
        set_crm("UART0", true, false);
        wait(bus() * 4);
        set_crm("UART0", true, true);
        wait(bus() * 10);
        check(!p.plic_source[FU3_PLIC_ID_UART0 - 1].read() && read(FU3_UART0_BASE + FU3_UART_IER, 1) == 0,
              "UART0 reset: its interrupt line low, IER at reset");
        check(p.plic().pending() & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0),
              "the PLIC keeps the pending bit captured before the source reset (gateway)");
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0));
        check(read(kPlic + FU3_PLIC_CLAIM_OFFSET) == FU3_PLIC_ID_UART0, "claimed once");
        write(kPlic + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_UART0);
        wait(bus() * 4);
        check(!(p.plic().pending() & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_UART0)) && !p.cva6_external.read(),
              "completed with the source low: not pending again");
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
    std::cout << "fu3_soc irq reset: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.failures ? "FAIL" : "PASS") << '\n';
    return b.failures ? 1 : 0;
}
