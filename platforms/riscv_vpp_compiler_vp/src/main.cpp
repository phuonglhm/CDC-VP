// SPDX-License-Identifier: Apache-2.0
//
// `riscv_vpp_compiler_vp` entry point.
//
// Reads a configuration, validates the image against the platform map,
// elaborates one RV32GCV hart with RAM and the simulator-only host-I/O target,
// runs it under two watchdogs, and reports.
//
// ## The run loop, and why it is a loop
//
// The ISS executes inside its own SC_THREAD, so nothing outside can interrupt
// it. `sc_start()` with the full time limit would honour the simulated-time
// watchdog and nothing else: a program spinning in a tight loop retires
// instructions for ever while simulated time crawls, and the run would sit
// there until the CI job's own timeout killed it — which is the failure mode
// Phase 4.5 asks to be made deterministic.
//
// So time is advanced in slices and both bounds are checked between them. The
// simulated-time bound is exact, because the last slice is clipped to land on
// it. The instruction bound is checked at slice boundaries, so a run may retire
// up to one slice's worth of instructions past it before stopping; the report
// prints the true count rather than the limit, so the overshoot is visible
// rather than hidden. Making it exact would mean polling every quantum, which
// costs a `sc_start` call per microsecond of simulated time — about a thousand
// times more kernel entries for a bound whose purpose is "stop eventually".
//
// The slice is derived from the time limit rather than fixed, so the number of
// polls is roughly constant whatever the limit: a 1 ms run is not polled once
// and a 10 s run is not polled ten thousand times.

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <systemc>

#include "riscv_vp_plusplus_wrapper.h"

#include "cli.h"
#include "compiler_vp/host_io_map.h"
#include "elf_image.h"
#include "platform_top.h"

namespace {

namespace platform = cdc::platforms::riscv_vpp_compiler_vp;

/// Host exit codes. Distinct on purpose: a CI job that only knows "non-zero"
/// cannot tell a guest that failed its own checks from a simulator that refused
/// the image, and those two send whoever reads the log to different places.
enum exit_code {
    exit_pass = 0,
    exit_guest_failed = 1,
    exit_usage = 2,
    exit_image_rejected = 3,
    exit_watchdog = 4,
    exit_model_defect = 5,
    exit_traffic_missing = 6
};

/// The backstop: a host thread that ends the process if the run outlasts a wall
/// clock budget.
///
/// The two simulated watchdogs are the ones that describe the *work* a run may
/// do, and they are what a user should reach for. Neither can help when the
/// guest stops letting the SystemC kernel run at all, because both are polled
/// from outside `sc_start()` and `sc_start()` never returns. `guest_fault_loop`
/// catches the one shape of that failure this platform can recognise from the
/// bus; this catches the rest, including shapes nobody has thought of yet.
///
/// Phase 4.5 asks that a non-terminating guest "fail deterministically rather
/// than hang the packaging or CI job". The deterministic part is the two
/// simulated bounds and the fault-loop detector. This is the "rather than hang
/// the CI job" part, and it is explicitly a wall-clock bound: *when* it fires
/// depends on the machine, so nothing may be concluded from the fact that it
/// fired beyond "this did not finish".
class wall_clock_backstop {
public:
    explicit wall_clock_backstop(double seconds)
    {
        if (seconds <= 0.0) {
            return;
        }
        thread_ = std::thread([this, seconds] {
            std::unique_lock<std::mutex> lock(mutex_);
            const auto deadline = std::chrono::steady_clock::now()
                                  + std::chrono::duration<double>(seconds);
            if (finished_.wait_until(lock, deadline,
                                     [this] { return done_; })) {
                return;
            }
            std::cerr << "\nriscv_vpp_compiler_vp: the run exceeded its "
                      << seconds
                      << " s wall-clock backstop and was stopped. The guest is "
                         "not letting the simulation kernel run, so neither the "
                         "instruction nor the simulated-time watchdog could "
                         "fire. Raise --wall-timeout if the image is genuinely "
                         "this large.\n";
            std::cerr.flush();
            std::cout.flush();
            // `_Exit`, not `exit`: static destructors would run on this thread
            // while the ISS thread is still inside the kernel, and the crash
            // that produces would replace a clear diagnostic with a confusing
            // one.
            std::_Exit(exit_watchdog);
        });
    }

