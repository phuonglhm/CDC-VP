// SPDX-License-Identifier: Apache-2.0

#include "cli.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <systemc>

#include "riscv_vp_plusplus_wrapper.h"

#include "compiler_vp/host_io_map.h"
#include "build_info.h"

namespace cdc::platforms::riscv_vpp_compiler_vp {

namespace {

/// Defaults for the two watchdogs, chosen as a matched pair.
///
/// The ISS cycle time is 10 ns, so 1 s of simulated time is 100 million cycles
/// and, at roughly one instruction per cycle, about the same number of
/// instructions. Setting the two bounds to describe the same amount of work
/// means a run that trips one of them is genuinely stuck rather than merely
/// larger than one arbitrary limit — and when they *do* disagree, the
/// difference itself is the diagnosis: instructions exhausted with time to
/// spare is a spin, time exhausted with instructions to spare is a stall.
constexpr std::uint64_t kDefaultMaxInstructions = 100'000'000;
constexpr double kDefaultTimeoutNs = 1'000'000'000.0;

/// Wall-clock backstop. Generous against any legitimate run this package can
/// produce — the vector demonstration finishes in about a second — and short
/// enough that a CI job blocked on a non-yielding guest fails within a coffee
/// break instead of being killed by the job's own timeout with no diagnostic.
constexpr double kDefaultWallTimeoutSeconds = 600.0;

/// Default `--trace` budget. A vector example makes tens of thousands of
/// transactions; an unbounded trace turns a diagnostic aid into a way to fill a
/// disk, and the first few hundred are what anyone actually reads.
constexpr std::uint64_t kDefaultTraceLimit = 200;

std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// `std::stoull` accepts a leading `-` and wraps it, so `--ram-size -1` becomes
/// 18446744073709551615 rather than an error. Every count and size here is
/// unsigned by construction, so a minus sign is always a mistake and is always
/// worth saying so about, rather than being turned into an absurd limit that
/// then trips a different check with a confusing message.
void reject_negative(const std::string& value, const std::string& what)
{
    if (!value.empty() && value[0] == '-') {
        throw std::runtime_error(what + ": '" + value
                                 + "' is negative; this option takes a "
                                   "non-negative value");
    }
}

/// Decimal or `0x` hex, with optional binary size suffixes.
std::uint64_t parse_size(const std::string& text, const std::string& what)
{
    const std::string value = trim(text);
    if (value.empty()) {
        throw std::runtime_error(what + ": empty value");
    }
    reject_negative(value, what);

    std::size_t consumed = 0;
    unsigned long long number = 0;
    try {
        number = std::stoull(value, &consumed, 0);
    } catch (const std::exception&) {
        throw std::runtime_error(what + ": '" + value + "' is not a number");
    }

    std::string suffix = trim(value.substr(consumed));
    for (char& c : suffix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::uint64_t multiplier = 1;
    if (suffix.empty()) {
        multiplier = 1;
    } else if (suffix == "k" || suffix == "kib") {
        multiplier = 1024ull;
    } else if (suffix == "m" || suffix == "mib") {
        multiplier = 1024ull * 1024;
    } else if (suffix == "g" || suffix == "gib") {
        multiplier = 1024ull * 1024 * 1024;
    } else {
        throw std::runtime_error(what + ": unknown size suffix '" + suffix
                                 + "'; use K/KiB, M/MiB or G/GiB");
    }

    if (number != 0 && multiplier > (~0ull) / number) {
        throw std::runtime_error(what + ": '" + value + "' overflows 64 bits");
    }
    return static_cast<std::uint64_t>(number) * multiplier;
}

std::uint64_t parse_count(const std::string& text, const std::string& what)
{
    const std::string value = trim(text);
    reject_negative(value, what);
    std::size_t consumed = 0;
    unsigned long long number = 0;
    try {
        number = std::stoull(value, &consumed, 0);
    } catch (const std::exception&) {
        throw std::runtime_error(what + ": '" + value + "' is not a number");
    }
    if (consumed != value.size()) {
        throw std::runtime_error(what + ": '" + value
                                 + "' has trailing characters; this option takes a "
                                   "plain count");
    }
    return static_cast<std::uint64_t>(number);
}

/// A simulated-time value. Bare numbers are nanoseconds; `ns`, `us`, `ms` and
/// `s` are accepted because "how long may this run take" is a question people
/// answer in seconds and a nanosecond count with nine zeros is easy to mistype
/// by one zero.
double parse_time_ns(const std::string& text, const std::string& what)
{
    const std::string value = trim(text);
    std::size_t consumed = 0;
    double number = 0.0;
    try {
        number = std::stod(value, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error(what + ": '" + value + "' is not a number");
    }

    // `std::stod` accepts "nan" and "inf" as perfectly good doubles, and every
    // subsequent comparison against a NaN is false — so `timeout <= 0` does not
    // reject it, `elapsed >= limit` never becomes true, and the run proceeds
    // with the simulated-time watchdog silently disarmed while `--print-config`
    // still says it is armed. A limit that is not a finite number is not a
    // limit, and "the watchdog is on" has to be true or refused.
    if (!std::isfinite(number)) {
        throw std::runtime_error(
            what + ": '" + value
            + "' is not a finite number. There is no value meaning 'no limit': "
              "a packaging or CI job must not be able to hang.");
    }
    if (number < 0.0) {
        throw std::runtime_error(what + ": '" + value + "' is negative");
    }

    std::string suffix = trim(value.substr(consumed));
    for (char& c : suffix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    double scaled = 0.0;
    if (suffix.empty() || suffix == "ns") {
        scaled = number;
    } else if (suffix == "us") {
        scaled = number * 1e3;
    } else if (suffix == "ms") {
        scaled = number * 1e6;
    } else if (suffix == "s") {
        scaled = number * 1e9;
    } else {
        throw std::runtime_error(what + ": unknown time unit '" + suffix
                                 + "'; use ns, us, ms or s");
    }

    // Checked again after the unit: `1e308 s` is finite on its own and infinite
    // once multiplied, which would re-open exactly the hole closed above.
    if (!std::isfinite(scaled)) {
        throw std::runtime_error(what + ": '" + value
                                 + "' overflows to infinity once the unit is "
                                   "applied");
    }
    return scaled;
}

void apply_key(const std::string& key, const std::string& value,
               const std::string& where, cli_options& options)
{
    if (key == "platform") {
        if (value != "riscv_vpp_compiler_vp") {
            throw std::runtime_error(
                where + ": platform is '" + value
                + "', but this executable is riscv_vpp_compiler_vp. A "
                  "configuration written for another platform will not mean here "
                  "what it meant there.");
        }
    } else if (key == "name") {
        options.config_name = value;
    } else if (key == "ram_size_bytes") {
        options.ram_size = parse_size(value, where + ": ram_size_bytes");
    } else if (key == "hart_id") {
        // Range-checked before the cast, like the command-line path. Without
        // this, `hart_id: 4294967296` in a file became hart 0 -- a run that
        // silently used a different machine than the file described.
        const std::uint64_t hart = parse_count(value, where + ": hart_id");
        if (hart > 0xffffffffull) {
            throw std::runtime_error(where + ": hart_id " + std::to_string(hart)
                                     + " does not fit in mhartid");
        }
        options.hart_id = static_cast<std::uint32_t>(hart);
    } else if (key == "max_instructions") {
        options.max_instructions = parse_count(value, where + ": max_instructions");
    } else if (key == "timeout_ns") {
        options.timeout_ns = parse_time_ns(value, where + ": timeout_ns");
    } else if (key == "wall_timeout_seconds") {
        options.wall_timeout_seconds =
            static_cast<double>(parse_count(value, where + ": wall_timeout_seconds"));
    } else if (key == "trace_limit") {
        options.trace_limit = parse_count(value, where + ": trace_limit");
    } else {
        throw std::runtime_error(
            where + ": unknown key '" + key
            + "'. Accepted keys are: platform, name, ram_size_bytes, hart_id, "
              "max_instructions, timeout_ns, wall_timeout_seconds, "
              "trace_limit.");
    }
}

std::string patch_series_text()
{
    std::ostringstream out;
    bool any = false;
    for (const char* const* record = compiler_vp_build::vpp_patches;
         *record != nullptr; ++record) {
        out << "    " << *record << '\n';
        any = true;
    }
    if (!any) {
        out << "    (none recorded)\n";
    }
    return out.str();
}

} // namespace

cli_options default_options()
{
    cli_options options;
    options.ram_size = COMPILER_VP_RAM_SIZE_DEFAULT;
    options.max_instructions = kDefaultMaxInstructions;
    options.timeout_ns = kDefaultTimeoutNs;
    options.wall_timeout_seconds = kDefaultWallTimeoutSeconds;
    options.trace_limit = 0;
    return options;
}

void load_config_file(const std::string& path, cli_options& options)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open configuration file '" + path + "'");
    }

    std::string line;
    unsigned number = 0;
    while (std::getline(in, line)) {
        ++number;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const std::string content = trim(line);
        if (content.empty()) {
            continue;
        }
        const auto colon = content.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(number)
                                     + ": expected 'key: value'");
        }
        const std::string where = path + ":" + std::to_string(number);
        apply_key(trim(content.substr(0, colon)), trim(content.substr(colon + 1)),
                  where, options);
    }
}

