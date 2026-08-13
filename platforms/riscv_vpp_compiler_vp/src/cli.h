// SPDX-License-Identifier: Apache-2.0
//
// Command line and configuration file for `riscv_vpp_compiler_vp`.
//
// The parser lives in the platform, like the TPU_V3 one and for the same
// reason: a component takes a validated object and never reads a file.
//
// Every option refuses a value it does not understand. Phase 4.5 says so in one
// line — "Unsupported values fail instead of silently falling back" — and it is
// the single most important property of a handoff tool. A compiler team
// debugging a codegen difference cannot afford a simulator that quietly ran
// something other than what the command line asked for.

#pragma once

#include <cstdint>
#include <string>

namespace cdc::platforms::riscv_vpp_compiler_vp {

struct cli_options {
    std::string program = "riscv_vpp_compiler_vp";
    std::string config_name = "built-in defaults";

    std::string elf_path;
    std::string config_path;
    std::string signature_path;

    bool show_help = false;
    bool show_version = false;
    bool print_config = false;

    std::uint32_t hart_id = 0;
    std::uint64_t ram_size = 0;

    // ── watchdogs ────────────────────────────────────────────────────────────
    //
    // Both are always armed. Phase 4.5 requires that "a non-terminating guest
    // must fail deterministically rather than hang the packaging or CI job",
    // and a watchdog that is off by default protects nobody: the run that hangs
    // is the one nobody expected to.
    //
    // Two of them, because they fail for different reasons. A program stuck in
    // a tight spin retires instructions for ever and trips the instruction
    // bound; one blocked on an event that never arrives retires nothing while
    // simulated time advances, and only the time bound catches it.
    std::uint64_t max_instructions = 0;
    double timeout_ns = 0.0;

    /// Wall-clock backstop, in seconds. Zero disables it.
    ///
    /// Not a third way of saying the same thing. Both bounds above are polled
    /// between slices of `sc_start()`, so both are useless against a guest that
    /// never yields to the SystemC kernel — and that is a guest an ordinary
    /// linker mistake produces, not an exotic one. This is the only limit that
    /// can end such a run, so it defaults to on.
    ///
    /// It is explicitly a *wall-clock* bound. When it fires depends on the
    /// machine, so nothing may be concluded from it beyond "this did not
    /// finish".
    double wall_timeout_seconds = 0.0;

    /// Transactions `--trace` prints before it stops. Zero disables tracing.
    std::uint64_t trace_limit = 0;
};

/// Defaults before any file or command-line option is applied.
cli_options default_options();

/// Apply a configuration file to `options`.
///
/// The format is the same restricted `key: value` subset the TPU_V3 platform
/// uses — `#` comments, blank lines, one pair per line at column zero, no
/// nesting, no lists, no quoting. The `.yaml` extension is for editor
/// highlighting; calling it YAML in the documentation would be a lie someone
/// eventually relies on.
///
/// An unknown key is an error. A typo in `max_instructions` that was ignored
/// would leave the run using the default while the file says otherwise.
void load_config_file(const std::string& path, cli_options& options);

/// Parse `argv` on top of `options`. Command-line options are applied after the
/// file, in command-line order, so `--config a.yaml --hart-id 3` means what it
/// looks like it means.
///
/// Throws `std::runtime_error` for an unknown option, a missing argument or a
/// value the platform refuses.
void parse_command_line(int argc, char** argv, cli_options& options);

std::string usage_text(const std::string& program);

/// Everything the plan requires `--version` and `--print-config` to report:
/// ISA, ABI, VLEN, ELEN, `vlenb`, hart count, accuracy status, memory map, host
/// toolchain and SystemC, the CDC-VP revision, the VP++ base revision and every
/// effective patch with its hash.
///
/// `vlenb` comes from a constructed hart's own CSR rather than from VLEN/8, so
/// the value printed is the value firmware would read.
std::string identity_text();

/// `identity_text()` plus the configuration this invocation would run with.
std::string configuration_text(const cli_options& options);

} // namespace cdc::platforms::riscv_vpp_compiler_vp