    ~wall_clock_backstop()
    {
        if (!thread_.joinable()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
        }
        finished_.notify_all();
        thread_.join();
    }

    wall_clock_backstop(const wall_clock_backstop&) = delete;
    wall_clock_backstop& operator=(const wall_clock_backstop&) = delete;

private:
    std::mutex mutex_;
    std::condition_variable finished_;
    bool done_ = false;
    std::thread thread_;
};

sc_core::sc_time watchdog_slice(const sc_core::sc_time& limit)
{
    // Aim for about a thousand polls over the whole run, clamped so the slice
    // is never smaller than the ISS's TLM quantum (1 us — polling faster than
    // the quantum cannot see anything new) and never larger than a millisecond.
    const double target_ns = limit.to_seconds() * 1e9 / 1024.0;
    const double clamped = std::min(std::max(target_ns, 1000.0), 1'000'000.0);
    return sc_core::sc_time(clamped, sc_core::SC_NS);
}

bool write_signature(const platform::elf_image& image, platform::compiler_vp_top& top,
                     const std::string& path)
{
    const auto begin = image.symbols.find("begin_signature");
    const auto end = image.symbols.find("end_signature");
    const std::uint64_t from = begin->second.value;
    const std::uint64_t to = end->second.value;

    std::vector<unsigned char> bytes(static_cast<std::size_t>(to - from));
    if (!top.debug_read(from, bytes.data(), bytes.size())) {
        std::cerr << "riscv_vpp_compiler_vp: the signature range [0x" << std::hex
                  << from << ", 0x" << to << std::dec
                  << ") is not inside RAM\n";
        return false;
    }

    std::ofstream out(path);
    if (!out) {
        std::cerr << "riscv_vpp_compiler_vp: cannot write signature to '" << path
                  << "'\n";
        return false;
    }
    // One 32-bit word per line, lowest address first, lower-case hex — the
    // riscv-arch-test convention, so an existing comparator works unchanged.
    for (std::size_t i = 0; i + 4 <= bytes.size(); i += 4) {
        const std::uint32_t word = static_cast<std::uint32_t>(bytes[i])
                                   | (static_cast<std::uint32_t>(bytes[i + 1]) << 8)
                                   | (static_cast<std::uint32_t>(bytes[i + 2]) << 16)
                                   | (static_cast<std::uint32_t>(bytes[i + 3]) << 24);
        out << std::hex << std::setw(8) << std::setfill('0') << word << '\n';
    }
    return static_cast<bool>(out);
}

} // namespace