void parse_command_line(int argc, char** argv, cli_options& options)
{
    if (argc > 0 && argv[0] != nullptr) {
        options.program = argv[0];
    }

    // The file is read before the overrides so that an override can fix a file,
    // which means a first pass just to find `--config`.
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--config") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--config needs a path");
            }
            options.config_path = argv[++i];
        }
    }
    if (!options.config_path.empty()) {
        load_config_file(options.config_path, options);
    }

    auto next = [&](int& i, const char* option) -> std::string {
        if (i + 1 >= argc) {
            throw std::runtime_error(std::string(option) + " needs a value");
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else if (argument == "--version") {
            options.show_version = true;
        } else if (argument == "--print-config") {
            options.print_config = true;
        } else if (argument == "--config") {
            ++i; // already consumed
        } else if (argument == "--elf") {
            options.elf_path = next(i, "--elf");
        } else if (argument == "--dump-signature") {
            options.signature_path = next(i, "--dump-signature");
        } else if (argument == "--hart-id") {
            const std::uint64_t value = parse_count(next(i, "--hart-id"), "--hart-id");
            if (value > 0xffffffffull) {
                throw std::runtime_error("--hart-id: " + std::to_string(value)
                                         + " does not fit in mhartid");
            }
            options.hart_id = static_cast<std::uint32_t>(value);
        } else if (argument == "--ram-size") {
            options.ram_size = parse_size(next(i, "--ram-size"), "--ram-size");
        } else if (argument == "--max-instructions") {
            options.max_instructions =
                parse_count(next(i, "--max-instructions"), "--max-instructions");
        } else if (argument == "--timeout") {
            options.timeout_ns = parse_time_ns(next(i, "--timeout"), "--timeout");
        } else if (argument == "--wall-timeout") {
            options.wall_timeout_seconds = static_cast<double>(
                parse_count(next(i, "--wall-timeout"), "--wall-timeout"));
        } else if (argument == "--trace") {
            // Optional argument: `--trace` alone means the default budget,
            // `--trace 5000` raises it. A bare `--trace` immediately before
            // another option must not swallow it.
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options.trace_limit = parse_count(argv[++i], "--trace");
            } else {
                options.trace_limit = kDefaultTraceLimit;
            }
        } else {
            throw std::runtime_error(
                "unknown option '" + argument + "'. Try --help.");
        }
    }

    // ── refusals ─────────────────────────────────────────────────────────────

    if (options.ram_size < 64 * 1024) {
        throw std::runtime_error(
            "--ram-size: " + std::to_string(options.ram_size)
            + " bytes is too small for any image this package can build; the "
              "shipped startup alone reserves a 32 KiB stack. The minimum is "
              "64 KiB.");
    }
    if (options.ram_size > 0x8000'0000ull
        || COMPILER_VP_RAM_BASE + options.ram_size > 0x1'0000'0000ull) {
        throw std::runtime_error(
            "--ram-size: RAM would end above the 4 GiB RV32 address space "
            "(base " + std::to_string(static_cast<std::uint64_t>(COMPILER_VP_RAM_BASE))
            + " + " + std::to_string(options.ram_size) + ")");
    }
    if (options.max_instructions == 0) {
        throw std::runtime_error(
            "--max-instructions: 0 would stop the run before it started. There is "
            "no 'unlimited' value on purpose: a packaging or CI job must not be "
            "able to hang.");
    }
    if (options.timeout_ns <= 0.0) {
        throw std::runtime_error(
            "--timeout: must be positive. There is no 'unlimited' value on "
            "purpose: a packaging or CI job must not be able to hang.");
    }
}

