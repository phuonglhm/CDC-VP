// Plan T05: run a test firmware on one CPU of the assembled platform.
//
//   test_fu3_soc_cpu [-q <quantum ns>] [-d <us>] ibex <image.elf>
//   test_fu3_soc_cpu [-q <quantum ns>] cva6 <image.elf> <ibex_boot.elf>
//
// Loads the image through that CPU's bus port (debug path) and lets the
// platform boot (T07): the CRM releases Ibex after power-on reset; for a CVA6
// run Ibex executes <ibex_boot.elf> (tests/fw/ibex_handoff.S), which sets the
// PLL and releases CVA6 through CRM FCLK_SW. Then waits for the firmware
// verdict in the DSRAM test mailbox
// (tests/fw/fw_common.h). Until the interrupt sources are integrated (T08/T09)
// and the Debug Module is wired (T18), the harness drives the CPU's interrupt
// inputs (bits 0-30) and debug request (bit 31) from the mailbox `req` word and
// mirrors what it drives into `ack`. The mailbox is a test convention only. The
// debug request goes through the platform's DebugHart for that CPU (hartsel 0 =
// CVA6, 1 = Ibex), the path the Debug Module will use.

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include <systemc>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

constexpr std::uint32_t kPass = 0x600dc0de;
constexpr std::uint64_t kDsramBase = 0x0100'0000;

struct harness : sc_core::sc_module {
    fu3_soc_top top;
    cdc::cpu::cva6_wrapper* cpu = nullptr;
    fu3_dbg::DebugHart* hart = nullptr;
    std::uint64_t mailbox = 0;  // DSRAM offset
    std::uint32_t driven = 0;
    std::uint32_t status = 0;
    bool timed_out = false;
    // -d <us>: the board holds DEBUG_EN (die 6) high from power-on and
    // releases it at that time (FU3_VP_ABI_1 Boot ROM wait, review ABI1-R01).
    sc_core::sc_time debug_en_release = sc_core::SC_ZERO_TIME, debug_en_released_at = sc_core::SC_ZERO_TIME;

    SC_HAS_PROCESS(harness);
    harness(sc_core::sc_module_name n, const fu3_soc_options& options, const std::string& which,
            const std::string& elf, const std::string& boot)
        : sc_module(n), top("fu3_soc", options)
    {
        cpu = which == "cva6" ? top.cva6() : top.ibex();
        const auto harts = top.debug_harts();
        if (harts.size() != 2)
            throw std::runtime_error("fu3_soc_top::debug_harts(): expected 2 harts");
        hart = harts[which == "cva6" ? 0 : 1];
        mailbox = (which == "cva6" ? 0x0103'ff00 : 0x0103'ff40) - kDsramBase;
        cpu->load_elf(elf);
        if (which == "cva6")
            top.ibex()->load_elf(boot);
        if (std::getenv("FU3_TRACE"))
            cpu->set_trace(true);
        SC_THREAD(run);
    }

    std::uint32_t word(std::uint64_t offset)
    {
        auto* m = top.memory("DSRAM");
        return m->peek(offset) | m->peek(offset + 1) << 8 | m->peek(offset + 2) << 16 |
               std::uint32_t(m->peek(offset + 3)) << 24;
    }
    void set_word(std::uint64_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            top.memory("DSRAM")->poke(offset + i, (value >> (8 * i)) & 0xff);
    }