int sc_main(int argc, char* argv[])
{
    platform::cli_options options = platform::default_options();

    try {
        platform::parse_command_line(argc, argv, options);
    } catch (const std::exception& error) {
        std::cerr << "riscv_vpp_compiler_vp: " << error.what() << '\n';
        return exit_usage;
    }

    if (options.show_help) {
        std::cout << platform::usage_text(options.program);
        return exit_pass;
    }
    if (options.show_version) {
        std::cout << platform::identity_text();
        return exit_pass;
    }
    if (options.print_config) {
        std::cout << platform::configuration_text(options);
        return exit_pass;
    }

    if (options.elf_path.empty()) {
        std::cerr << "riscv_vpp_compiler_vp: no image. Pass --elf <image.elf>, "
                     "or --help.\n";
        return exit_usage;
    }

    platform::platform_config config;
    config.ram = {COMPILER_VP_RAM_BASE, options.ram_size, "RAM"};
    config.host_io = {COMPILER_VP_HOSTIO_BASE, COMPILER_VP_HOSTIO_SIZE, "host I/O"};
    config.hart_id = options.hart_id;
    config.trace_limit = options.trace_limit;

    platform::elf_image image;
    try {
        image = platform::read_and_validate_elf(options.elf_path, config.ram,
                                                config.host_io);
    } catch (const std::exception& error) {
        std::cerr << "riscv_vpp_compiler_vp: " << error.what() << '\n';
        return exit_image_rejected;
    }

    // Checked before elaboration, not after the run: a signature request an
    // image cannot satisfy is a mistake worth making immediately, and finding
    // out afterwards throws away the run that just succeeded.
    if (!options.signature_path.empty()) {
        const bool has_begin = image.symbols.count("begin_signature") != 0;
        const bool has_end = image.symbols.count("end_signature") != 0;
        if (!has_begin || !has_end) {
            std::cerr << "riscv_vpp_compiler_vp: --dump-signature needs the "
                         "symbols begin_signature and end_signature, and '"
                      << options.elf_path << "' defines "
                      << (has_begin ? "only begin_signature"
                                    : (has_end ? "only end_signature" : "neither"))
                      << ". Link them around the block to be dumped, or drop the "
                         "option.\n";
            return exit_usage;
        }
        if (image.symbols["end_signature"].value
            < image.symbols["begin_signature"].value) {
            std::cerr << "riscv_vpp_compiler_vp: end_signature is below "
                         "begin_signature in '"
                      << options.elf_path << "'\n";
            return exit_usage;
        }
    }

    std::cout << "riscv_vpp_compiler_vp: one RV32GCV hart, "
              << (options.ram_size % (1024 * 1024) == 0
                      ? std::to_string(options.ram_size / (1024 * 1024)) + " MiB"
                      : std::to_string(options.ram_size / 1024) + " KiB")
              << " RAM, hart id " << options.hart_id << '\n'
              << "  image         : " << options.elf_path << '\n'
              << "  entry         : 0x" << std::hex << std::setw(8)
              << std::setfill('0') << image.entry << std::dec << std::setfill(' ')
              << '\n'
              << "  declared ISA  : "
              << (image.architecture.empty()
                      ? std::string("(no .riscv.attributes section)")
                      : image.architecture)
              << '\n'
              << "  loadable      : " << image.segments.size() << " segments, "
              << image.loaded_bytes() << " bytes\n"
              << std::endl;

    bool time_expired = false;
    bool instructions_expired = false;
    int result = exit_pass;

    const wall_clock_backstop backstop(options.wall_timeout_seconds);

    // Declared outside the `try` so the catch below can still ask it what
    // happened. SystemC rethrows anything escaping a process as an `sc_report`,
    // so the exception *type* does not survive; the object that set the flag
    // has to.
    std::unique_ptr<platform::compiler_vp_top> owner;

    try {
        owner = std::make_unique<platform::compiler_vp_top>("compiler_vp",
                                                            config, image);
        platform::compiler_vp_top& top = *owner;

        const sc_core::sc_time limit(options.timeout_ns, sc_core::SC_NS);
        const sc_core::sc_time slice = watchdog_slice(limit);

        while (true) {
            const sc_core::sc_time elapsed = sc_core::sc_time_stamp();
            if (elapsed >= limit) {
                time_expired = true;
                break;
            }
            sc_core::sc_start(std::min(slice, limit - elapsed));

            if (sc_core::sc_end_of_simulation_invoked()) {
                break;
            }
            if (top.cpu().get_instret() >= options.max_instructions) {
                instructions_expired = true;
                break;
            }
        }

        if (!sc_core::sc_end_of_simulation_invoked()) {
            sc_core::sc_stop();
        }
        top.flush_console();

        const auto& host = top.host_io();
        const auto& bus = top.bus();

        const std::uint64_t retired = top.cpu().get_instret();

        std::cout << '\n'
                  << top.traffic_report() << "run\n"
                  << "  instructions  : " << retired << '\n'
                  // One fetch per retired instruction is what "no decode cache
                  // and no DMI" looks like from outside: the moment either is
                  // enabled, the ISS stops asking the bus for instructions it
                  // has already seen and this ratio collapses. Printed rather
                  // than only asserted in a test, because it is the line that
                  // tells a reader of one log that the claim held for that run.
                  << "  fetches on TLM: " << bus.fetch.reads;
        if (retired != 0) {
            std::cout << "  (" << std::fixed << std::setprecision(2)
                      << (static_cast<double>(bus.fetch.reads)
                          / static_cast<double>(retired))
                      << " per retired instruction)";
            std::cout.unsetf(std::ios::floatfield);
        }
        std::cout << '\n'
                  << "  simulated time: " << sc_core::sc_time_stamp() << '\n'
                  << "  console bytes : " << host.console_bytes << '\n';

        if (bus.window_violated) {
            std::cout << "status         : FAIL (declared bus traffic missing)\n";
            result = exit_traffic_missing;
        } else if (time_expired || instructions_expired) {
            std::cout << "status         : FAIL (watchdog: "
                      << (instructions_expired ? "instruction limit "
                                               : "simulated-time limit ")
                      << (instructions_expired
                              ? std::to_string(options.max_instructions)
                              : std::to_string(options.timeout_ns) + " ns")
                      << " reached; the image never signalled exit)\n";
            result = exit_watchdog;
        } else if (!host.exited) {
            // The ISS terminated on its own, or every process ran out of work.
            std::cout << "status         : FAIL (the image stopped without "
                         "writing the exit register)\n";
            result = exit_guest_failed;
        } else if (host.exit_kind != COMPILER_VP_EXIT_KIND_NORMAL) {
            std::cout << "status         : FAIL (trapped, mcause 0x" << std::hex
                      << host.exit_mcause << ", mepc 0x" << host.exit_mepc
                      << std::dec << ")\n";
            result = exit_guest_failed;
        } else if (host.exit_status != COMPILER_VP_EXIT_PASS) {
            std::cout << "status         : FAIL (the image reported check "
                      << host.exit_status << ")\n";
            result = exit_guest_failed;
        } else {
            std::cout << "status         : PASS\n";
        }

        if (!options.signature_path.empty()) {
            if (!write_signature(image, top, options.signature_path)) {
                result = (result == exit_pass) ? exit_usage : result;
            } else {
                std::cout << "signature      : " << options.signature_path << '\n';
            }
        }
    } catch (const std::exception& error) {
        // A watchdog result, not a model defect, when the decoder is the one
        // that raised it: the guest is broken, the platform noticed, and the
        // exit code has to say "this did not terminate" rather than "the
        // simulator is wrong". It arrives as an exception only because that is
        // the one way out of a SystemC process that will not yield, and it is
        // recognised by the flag rather than by its type because SystemC
        // rewrote the type on the way out.
        if (owner && owner->bus().fault_loop_detected) {
            owner->flush_console();
            std::cout << std::endl;
            std::cerr << "riscv_vpp_compiler_vp: "
                      << owner->bus().fault_loop_message << '\n';
            std::cout << "status         : FAIL (watchdog: the guest stopped "
                         "making progress)\n";
            return exit_watchdog;
        }

        // A TLM protocol error, or a refusal from the backend. Decision record
        // D13 draws the line: a target that legitimately refused an access is a
        // guest fault and never reaches here, while a malformed transaction is
        // a defect in the model or its integration and must not be dressed up
        // as something the program did.
        std::cerr << "\nriscv_vpp_compiler_vp: " << error.what() << '\n'
                  << "This is a model or integration defect, not a guest "
                     "fault.\n";
        return exit_model_defect;
    }

    return result;
}
