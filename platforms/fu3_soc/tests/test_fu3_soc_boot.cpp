// Plan T07: Ibex boot firmware (tests/fw/t07_boot_tests.c) through three boots
// of the assembled platform: power-on, software reset, global reset.
//
//   test_fu3_soc_boot [-q <quantum ns>] <t07_boot.elf>
//
// The harness only provides the board: straps boot = mode_0 = 1 at power-on,
// changed to mode_1 = 1 while boot 1 runs. It reads the firmware verdict from
// the Ibex test mailbox and checks what firmware cannot see: the recorded
// reset causes (decision Q05-b), the boot order and the CPU clock.

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

constexpr std::uint32_t kPass = 0x600dc0de;
constexpr std::uint64_t kMailbox = 0x0103'ff40 - 0x0100'0000;     // DSRAM offsets
constexpr std::uint64_t kCva6Starts = 0x0103'fe00 - 0x0100'0000;  // T07_CVA6_STARTS

struct harness : sc_core::sc_module {
    fu3_soc_top top;
    std::uint32_t status = 0;
    unsigned failures = 0;

    static fu3_soc_options board(sc_core::sc_time quantum)
    {
        fu3_soc_options o;
        o.board.boot = true;
        o.board.mode_0 = true;
        o.quantum = quantum;
        return o;
    }

    SC_HAS_PROCESS(harness);
    harness(sc_core::sc_module_name n, const std::string& elf, sc_core::sc_time quantum)
        : sc_module(n), top("fu3_soc", board(quantum))
    {
        top.ibex()->load_elf(elf);
        SC_THREAD(run);
    }
    std::uint32_t word(std::uint64_t offset)
    {
        auto* m = top.memory("DSRAM");
        return m->peek(offset) | m->peek(offset + 1) << 8 | m->peek(offset + 2) << 16 |
               std::uint32_t(m->peek(offset + 3)) << 24;
    }
    void check(bool ok, const std::string& what)
    {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << what << '\n';
        }
    }
    unsigned count(const std::string& event) const
    {
        unsigned n = 0;
        for (const auto& e : top.events())
            n += e.second == event;
        return n;
    }
    // Position of the first `event` in the platform event log (log size if absent).
    std::size_t first(const std::string& event) const
    {
        std::size_t i = 0;
        while (i < top.events().size() && top.events()[i].second != event)
            ++i;
        return i;
    }
    void run()
    {
        // Change the straps while boot 1 runs (after its handoff, before its
        // software reset): its own straps were latched at reset release.
        const auto deadline = sc_core::sc_time(50, sc_core::SC_MS);
        while (sc_core::sc_time_stamp() < deadline && word(kCva6Starts) == 0)
            wait(100, sc_core::SC_NS);
        top.set_straps(false, false, true);
        while (sc_core::sc_time_stamp() < deadline && !(status = word(kMailbox)))
            wait(1, sc_core::SC_US);
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int argc, char** argv)
{
    sc_core::sc_time quantum(1, sc_core::SC_US);
    if (argc > 2 && std::string(argv[1]) == "-q") {
        quantum = sc_core::sc_time(std::stod(argv[2]), sc_core::SC_NS);
        argc -= 2;
        argv += 2;
    }
    if (argc != 2) {
        std::cerr << "usage: test_fu3_soc_boot [-q <quantum ns>] <t07_boot.elf>\n";
        return 2;
    }
    sc_core::sc_report_handler::set_actions("fu3_soc/not_integrated", sc_core::SC_DO_NOTHING);
    sc_core::sc_report_handler::set_actions("fu3_soc/gated", sc_core::SC_DO_NOTHING);
    harness h("harness", argv[1], quantum);
    sc_core::sc_start();

    // Reset causes, oldest first: power-on, software, global.
    std::vector<unsigned> causes;
    for (const auto& r : h.top.reset_history())
        causes.push_back(r.causes);
    h.check(causes == std::vector<unsigned>{reset_por, reset_software, reset_global},
            "reset causes POR, software, global");
    h.check(h.count("Ibex released") == 3, "Ibex booted three times");
    h.check(sc_core::sc_report_handler::get_count("fu3_crm/clock_switch") == 0,
            "boot firmware switches clocks in the supported sequence (CRM spec 3.7.4)");
    h.check(h.count("boot staging closed") == 1 && h.count("CVA6 released") == 2 &&
                h.first("boot staging closed") < h.first("CVA6 released"),
            "one handoff closed staging before releasing CVA6; the local CVA6 reset released it again");
    h.check(h.count("CPU clock 400000000 Hz") == 1 && h.count("CPU clock 20000000 Hz") >= 1 &&
                h.top.cpu_clock_period() == sc_core::sc_time(50, sc_core::SC_NS),
            "400 MHz set by boot 1, kept by the software reset, back to 20 MHz by the global reset");

    const auto checks = h.word(kMailbox + 4), failed = h.word(kMailbox + 8), detail = h.word(kMailbox + 12);
    std::cout << "fu3_soc boot firmware: " << checks << " checks, status 0x" << std::hex << std::setw(8)
              << std::setfill('0') << h.status << std::dec;
    if (failed == 0xeee)
        std::cout << ", unexpected exception: mcause " << h.word(kMailbox + 40) << ", mepc 0x" << std::hex
                  << h.word(kMailbox + 44) << ", mtval 0x" << h.word(kMailbox + 48) << std::dec;
    else if (failed)
        std::cout << ", first failed check " << failed << " (observed 0x" << std::hex << detail << std::dec << ")";
    std::cout << ", " << h.top.reset_history().size() << " resets, " << h.failures << " harness failures, "
              << sc_core::sc_time_stamp() << ", quantum " << quantum << " -> " << (h.status == kPass && !h.failures ? "PASS" : "FAIL") << '\n';
    return h.status == kPass && !h.failures ? 0 : 1;
}
