// Plan T21: debug halt and resets around transactions in flight (CPUs on).
// Ibex runs from the ROM a loop of loads from the QSPI XIP window, 4 KiB apart
// so that each one misses the controller's cache and takes several
// microseconds of serial flash traffic:
//   - halt requests at many points of a load: the hart halts at the next
//     instruction boundary (no hang, the load completes with the flash data,
//     dpc inside the loop), and resumes;
//   - ndmreset while halted with the halt request held: the hart halts again
//     on its first instruction (dpc 0x80), havereset set, DM state kept;
//   - watchdog reset while halted: the DBG domain resets too (halt request
//     dropped), the hart restarts from 0x80 and runs.
// The hart control is the one the Debug Module drives (cpu_debug_hart).

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

#include <fu3_dbg/registers.h>

#include "cpu_debug_hart.h"
#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

// At 0x80: QSPI memory-mapped (DCR FSIZE 23, CR EN, CCR mode 3, 24-bit
// address, READ 0x03); t2 = 0x3000_0000, s0 = 4 KiB, s1 = 0x300f_f000; then
// loop: lw a0, 0(t2); addi a1, a1, 1; add t2, t2, s0; and t2, t2, s1; j loop.
constexpr std::uint32_t kRom[] = {0x440002b7, 0x00170337, 0x0062a223, 0x00100313, 0x0062a023, 0x0d002337,
                                  0x50330313, 0x0062aa23, 0x300003b7, 0x00001437, 0x300ff4b7, 0x0003a503,
                                  0x00158593, 0x008383b3, 0x0093f3b3, 0xff1ff06f};
