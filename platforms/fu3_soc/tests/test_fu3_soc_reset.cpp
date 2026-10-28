// Plan T21: a system reset (watchdog request to the CRM) arriving while a bus
// access is on its way, for every bus target behind a gate, at several points
// of the access (before it reaches the gate, inside the model). The bench takes
// the IBEX port (CPUs off) and, for a second access queued at the same target
// at the same time, the CVA6 port. Reads and writes (the probe register's reset
// value, 0 in memories). QSPI XIP and HyperRAM are set up (memory-mapped mode;
// PLL and controller initialisation) before each access, so that their
// accesses do run through the model. IBEX_DBG (the DM) is closed to both ports
// by the access profile: its accesses end at the bus (JTAG case below).
//   - the access completes (no hang), the bus drains and the system is ready
//     again;
//   - drain before release: an access the reset caught completes before the
//     target's domain leaves reset (nothing lands in the block after it);
//   - no warning: an access the reset caught is "aborted by reset" (counted),
//     not a software error;
//   - afterwards the target answers again, a register probe with its reset value.
// A clock-gate refusal outside a reset is still a warning (software error); a
// gated target caught by a system reset counts as aborted by the reset (the
// domain reset comes first). A release that comes while the target is gated
// waits for its clock and then two bus periods (CRM Spec V1.1 3.1.2).
// Review T21-R01: a JTAG read of XIP on its way when a watchdog reset (DBG
// domain included) comes leaves no result in the reset bridge (STATUS, ADDR);
// with ndmreset (DBG kept) the bridge reports the error of its access.

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <fu3_bus/access.h>
#include <fu3_crm/registers.h>
#include <fu3_dbg/registers.h>
#include <fu3/qspi/registers.h>
#include <fu3_hyperram/registers.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    tlm_utils::simple_initiator_socket<bench> master{"master"}, second{"second"};
    unsigned checks = 0, failures = 0;
    bool finished = false;
    sc_core::sc_event fire;
    sc_core::sc_time reset_after;

    static fu3_soc_options options()
    {
        fu3_soc_options o;
        o.cpus = false;
        return o;
    }

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", options())
    {
        master.bind(top.fabric().initiator("IBEX"));
        second.bind(top.fabric().initiator("CVA6"));
        SC_THREAD(run);
        SC_THREAD(shadow);
        SC_THREAD(watchdog);
    }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << what << '\n';
        }
    }

    tlm::tlm_response_status access(bool write, std::uint64_t address, std::uint32_t& value, unsigned width = 4,
                                    bool on_second = false)
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
        (on_second ? second : master)->b_transport(tx, delay);
        wait(delay);
        value = data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
        return tx.get_response_status();
    }
    void write(std::uint64_t address, std::uint32_t value) { access(true, address, value); }

    // The same access through the CVA6 port, started with the IBEX one.
    struct shadow_request {
        bool write = false;
        std::uint64_t address = 0;
        std::uint32_t value = 0;
        unsigned width = 4;
        std::string target;
    } shadow_req;
    sc_core::sc_event shadow_go;
    bool shadow_busy = false, shadow_in_reset = false;
    sc_core::sc_time shadow_done;
    void shadow()
    {
        while (true) {
            wait(shadow_go);
            std::uint32_t v = shadow_req.value;
            access(shadow_req.write, shadow_req.address, v, shadow_req.width, true);
            shadow_done = sc_core::sc_time_stamp();
            shadow_in_reset = !top.domain_reset_n(shadow_req.target);
            shadow_busy = false;
        }
    }
    // Register value through the debug path (no policy, no width rule), as the self-test reads it.
    std::uint32_t debug_read(std::uint64_t address)
    {
        unsigned char data[4] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        master->transport_dbg(tx);
        return data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
    }

    // Watchdog reset request `reset_after` after `fire`.
    bool use_ndmreset = false;   // the reset `fire` starts: watchdog request or DM ndmreset
    void watchdog()
    {
        while (true) {
            wait(fire);
            wait(reset_after);
            if (use_ndmreset) {
                top.drive_ndmreset(true);
                wait(1, sc_core::SC_US);
                top.drive_ndmreset(false);
            } else {
                top.drive_watchdog_request(true);
                wait(1, sc_core::SC_US);
                top.drive_watchdog_request(false);
            }
        }
    }
    // JTAG read of the XIP window with a reset 20 ns into it; returns STATUS and ADDR afterwards.
    std::pair<std::uint32_t, std::uint32_t> jtag_xip_read_across_reset(bool ndmreset)
    {
        auto& j = *top.jtag();
        qspi_memory_mapped();
        j.scan_ir(FU3_DBG_IR_CMD_STATUS);
        j.scan_dr(FU3_DBG_STATUS_ERRORS);
        j.scan_ir(FU3_DBG_IR_CMD_ADDR);
        j.scan_dr(0x30000000);
        j.scan_ir(FU3_DBG_IR_CMD_DATA);
        const auto resets = top.reset_history().size();
        use_ndmreset = ndmreset;
        reset_after = sc_core::sc_time(20, sc_core::SC_NS);
        fire.notify();
        j.scan_dr(FU3_DBG_DATA_READ);
        check(wait_for([&] { return top.reset_history().size() > resets && top.system_ready() && top.domain_reset_n("IBEX_DBG"); },
                       sc_core::sc_time(50, sc_core::SC_US)),
              std::string(ndmreset ? "ndmreset" : "watchdog reset") + " during a JTAG XIP read: reset over");
        wait(2, sc_core::SC_US);
        use_ndmreset = false;
        j.scan_ir(FU3_DBG_IR_CMD_STATUS);
        const auto status = static_cast<std::uint32_t>(j.scan_dr(0));
        j.scan_ir(FU3_DBG_IR_CMD_ADDR);
        const auto address = static_cast<std::uint32_t>(j.scan_dr(0));
        return {status, address};
    }
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(10, sc_core::SC_NS);
        return cond();
    }
    void qspi_memory_mapped()
    {
        namespace q = fu3::qspi::reg;
        write(q::AP_QSPIC_BASE + q::DCR, 23u << 16);
        write(q::AP_QSPIC_BASE + q::CR, q::EN);
        write(q::AP_QSPIC_BASE + q::CCR, (3u << 26) | (1u << 24) | (2u << 12) | (1u << 10) | (1u << 8) | 0x03);
    }

    std::uint32_t read(std::uint64_t address)
    {
        std::uint32_t v = 0;
        access(false, address, v);
        return v;
    }
    // HyperRAM answers only after firmware initialisation (Q07-a), at the
    // 200 MHz bus clock (tCSM): PLL, then the controller, before each access.
    void hyperram_ready()
    {
        namespace h = fu3_hyperram::reg;
        const std::uint64_t crm = FU3_CRM_BASE, csr = fu3_hyperram::csr_base;
        if (top.bus_clock_period() != sc_core::sc_time(5, sc_core::SC_NS)) {
            write(crm + FU3_CRM_PLL_OFFSET, 0x150);
            write(crm + FU3_CRM_PLL_CTRL_OFFSET, 0x5948);
            wait_for([&] { return (read(crm + FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK) != 0; },
                     sc_core::sc_time(40, sc_core::SC_US));
            write(crm + FU3_CRM_CLK_OFFSET, 1);
            write(crm + FU3_CRM_CLK_0_OFFSET, 1);
            write(crm + FU3_CRM_CLK_1_OFFSET, 1);
            wait(sc_core::SC_ZERO_TIME);
        }
        write(csr + h::ERROR_CLEAR, ~0u);
        write(csr + h::CTRL, 0);
        write(csr + h::LATENCY, 0x107);
        write(csr + h::TCSM, 200);
        write(csr + h::SETUP, 4);
        write(csr + h::HOLD, 4);
        write(csr + h::RECOVERY, 8);
        write(csr + h::APPLY, 1);
        wait_for([&] { return !(read(csr + h::STATUS) & h::PENDING); }, sc_core::sc_time(100, sc_core::SC_US));
        write(csr + h::CTRL, 1);
        check(read(csr + h::STATUS) & h::READY, "HyperRAM controller ready");
    }

    void run()
    {
        check(wait_for([&] { return top.system_ready(); }, sc_core::sc_time(100, sc_core::SC_US)), "power-on reset ends");
        top.peripherals().flash().load({0x11, 0x22, 0x33, 0x44});
        const auto warnings = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);
        std::uint64_t aborted = 0, sweeps = 0;
        const sc_core::sc_time offsets[] = {sc_core::SC_ZERO_TIME, sc_core::sc_time(20, sc_core::SC_NS),
                                            sc_core::sc_time(200, sc_core::SC_NS), sc_core::sc_time(2, sc_core::SC_US)};
        for (const auto& i : top.instances()) {
            if (!top.gate(i.name))
                continue;   // the CRM: its own reset domain, it drives the reset
            const auto* probe = top.probe(i.name);
            const std::uint64_t address = i.base + (probe ? probe->offset : 0);
            // UART registers are byte wide. IBEX_DBG (the DM) is closed to the IBEX
            // port by the access profile: its accesses end at the bus, before the gate.
            const unsigned width = i.name.rfind("UART", 0) == 0 ? 1 : 4;
            for (const bool write : {false, true})
            for (const auto& offset : offsets) {
                const std::string what = i.name + (write ? " write" : " read") + ", reset after " + offset.to_string();
                if (i.name == "QSPI_XIP")
                    qspi_memory_mapped();
                if (i.name == "HYPERBUS")
                    hyperram_ready();
                const auto resets = top.reset_history().size();
                const auto aborted_before = top.gate(i.name)->aborted_by_reset();
                reset_after = offset;
                fire.notify();
                std::uint32_t value = write && probe ? probe->value : 0;
                shadow_req = {write, address, value, width, i.name};
                shadow_busy = true;
                shadow_go.notify();
                const auto start = sc_core::sc_time_stamp();
                access(write, address, value, width);
                const auto done = sc_core::sc_time_stamp();
                const bool domain_in_reset = !top.domain_reset_n(i.name);
                check(done - start < sc_core::sc_time(50, sc_core::SC_US), what + ": access completes");
                check(wait_for([&] { return !shadow_busy; }, sc_core::sc_time(50, sc_core::SC_US)),
                      what + ": the access queued from the CVA6 port completes");
                // Drain before release: an access the reset caught (it was still
                // on its way when the reset came) ends while the domain is in reset.
                check(wait_for([&] { return top.reset_history().size() > resets; }, sc_core::sc_time(5, sc_core::SC_US)),
                      what + ": the reset happened");
                const auto reset_time = top.reset_history().back().time;
                check(done <= reset_time || domain_in_reset,
                      what + ": completed before the reset or while the domain is still in reset");
                check(shadow_done <= reset_time || shadow_in_reset,
                      what + ": the queued CVA6-port access too");
                check(wait_for([&] { return top.system_ready() && top.domain_reset_n(i.name); },
                               sc_core::sc_time(50, sc_core::SC_US)),
                      what + ": system ready and domain released again");
                aborted += top.gate(i.name)->aborted_by_reset() - aborted_before;
                ++sweeps;
                wait(2, sc_core::SC_US);   // watchdog request released
            }
            // Usable again, at its reset value.
            if (i.name == "QSPI_XIP")
                qspi_memory_mapped();
            if (i.name == "HYPERBUS")
                hyperram_ready();
            std::uint32_t value = 0;
            const auto status = access(false, address, value, width);
            if (i.name != "IBEX_DBG")
                check(status == tlm::TLM_OK_RESPONSE, i.name + ": functional access answers again after the resets");
            if (probe)
                check(debug_read(address) == probe->value, i.name + ": probe register at its reset value after the resets");
        }
        check(sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) == warnings,
              "no warning from accesses the resets caught (" +
                  std::to_string(sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) - warnings) + ")");
        check(aborted > 0, "some accesses reached a gate during the reset (aborted_by_reset counted)");
        std::cout << "reset sweep: " << sweeps << " accesses, " << aborted << " aborted by reset at the gate\n";

        // Outside a reset a refusal is a software error: clock gate -> warning.
        write(FU3_CRM_BASE + FU3_CRM_CLK_GATE_0_OFFSET, 0x1ff & ~FU3_CRM_GATE0_DSRAM);
        std::uint32_t v = 0;
        const auto before = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);
        check(access(false, 0x0100'0000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE &&
                  top.gate("DSRAM")->current_state() == gated_target::state::clock_gated &&
                  sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) == before + 1,
              "gated DSRAM outside a reset: refused with a warning");
        write(FU3_CRM_BASE + FU3_CRM_CLK_GATE_0_OFFSET, 0x1ff);

        // A gated target reached during a system reset: aborted by the reset.
        const std::uint64_t gate1 = FU3_CRM_BASE + FU3_CRM_CLK_GATE_1_OFFSET, rst1 = FU3_CRM_BASE + FU3_CRM_SW_RST_N_1_OFFSET;
        constexpr std::uint32_t timer1 = 1u << 19;   // TIMER1: CLK_GATE_1[19], SW_RST_N_1[19]
        write(gate1, read(gate1) & ~timer1);
        const auto warnings_gated = sc_core::sc_report_handler::get_count(sc_core::SC_WARNING);
        const auto aborted_gated = top.gate("TIMER1")->aborted_by_reset();
        reset_after = sc_core::SC_ZERO_TIME;
        fire.notify();
        access(false, 0x44070000 + 0x8, v);
        check(top.gate("TIMER1")->aborted_by_reset() == aborted_gated + 1 &&
                  sc_core::sc_report_handler::get_count(sc_core::SC_WARNING) == warnings_gated,
              "gated TIMER1 reached during a system reset: aborted by the reset, no warning");
        check(wait_for([&] { return top.system_ready(); }, sc_core::sc_time(50, sc_core::SC_US)), "ready after it");
        wait(2, sc_core::SC_US);

        // JTAG access on its way across a reset of the DBG domain (review T21-R01).
        const auto [wdt_status, wdt_address] = jtag_xip_read_across_reset(false);
        check(wdt_status == 0 && wdt_address == 0,
              "watchdog reset (DBG included) during a JTAG XIP read: no late STATUS/ADDR in the reset bridge (status " +
                  std::to_string(wdt_status) + ", address " + std::to_string(wdt_address) + ")");
        {
            auto& j = *top.jtag();
            j.scan_ir(FU3_DBG_IR_CMD_ADDR);
            j.scan_dr(0x44010100);   // FU3_VP_ABI_1 VP_ABI_VERSION
            j.scan_ir(FU3_DBG_IR_CMD_DATA);
            j.scan_dr(FU3_DBG_DATA_READ);
            const auto version = static_cast<std::uint32_t>(j.scan_dr(0));
            j.scan_ir(FU3_DBG_IR_CMD_STATUS);
            check(version == 0x10000 && j.scan_dr(0) == 0, "the next JTAG access after the reset works");
        }
        const auto [ndm_status, ndm_address] = jtag_xip_read_across_reset(true);
        check(ndm_status == FU3_DBG_STATUS_SLVERR && ndm_address == 0x30000000,
              "ndmreset (DBG kept) during a JTAG XIP read: the bridge reports the error of its access");
        {
            auto& j = *top.jtag();
            j.scan_ir(FU3_DBG_IR_CMD_STATUS);
            j.scan_dr(FU3_DBG_STATUS_ERRORS);
        }

        // Release while gated: held until the clock returns, then two bus periods.
        write(rst1, read(rst1) & ~timer1);
        wait(top.bus_clock_period() * 4);
        write(rst1, read(rst1) | timer1);   // released while TIMER1 is still gated
        wait(top.bus_clock_period() * 10);
        check(!top.domain_reset_n("TIMER1"), "release while gated: TIMER1 still in reset");
        write(gate1, read(gate1) | timer1);
        const auto ungated = sc_core::sc_time_stamp();
        for (unsigned i = 0; i < 1000 && !top.domain_reset_n("TIMER1"); ++i)
            wait(1, sc_core::SC_NS);   // 1 ns steps: the bus period is 5 ns here
        check(top.domain_reset_n("TIMER1"), "TIMER1 released once its clock runs");
        const auto held = sc_core::sc_time_stamp() - ungated;
        check(held >= top.bus_clock_period() * 2 && held < top.bus_clock_period() * 3,
              "release synchronizer: two bus periods after the clock returns (" + held.to_string() + ")");
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("fu3_soc/gated", sc_core::SC_WARNING, sc_core::SC_DISPLAY);
    bench b("bench");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_SEC));
    b.check(b.finished, "bench reached its end");
    std::cout << "fu3_soc reset: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.failures ? "FAIL" : "PASS") << '\n';
    return b.failures ? 1 : 0;
}
