// vp_fu3_soc: FU3 SoC virtual platform (plan T20: image loading and board
// control; plan T19: GDB server per CPU).
//
//   vp_fu3_soc --rom <rom.elf|rom.bin> [--elf FILE ...] [--load ADDR:FILE ...]
//              [--flash FILE[@OFFSET] ...] [board, console, stimulus, run options]
//   vp_fu3_soc --list-instances | --self-test | --version | --help
//
// Images are a test preload (image_loader.h), written before the simulation
// starts and outside the hardware boot flow; the product Boot ROM belongs to
// FW/SW. Ibex resets at 0x0000_0080 in the Ibex-only ROM, so a run needs ROM
// content. The firmware interface is FU3_VP_ABI_1 (doc/FU3_VP_ABI_1.md).
//
// Exit status: 0 PASS (or, without --expect-uart, the run reached its time
// budget), 1 FAIL (--fail-uart text seen, also after the --expect-uart text,
// up to the stop at the end of that console line; self-test failure), 2 timeout
// (--expect-uart text not seen in time), 3 simulator, image or integration
// error, 4 usage.

#include <array>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <systemc>

#include "fu3_soc_build_info.h"
#include "fu3_soc_top.h"
#include "fu3_vp_abi.h"
#include "gdb_server.h"
#include "image_loader.h"
#include "self_test.h"