std::string usage_text(const std::string& program)
{
    std::ostringstream out;
    out << "usage: " << program << " --elf <image.elf> [options]\n"
        << "\n"
        << "RISC-V VP++ Compiler Enablement VP: one RV32GCV hart (scalar + RVV\n"
        << "1.0, VLEN=512), TLM program/data RAM and a simulator-only host-I/O\n"
        << "target. No NoC, no accelerators, no second hart.\n"
        << "\n"
        << "  --elf <path>              the RV32 ELF executable to run\n"
        << "  --config <path>           configuration file (key: value)\n"
        << "  --hart-id <n>             architectural hart id, visible as mhartid\n"
        << "  --ram-size <bytes>        RAM size; K/KiB, M/MiB, G/GiB accepted\n"
        << "  --max-instructions <n>    instruction-retired watchdog\n"
        << "  --timeout <time>          simulated-time watchdog; ns/us/ms/s\n"
        << "  --wall-timeout <seconds>  wall-clock backstop for a guest that\n"
        << "                            never yields to the kernel; 0 disables\n"
        << "  --trace [n]               log the first n bus transactions to "
           "stderr\n"
        << "  --dump-signature <path>   write [begin_signature, end_signature)\n"
        << "                            as hex words after the run\n"
        << "  --print-config            report the machine and this "
           "configuration\n"
        << "  --version                 report the machine and the build\n"
        << "  --help                    this text\n"
        << "\n"
        << "Configuration keys: platform, name, ram_size_bytes, hart_id,\n"
        << "max_instructions, timeout_ns, wall_timeout_seconds, trace_limit.\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  the image ran and reported success\n"
        << "  1  the image ran and reported failure, or trapped\n"
        << "  2  usage error\n"
        << "  3  the image was rejected before loading\n"
        << "  4  a watchdog expired\n"
        << "  5  a model or integration defect (a TLM protocol error)\n"
        << "  6  the image's declared bus traffic did not reach the bus\n";
    return out.str();
}