constexpr std::uint32_t kLoop = 0xac, kLoopEnd = 0xbc;
// Flash word at k * 4 KiB (k < 256).
constexpr std::uint32_t pattern(std::uint32_t k) { return 0xa5000000u | k; }
constexpr std::uint32_t kPark = 0x0000006f;   // j .

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    unsigned checks = 0, failures = 0;
    bool finished = false;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", fu3_soc_options{})
    {
        auto poke = [&](const char* mem, std::uint64_t offset, std::uint32_t word) {
            for (unsigned i = 0; i < 4; ++i)
                top.memory(mem)->poke(offset + i, (word >> (8 * i)) & 0xff);
        };
        for (unsigned i = 0; i < sizeof kRom / 4; ++i)
            poke("ROM", 0x80 + 4 * i, kRom[i]);
        poke("IBEX_ISRAM", 0x10000, kPark);
        poke("IBEX_ISRAM", 0x10008, kPark);
        std::vector<unsigned char> flash(256 * 4096, 0xff);
        for (std::uint32_t k = 0; k < 256; ++k)
            for (unsigned b = 0; b < 4; ++b)
                flash[k * 4096 + b] = (pattern(k) >> (8 * b)) & 0xff;
        top.peripherals().flash().load(flash);
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
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(10, sc_core::SC_NS);
        return cond();
    }
    std::uint32_t reg(cpu_debug_hart& h, std::uint16_t regno)
    {
        std::uint32_t v = 0xdeadbeef;
        h.access_register(regno, false, v);
        return v;
    }

    void run()
    {
        auto& ibex = *top.debug_hart(1);
        auto& dm = top.dm();
        wait(200, sc_core::SC_US);   // CRM startup, QSPI set up, loads running

        // Halts at many points of the XIP loads.
        std::uint32_t last_count = 0;
        sc_core::sc_time longest;
        for (unsigned k = 0; k < 12; ++k) {
            wait(sc_core::sc_time(3000 + 613 * k, sc_core::SC_NS));
            const auto start = sc_core::sc_time_stamp();
            ibex.set_halt_request(true);
            const bool halted = wait_for([&] { return ibex.halted(); }, sc_core::sc_time(20, sc_core::SC_US));
            ibex.set_halt_request(false);
            const auto latency = sc_core::sc_time_stamp() - start;
            if (latency > longest)
                longest = latency;
            const auto pc = reg(ibex, 0x7b1), a0 = reg(ibex, 0x100a), a1 = reg(ibex, 0x100b), t2 = reg(ibex, 0x1007);
            const std::uint32_t line = (t2 >> 12) & 0xff;   // next address; the last load read it or the one before
            const std::string at = "halt " + std::to_string(k) + ": ";
            check(halted, at + "hart halted (no hang)");
            check(pc >= kLoop && pc <= kLoopEnd, at + "dpc inside the loop (" + std::to_string(pc) + ")");
            check(a1 == 0 || a0 == pattern(line) || a0 == pattern((line - 1) & 0xff),
                  at + "the completed load holds its flash word (a0 " + std::to_string(a0) + ")");
            check(a1 >= last_count, at + "loop count goes on");
            last_count = a1;
            ibex.resume();
            check(wait_for([&] { return !ibex.halted(); }, sc_core::sc_time(5, sc_core::SC_US)), at + "resumed");
        }
        check(last_count >= 10, "the loop ran between the halts (" + std::to_string(last_count) + " loads)");
        check(longest > sc_core::sc_time(1, sc_core::SC_US),
              "some halt waited for a load in flight (longest " + longest.to_string() + ")");
        std::cout << "longest halt latency " << longest << " (an XIP load in flight completes first)\n";

        // ndmreset while halted, halt request held: halts again at 0x80.
        dm.dmi_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1));
        dm.dmi_write(FU3_DBG_DMCONTROL,
                     FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | FU3_DBG_DMCONTROL_ACKHAVERESET);
        dm.dmi_write(FU3_DBG_DMCONTROL,
                     FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([&] { return ibex.halted(); }, sc_core::sc_time(20, sc_core::SC_US)), "DM halts Ibex");
        const auto resets = top.reset_history().size();
        dm.dmi_write(FU3_DBG_DMCONTROL, FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) |
                                            FU3_DBG_DMCONTROL_HALTREQ | FU3_DBG_DMCONTROL_NDMRESET);
        wait(2, sc_core::SC_US);
        dm.dmi_write(FU3_DBG_DMCONTROL,
                     FU3_DBG_DMCONTROL_DMACTIVE | FU3_DBG_DMCONTROL_HARTSEL(1) | FU3_DBG_DMCONTROL_HALTREQ);
        check(wait_for([&] { return top.reset_history().size() > resets && ibex.halted() && !ibex.in_reset(); },
                       sc_core::sc_time(500, sc_core::SC_US)),
              "ndmreset with the halt request held: halted again after the reset");
        check(reg(ibex, 0x7b1) == 0x80, "halted on its first instruction (dpc 0x80)");
        check(dm.dmi_read(FU3_DBG_DMSTATUS) & FU3_DBG_DMSTATUS_ANYHAVERESET, "havereset set, DM state kept");

        // Watchdog reset while halted: DBG reset too, the hart restarts and runs.
        const auto resets2 = top.reset_history().size();
        top.drive_watchdog_request(true);
        wait(1, sc_core::SC_US);
        top.drive_watchdog_request(false);
        check(wait_for([&] { return top.reset_history().size() > resets2 && !ibex.in_reset(); },
                       sc_core::sc_time(500, sc_core::SC_US)),
              "watchdog reset while halted: the hart is released again");
        const auto instret = top.ibex()->get_instret();
        wait(100, sc_core::SC_US);
        check(!ibex.halted() && top.ibex()->get_instret() > instret && dm.dmi_read(FU3_DBG_DMCONTROL) == 0,
              "the DBG reset dropped the halt request: the hart runs, DMCONTROL at reset");
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
    std::cout << "fu3_soc halt: " << b.checks << " checks, " << b.failures << " failures -> "
              << (b.failures ? "FAIL" : "PASS") << '\n';
    return b.failures ? 1 : 0;
}