namespace {

using namespace cdc::platforms::fu3_soc;

enum exit_code { exit_pass = 0, exit_fail = 1, exit_timeout = 2, exit_error = 3, exit_usage = 4 };

void version()
{
    std::cout << "vp_fu3_soc " << fu3_soc_build::package << " (FU3 SoC VP: CVA6 + Ibex, Common Spec v2.1 IP set)\n"
              << "  CDC-VP revision " << fu3_soc_build::git_revision << " (" << fu3_soc_build::git_state
              << " at configure)\n"
              << "  build type      " << fu3_soc_build::build_type << ", " << fu3_soc_build::cxx_compiler << "\n"
              << "  SystemC         " << sc_core::sc_release() << "\n"
              << "  bus map         " << fu3_bus::BusConfig::fu3().revision << " (fu3_bus::BusConfig::fu3())\n"
              << "  firmware ABI    FU3_VP_ABI_1 (0x" << std::hex << FU3_VP_ABI_VERSION_VALUE << std::dec << ")\n";
}

void usage(std::ostream& out)
{
    out << "usage: vp_fu3_soc --rom FILE [options]\n"
           "       vp_fu3_soc --list-instances | --self-test | --version | --help\n"
           "Images (test preload before the run; not a Boot ROM):\n"
           "  --rom FILE            ROM content: an ELF (its other segments load too) or a raw\n"
           "                        image at 0x0000_0000; required for a run (Ibex resets at 0x80)\n"
           "  --elf FILE            an ELF; every segment must lie in one preload region\n"
           "  --load ADDR:FILE      a raw file at a system address\n"
           "  --flash FILE[@OFF]    a raw file into the 16 MB QSPI NOR at OFF (default 0)\n"
           "                        Regions: ROM, CVA6_ISRAM 0x10000-0x3FFFF, IBEX_ISRAM 0x50000-0x5FFFF,\n"
           "                        DSRAM 0x1000000-0x103FFFF, QSPI XIP 0x30000000-0x30FFFFFF (NOR);\n"
           "                        debug partitions, HyperRAM and MMIO are refused; code is checked\n"
           "                        against the ISA of the CPU owning its region\n"
           "  --flash-out FILE      write the NOR content after the run\n"
           "  --dump ADDR:LEN:FILE  write LEN bytes of an on-chip memory after the run (repeatable)\n"
           "Board (Common v2.1 die pads):\n"
           "  --mode M1M0           boot mode straps MODE_1 MODE_0, e.g. 10 (default 00)\n"
           "  --boot-pin 0|1        BOOT strap (default 0)\n"
           "  --test-mode 0|1, --test-mode-sel 0|1\n"
           "  --debug-en 0|1        DEBUG_EN pad from power-on (default 0)\n"
           "  --debug-en-release-us N  drive DEBUG_EN low at N us of simulated time\n"
           "  --pad DIE=SEL         csr_io_cfg select of a die pad (repeatable; SYSCSR mapping pending)\n"
           "  --pad-level DIE=0|1   board level of a die pad (repeatable)\n"
           "Console and stimulus:\n"
           "  --console N|none      UART whose line is the console (default 0)\n"
           "  --quiet-uart          do not echo console lines to stdout\n"
           "  --uart-log FILE       write every console character to FILE after the run\n"
           "  --uart-rx-file FILE   send FILE to the console UART RX\n"
           "  --uart-rx-delay-us N  start sending at N us (default 0)\n"
           "  --adc CH=VALUE        12-bit value of ADC channel CH (repeatable)\n"
           "  --rng-seed N          RNG bit-source seed (printed at start)\n"
           "  --spi-loopback N      board wire MOSI -> MISO on SPI N's pads (repeatable)\n"
           "Run:\n"
           "  --timeout-ms N        simulated-time budget (default 1000; 0 = until a verdict)\n"
           "  --quantum-ns N        TLM global quantum (default 1000)\n"
           "  --expect-uart TEXT    PASS (exit 0) when the console has printed TEXT\n"
           "  --fail-uart TEXT      FAIL (exit 1) when the console has printed TEXT; it wins over\n"
           "                        --expect-uart when both appear before the run stops\n"
           "Debug (GDB remote protocol on 127.0.0.1; the run then has no time budget unless --timeout-ms):\n"
           "  --gdb-cva6 PORT       GDB server for CVA6 (hart 0); 0 = any free port (printed)\n"
           "  --gdb-ibex PORT       GDB server for Ibex (hart 1)\n"
           "  --gdb-no-wait         start at once instead of waiting for every GDB client\n"
           "Other:\n"
           "  --list-instances      print the bus map and every target's state, then exit\n"
           "  --self-test           check decode, models, error responses and unmapped space\n"
           "  --trace, --verbose, --version, --help\n";
}

struct cli {
    fu3_soc_options options;
    bool list = false, self_test = false, verbose = false, quiet_uart = false;
    std::string rom, flash_out, uart_log, uart_rx_file, expect, fail;
    std::vector<std::string> elfs;
    std::vector<std::pair<std::uint64_t, std::string>> loads;
    std::vector<std::pair<std::string, std::uint64_t>> flashes;
    struct dump_request {
        std::uint64_t address, length;
        std::string path;
    };
    std::vector<dump_request> dumps;
    std::vector<std::pair<unsigned, unsigned>> pads, pad_levels, adc;
    std::vector<unsigned> spi_loopback;
    bool rng_seed_set = false;
    std::uint64_t rng_seed = 0;
    double debug_en_release_us = -1, uart_rx_delay_us = 0, timeout_ms = 1000;
    bool timeout_set = false;
    std::array<int, 2> gdb_ports{-1, -1};
    bool gdb_wait = true;
};

bool parse_u64(const std::string& text, std::uint64_t max, std::uint64_t& value)
{
    try {
        std::size_t used = 0;
        value = std::stoull(text, &used, 0);
        return used == text.size() && value <= max;
    } catch (const std::exception&) {
        return false;
    }
}
bool parse_double(const std::string& text, double& value)
{
    try {
        std::size_t used = 0;
        value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value) && value >= 0;
    } catch (const std::exception&) {
        return false;
    }
}
// A=B with unsigned A <= max_a and B <= max_b.
bool parse_pair(const std::string& text, std::uint64_t max_a, std::uint64_t max_b, std::pair<unsigned, unsigned>& out)
{
    const auto eq = text.find('=');
    std::uint64_t a = 0, b = 0;
    if (eq == std::string::npos || !parse_u64(text.substr(0, eq), max_a, a) || !parse_u64(text.substr(eq + 1), max_b, b))
        return false;
    out = {unsigned(a), unsigned(b)};
    return true;
}
bool parse_bit(const std::string& text, bool& value)
{
    if (text != "0" && text != "1")
        return false;
    value = text == "1";
    return true;
}