    void run()
    {
        const sc_core::sc_time timeout(20, sc_core::SC_MS), poll(20, sc_core::SC_NS);
        const auto deadline = sc_core::sc_time_stamp() + timeout;
        while (sc_core::sc_time_stamp() < deadline) {
            wait(poll);
            if (debug_en_release != sc_core::SC_ZERO_TIME && debug_en_released_at == sc_core::SC_ZERO_TIME &&
                sc_core::sc_time_stamp() >= debug_en_release) {
                top.set_debug_en(false);
                debug_en_released_at = sc_core::sc_time_stamp();
            }
            // Mirror requested interrupt levels onto the CPU inputs.
            const std::uint32_t req = word(mailbox + 16);
            for (unsigned cause = 0; cause < 31; ++cause) {
                const std::uint32_t bit = 1u << cause;
                if ((req ^ driven) & bit)
                    cpu->set_irq(cause, req & bit);
            }
            if ((req ^ driven) & (1u << 31))   // bit 31: debug_req_i (T06)
                hart->set_halt_request(req >> 31);
            driven = req;
            set_word(mailbox + 20, driven);
            status = word(mailbox);
            if (status)
                break;
        }
        timed_out = status == 0;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int argc, char** argv)
{
    fu3_soc_options options;
    sc_core::sc_time debug_en_release = sc_core::SC_ZERO_TIME;
    while (argc > 2 && (std::string(argv[1]) == "-q" || std::string(argv[1]) == "-d")) {
        if (std::string(argv[1]) == "-q") {
            options.quantum = sc_core::sc_time(std::stod(argv[2]), sc_core::SC_NS);
        } else {
            options.board.debug_en = true;
            debug_en_release = sc_core::sc_time(std::stod(argv[2]), sc_core::SC_US);
        }
        argc -= 2;
        argv += 2;
    }
    const std::string which = argc > 1 ? argv[1] : "";
    if (!((which == "ibex" && argc == 3) || (which == "cva6" && argc == 4))) {
        std::cerr << "usage: test_fu3_soc_cpu [-q <quantum ns>] [-d <us>] ibex <image.elf> | cva6 <image.elf> <ibex_boot.elf>\n";
        return 2;
    }
    sc_core::sc_report_handler::set_actions("fu3_soc/not_integrated", sc_core::SC_DO_NOTHING);
    harness h("harness", options, which, argv[2], argc == 4 ? argv[3] : "");
    h.debug_en_release = debug_en_release;
    sc_core::sc_start();

    // With -d: CVA6 must stay in reset while DEBUG_EN is high (Boot ROM wait).
    bool debug_en_ok = true;
    if (debug_en_release != sc_core::SC_ZERO_TIME) {
        sc_core::sc_time cva6_released = sc_core::SC_ZERO_TIME;
        for (const auto& [time, what] : h.top.events())
            if (what == "CVA6 released")
                cva6_released = time;
        // The firmware recorded DEBUG_EN as it read it at boot (0x0103fe24, abi1_boot_tests.c).
        const bool seen_at_boot = h.word(0x0103'fe24 - kDsramBase) == 1;
        debug_en_ok = seen_at_boot && h.debug_en_released_at != sc_core::SC_ZERO_TIME &&
                      cva6_released >= h.debug_en_released_at;
        std::cout << "DEBUG_EN " << (seen_at_boot ? "seen high" : "NOT seen high") << " by the firmware at boot\n";
        std::cout << "DEBUG_EN released at " << h.debug_en_released_at << ", CVA6 released at " << cva6_released
                  << (debug_en_ok ? " (after: OK)" : " (FAIL: CVA6 released while DEBUG_EN was high)") << '\n';
    }

    const auto checks = h.word(h.mailbox + 4), failed = h.word(h.mailbox + 8), detail = h.word(h.mailbox + 12);
    std::cout << "fu3_soc " << argv[1] << " firmware: " << checks << " checks, status 0x" << std::hex
              << std::setw(8) << std::setfill('0') << h.status << std::dec;
    if (h.timed_out)
        std::cout << " (timeout at " << sc_core::sc_time_stamp() << ", pc 0x" << std::hex << h.cpu->get_pc() << ")";
    if (failed == 0xeee)
        std::cout << ", unexpected exception: mcause " << h.word(h.mailbox + 40) << ", mepc 0x" << std::hex
                  << h.word(h.mailbox + 44) << ", mtval 0x" << h.word(h.mailbox + 48) << std::dec;
    else if (failed)
        std::cout << ", first failed check " << failed << " (observed 0x" << std::hex << detail << std::dec << ")";
    std::cout << ", " << h.cpu->get_instret() << " instructions, " << sc_core::sc_time_stamp() << ", CPU clock "
              << h.top.cpu_clock_period() << ", quantum " << tlm::tlm_global_quantum::instance().get() << " -> "
              << (h.status == kPass && debug_en_ok ? "PASS" : "FAIL") << '\n';
    return h.status == kPass && debug_en_ok ? 0 : 1;
}