std::string identity_text()
{
    // A hart is constructed to read `vlenb` from its own CSR. It is never run
    // and never bound: reporting VLEN/8 instead would print a number derived
    // from the same constant the line above it already printed, which proves
    // nothing about the machine that would have executed the image.
    cdc::cpu::cpu_config probe_config;
    probe_config.xlen = 32;
    probe_config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu probe("identity_probe", probe_config);

    std::ostringstream out;
    out << "riscv_vpp_compiler_vp (CDC-VP RISC-V VP++ Compiler Enablement VP)\n"
        << "\n"
        << "compiler contract\n"
        << "  architecture    : " << compiler_vp_build::isa_string << '\n'
        << "  ABI             : " << compiler_vp_build::abi_string << '\n'
        << "  XLEN            : 32\n"
        << "  RVV             : 1.0\n"
        << "  VLEN            : " << cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits()
        << " bits\n"
        << "  ELEN            : " << cdc::cpu::riscv_vp_plusplus_cpu::elen_bits()
        << " bits\n"
        << "  vlenb           : " << probe.vlenb() << "  (read from the hart's CSR)\n"
        << "  vector registers: "
        << cdc::cpu::riscv_vp_plusplus_cpu::vector_register_count() << '\n'
        << "  harts           : 1\n"
        << "  execution       : bare-metal / freestanding\n"
        << "\n"
        << "machine\n"
        << "  CPU             : " << probe.backend_name() << '\n'
        << "  accuracy        : functional instruction-set model, "
           "loosely-timed TLM.\n"
        << "                    Not pipeline- or cycle-accurate. It models no "
           "TPU\n"
        << "                    pipeline timing, memory bandwidth or NoC "
           "latency,\n"
        << "                    and no such number may be quoted from it.\n"
        << "  interconnect    : one TLM address decoder. No NoC.\n"
        << "  accelerators    : none. No Sauria, ImageTransform, NEO DMA, core\n"
        << "                    SRAM or NEO fabric is present.\n"
        << "\n"
        << "memory map (simulator-only host I/O; not a TPU_V3 peripheral)\n"
        << "  RAM             : 0x" << std::hex << std::setw(8)
        << std::setfill('0') << static_cast<std::uint64_t>(COMPILER_VP_RAM_BASE)
        << std::dec << std::setfill(' ') << ", size set by --ram-size\n"
        << "  host I/O        : 0x" << std::hex << std::setw(8)
        << std::setfill('0')
        << static_cast<std::uint64_t>(COMPILER_VP_HOSTIO_BASE) << " + "
        << std::dec << std::setfill(' ')
        << static_cast<std::uint64_t>(COMPILER_VP_HOSTIO_SIZE) << " bytes\n"
        << "  everything else : unmapped; an access faults\n"
        << "\n"
        << "build\n"
        << "  build type      : " << compiler_vp_build::build_type << '\n'
        << "  CDC-VP revision : " << compiler_vp_build::git_revision << '\n'
        << "  host compiler   : " << compiler_vp_build::cxx_compiler << '\n'
        << "  SystemC         : " << sc_core::sc_version() << '\n'
        << "  VP++ base       : " << compiler_vp_build::vpp_revision << '\n'
        << "  VP++ patches    :\n"
        << patch_series_text();
    return out.str();
}

std::string configuration_text(const cli_options& options)
{
    std::ostringstream out;
    out << identity_text() << '\n'
        << "this invocation\n"
        << "  configuration   : " << options.config_name;
    if (!options.config_path.empty()) {
        out << "  (" << options.config_path << ')';
    }
    out << '\n'
        << "  image           : "
        << (options.elf_path.empty() ? "(none given)" : options.elf_path) << '\n'
        << "  RAM size        : " << options.ram_size << " bytes\n"
        << "  hart id         : " << options.hart_id << '\n'
        << "  max instructions: " << options.max_instructions << '\n'
        << "  timeout         : " << options.timeout_ns << " ns\n"
        << "  wall backstop   : "
        << (options.wall_timeout_seconds <= 0.0
                ? std::string("off")
                : std::to_string(
                      static_cast<long long>(options.wall_timeout_seconds))
                      + " s")
        << '\n'
        << "  trace           : "
        << (options.trace_limit == 0
                ? std::string("off")
                : std::to_string(options.trace_limit) + " transactions")
        << '\n';
    return out.str();
}

} // namespace cdc::platforms::riscv_vpp_compiler_vp