// Run control: console stimulus, DEBUG_EN release and the console verdict.
struct runner : sc_core::sc_module {
    fu3_soc_top& top;
    const cli& args;
    int verdict = -1;   // -1 none yet, exit_pass or exit_fail

    SC_HAS_PROCESS(runner);
    runner(sc_core::sc_module_name n, fu3_soc_top& t, const cli& a) : sc_module(n), top(t), args(a)
    {
        if (!args.uart_rx_file.empty())
            SC_THREAD(console_rx);
        if (args.debug_en_release_us >= 0)
            SC_THREAD(debug_en);
        if (!args.expect.empty() || !args.fail.empty())
            SC_THREAD(watch);
    }
    uart_line& console() { return top.peripherals().uart_peer(unsigned(args.options.console_uart)); }
    void console_rx()
    {
        std::ifstream in(args.uart_rx_file, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        wait(sc_core::sc_time(args.uart_rx_delay_us, sc_core::SC_US));
        console().send(bytes);
    }
    void debug_en()
    {
        wait(sc_core::sc_time(args.debug_en_release_us, sc_core::SC_US));
        top.set_debug_en(false);
    }
    // Console verdict over everything the console printed so far: the fail
    // text wins over the expected one wherever both appear.
    int judge() const
    {
        const auto& text = top.peripherals().uart_peer(unsigned(args.options.console_uart)).received();
        if (!args.fail.empty() && text.find(args.fail) != std::string::npos)
            return exit_fail;
        if (!args.expect.empty() && text.find(args.expect) != std::string::npos)
            return exit_pass;
        return -1;
    }
    // Decides when to stop; the verdict itself is judged on the whole console
    // output once the simulation has stopped (review R01: a fail text printed
    // after the expected one, before the stop, makes the run FAIL).
    void watch()
    {
        while (judge() < 0)
            wait(1, sc_core::SC_US);
        // Let the console finish the line it is printing: stop at its end of
        // line, or when no character has arrived for 1 ms.
        const auto from = console().received().size();
        std::size_t seen = from;
        for (unsigned idle = 0; idle < 1000 && console().received().find('\n', from - 1) == std::string::npos;) {
            wait(1, sc_core::SC_US);
            const auto now = console().received().size();
            idle = now == seen ? idle + 1 : 0;
            seen = now;
        }
        sc_core::sc_stop();
    }
};

int run(const cli& args)
{
    if (!args.uart_rx_file.empty() && !std::ifstream(args.uart_rx_file))
        throw image_error("cannot read '" + args.uart_rx_file + "'");
    fu3_soc_top top("fu3_soc", args.options);
    image_loader loader(top);
    if (!args.rom.empty())
        loader.load_rom(args.rom);
    for (const auto& e : args.elfs)
        loader.load_elf(e);
    for (const auto& [address, path] : args.loads)
        loader.load_binary(address, path);
    for (const auto& [path, offset] : args.flashes)
        loader.load_flash(path, offset);
    if (!loader.rom_loaded()) {
        std::cerr << "vp_fu3_soc: no ROM image: Ibex resets at 0x0000_0080 in the Ibex-only ROM, so a run\n"
                     "  needs ROM content (--rom FILE, or an --elf with a ROM segment). The product Boot ROM\n"
                     "  belongs to FW/SW; tests/fw has small test ROM stubs.\n";
        return exit_error;
    }

    auto& p = top.peripherals();
    for (const auto& [die, select] : args.pads)
        p.set_pad_select(die, select);
    for (const auto& [die, level] : args.pad_levels)
        p.set_board_pad(die, level != 0);
    for (const auto& [channel, value] : args.adc)
        p.adc_input().set_input(channel, static_cast<std::uint16_t>(value));
    if (args.rng_seed_set)
        p.set_rng_seed(args.rng_seed);
    for (const auto k : args.spi_loopback)
        p.set_spi_loopback(k, true);
    if (args.options.console_uart >= 0 && args.quiet_uart)
        p.uart_peer(unsigned(args.options.console_uart)).set_echo(nullptr, "");

    const auto& b = args.options.board;
    std::cout << "vp_fu3_soc: FU3_VP_ABI_1, straps MODE=" << b.mode_1 << b.mode_0 << " BOOT=" << b.boot
              << " DEBUG_EN=" << b.debug_en;
    if (args.debug_en_release_us >= 0)
        std::cout << " (released at " << args.debug_en_release_us << " us)";
    std::cout << ", RNG seed 0x" << std::hex << p.rng_seed() << std::dec << ", console "
              << (args.options.console_uart >= 0 ? "UART" + std::to_string(args.options.console_uart) : "none")
              << ", quantum " << args.options.quantum << '\n';
    for (const auto& r : loader.records())
        std::cout << "vp_fu3_soc: preload " << r.region << " [0x" << std::hex << r.begin << ", 0x" << r.end << std::dec
                  << ") from " << r.source << '\n';

    runner control("runner", top, args);
    std::unique_ptr<gdb_server> gdb;
    if (args.gdb_ports[0] >= 0 || args.gdb_ports[1] >= 0)
        gdb = std::make_unique<gdb_server>("gdb", top, args.gdb_ports, args.gdb_wait);
    if (args.timeout_ms > 0)
        sc_core::sc_start(sc_core::sc_time(args.timeout_ms, sc_core::SC_MS));
    else
        sc_core::sc_start();

    for (const auto& d : args.dumps) {
        const auto bytes = loader.dump(d.address, d.length);
        std::ofstream out(d.path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        if (!out)
            throw image_error("cannot write '" + d.path + "'");
    }
    if (!args.flash_out.empty())
        p.flash().save_image(args.flash_out);
    if (!args.uart_log.empty() && args.options.console_uart >= 0) {
        std::ofstream out(args.uart_log, std::ios::binary);
        out << p.uart_peer(unsigned(args.options.console_uart)).received();
        if (!out)
            throw image_error("cannot write '" + args.uart_log + "'");
    }

    std::cout << "vp_fu3_soc: stopped at " << sc_core::sc_time_stamp() << ", Ibex " << top.ibex()->get_instret()
              << " instructions, CVA6 " << top.cva6()->get_instret() << " instructions, "
              << top.reset_history().size() << " resets\n";
    top.print_access_summary(std::cout);
    // Verdict on everything the console printed up to the stop (fail first).
    if (args.options.console_uart >= 0 && (!args.expect.empty() || !args.fail.empty()))
        control.verdict = control.judge();
    if (control.verdict == exit_pass) {
        std::cout << "vp_fu3_soc: PASS (console printed \"" << args.expect << "\")\n";
        return exit_pass;
    }
    if (control.verdict == exit_fail) {
        std::cout << "vp_fu3_soc: FAIL (console printed \"" << args.fail << "\")\n";
        return exit_fail;
    }
    if (gdb && gdb->killed()) {
        std::cout << "vp_fu3_soc: run ended by a GDB kill (no console verdict)\n";
        return exit_pass;
    }
    if (!args.expect.empty()) {
        std::cout << "vp_fu3_soc: TIMEOUT (console did not print \"" << args.expect << "\" within " << args.timeout_ms
                  << " ms)\n";
        return exit_timeout;
    }
    std::cout << "vp_fu3_soc: run ended at its time budget (no --expect-uart)\n";
    return exit_pass;
}

} // namespace

int sc_main(int argc, char** argv)
{
    cli args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string v;
        auto value = [&](std::string& out) {
            if (i + 1 >= argc)
                return false;
            out = argv[++i];
            return true;
        };
        auto bad = [&](const std::string& why) {
            std::cerr << "vp_fu3_soc: " << arg << ": " << why << '\n';
            usage(std::cerr);
            return exit_usage;
        };
        if (arg == "--help" || arg == "-h") {
            usage(std::cout);
            return exit_pass;
        }
        if (arg == "--version") {
            version();
            return exit_pass;
        }
        if (arg == "--list-instances")
            args.list = true;
        else if (arg == "--self-test")
            args.self_test = true;
        else if (arg == "--trace")
            args.options.bus_trace = true;
        else if (arg == "--verbose")
            args.verbose = true;
        else if (arg == "--quiet-uart")
            args.quiet_uart = true;
        else if (arg == "--rom") {
            if (!value(args.rom))
                return bad("needs a file");
        } else if (arg == "--elf") {
            if (!value(v))
                return bad("needs a file");
            args.elfs.push_back(v);
        } else if (arg == "--load") {
            std::uint64_t address = 0;
            const auto colon = value(v) ? v.find(':') : std::string::npos;
            if (colon == std::string::npos || !parse_u64(v.substr(0, colon), 0xffff'ffff, address) || colon + 1 == v.size())
                return bad("expects ADDR:FILE");
            args.loads.push_back({address, v.substr(colon + 1)});
        } else if (arg == "--flash") {
            std::uint64_t offset = 0;
            if (!value(v))
                return bad("needs a file");
            const auto at = v.rfind('@');
            if (at != std::string::npos && !parse_u64(v.substr(at + 1), 0xff'ffff, offset))
                return bad("offset must be 0-0xFFFFFF");
            args.flashes.push_back({at == std::string::npos ? v : v.substr(0, at), offset});
        } else if (arg == "--flash-out") {
            if (!value(args.flash_out))
                return bad("needs a file");
        } else if (arg == "--dump") {
            if (!value(v))
                return bad("expects ADDR:LEN:FILE");
            const auto a = v.find(':'), l = a == std::string::npos ? a : v.find(':', a + 1);
            std::uint64_t address = 0, length = 0;
            if (l == std::string::npos || !parse_u64(v.substr(0, a), 0xffff'ffff, address) ||
                !parse_u64(v.substr(a + 1, l - a - 1), 0x0100'0000, length) || !length || l + 1 == v.size())
                return bad("expects ADDR:LEN:FILE");
            args.dumps.push_back({address, length, v.substr(l + 1)});
        } else if (arg == "--mode") {
            if (!value(v) || v.size() != 2 || (v[0] != '0' && v[0] != '1') || (v[1] != '0' && v[1] != '1'))
                return bad("expects MODE_1 MODE_0 as two bits, e.g. 10");
            args.options.board.mode_1 = v[0] == '1';
            args.options.board.mode_0 = v[1] == '1';
        } else if (arg == "--boot-pin" || arg == "--test-mode" || arg == "--test-mode-sel" || arg == "--debug-en") {
            bool bit = false;
            if (!value(v) || !parse_bit(v, bit))
                return bad("expects 0 or 1");
            auto& board = args.options.board;
            (arg == "--boot-pin"        ? board.boot
             : arg == "--test-mode"     ? board.test_mode
             : arg == "--test-mode-sel" ? board.test_mode_sel
                                        : board.debug_en) = bit;
        } else if (arg == "--debug-en-release-us") {
            if (!value(v) || !parse_double(v, args.debug_en_release_us))
                return bad("expects a time in us");
        } else if (arg == "--pad" || arg == "--pad-level" || arg == "--adc") {
            std::pair<unsigned, unsigned> pair;
            const bool ok = value(v) && (arg == "--pad"         ? parse_pair(v, 108, 3, pair) && pair.first >= 1
                                         : arg == "--pad-level" ? parse_pair(v, 108, 1, pair) && pair.first >= 1
                                                                : parse_pair(v, 15, 0xfff, pair));
            if (!ok)
                return bad(arg == "--pad"         ? "expects DIE=SEL (die 1-108, select 0-3)"
                           : arg == "--pad-level" ? "expects DIE=0|1 (die 1-108)"
                                                  : "expects CH=VALUE (channel 0-15, value 0-0xFFF)");
            (arg == "--pad" ? args.pads : arg == "--pad-level" ? args.pad_levels : args.adc).push_back(pair);
        } else if (arg == "--rng-seed") {
            if (!value(v) || !parse_u64(v, ~std::uint64_t(0), args.rng_seed))
                return bad("expects a number");
            args.rng_seed_set = true;
        } else if (arg == "--spi-loopback") {
            std::uint64_t k = 0;
            if (!value(v) || !parse_u64(v, 3, k))
                return bad("expects 0-3");
            args.spi_loopback.push_back(unsigned(k));
        } else if (arg == "--console") {
            std::uint64_t k = 0;
            if (!value(v) || (v != "none" && !parse_u64(v, 7, k)))
                return bad("expects 0-7 or none");
            args.options.console_uart = v == "none" ? -1 : int(k);
        } else if (arg == "--uart-log" || arg == "--uart-rx-file" || arg == "--expect-uart" || arg == "--fail-uart") {
            if (!value(v) || v.empty())
                return bad("needs a value");
            (arg == "--uart-log"       ? args.uart_log
             : arg == "--uart-rx-file" ? args.uart_rx_file
             : arg == "--expect-uart"  ? args.expect
                                       : args.fail) = v;
        } else if (arg == "--uart-rx-delay-us") {
            if (!value(v) || !parse_double(v, args.uart_rx_delay_us))
                return bad("expects a time in us");
        } else if (arg == "--timeout-ms") {
            if (!value(v) || !parse_double(v, args.timeout_ms))
                return bad("expects a time in ms (0: no limit)");
            args.timeout_set = true;
        } else if (arg == "--gdb-cva6" || arg == "--gdb-ibex") {
            std::uint64_t port = 0;
            if (!value(v) || !parse_u64(v, 65535, port))
                return bad("expects a TCP port (0 = any free port)");
            args.gdb_ports[arg == "--gdb-cva6" ? 0 : 1] = int(port);
        } else if (arg == "--gdb-no-wait") {
            args.gdb_wait = false;
        } else if (arg == "--quantum-ns") {
            std::uint64_t q = 0;
            if (!value(v) || !parse_u64(v, 1'000'000, q) || !q)
                return bad("expects 1-1000000");
            args.options.quantum = sc_core::sc_time(double(q), sc_core::SC_NS);
        } else {
            std::cerr << "vp_fu3_soc: unknown option '" << arg << "'\n";
            usage(std::cerr);
            return exit_usage;
        }
    }
    const bool console_needed = !args.expect.empty() || !args.fail.empty() || !args.uart_rx_file.empty();
    if (console_needed && args.options.console_uart < 0) {
        std::cerr << "vp_fu3_soc: --expect-uart, --fail-uart and --uart-rx-file need a console UART\n";
        return exit_usage;
    }
    const bool gdb = args.gdb_ports[0] >= 0 || args.gdb_ports[1] >= 0;
    if (gdb && !args.timeout_set)
        args.timeout_ms = 0;   // a debug session has no time budget
    if (args.timeout_ms == 0 && args.expect.empty() && args.fail.empty() && !gdb)
        std::cerr << "vp_fu3_soc: --timeout-ms 0 without a console verdict runs until interrupted\n";
    if (args.verbose)
        sc_core::sc_report_handler::set_verbosity_level(sc_core::SC_DEBUG);

    try {
        if (args.self_test || args.list) {
            // The self-test drives the bus through the IBEX port itself.
            if (args.self_test)
                args.options.cpus = false;
            fu3_soc_top top("fu3_soc", args.options);
            top.print_instances(std::cout);
            if (!args.self_test)
                return exit_pass;
            self_test probe("self_test", top, std::cout);
            sc_core::sc_start();
            if (!probe.done()) {
                std::cerr << "vp_fu3_soc: self-test did not finish\n";
                return exit_error;
            }
            top.print_access_summary(std::cout);
            return probe.passed() ? exit_pass : exit_fail;
        }
        return run(args);
    } catch (const std::exception& e) {
        std::cerr << "vp_fu3_soc: " << e.what() << '\n';
        return exit_error;
    }
}
