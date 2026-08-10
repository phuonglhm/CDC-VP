// SPDX-License-Identifier: Apache-2.0
//
// Configuration file and command-line parsing for the TPU_V3 platform.
//
// This lives in the platform, not in `tpu_v3_common`, because plan §11.1 says
// low-level components take a validated object and never read a file. It is
// also why `tpu_v3_common` links no parser and no SystemC.

#pragma once

#include <string>
#include <vector>

#include "tpu_v3/architecture_config.h"

namespace cdc::platforms::tpu_v3_soc {

/// Parse a configuration file.
///
/// The format is a **restricted `key: value` subset**, not YAML. It supports
/// `#` comments, blank lines, and one `key: value` pair per line at column
/// zero. It supports no nesting, no lists, no anchors and no quoting. The
/// `.yaml` extension is for editor highlighting and for consistency with the
/// other platforms; calling it YAML in documentation would be a lie that
/// someone eventually relies on.
///
/// An unknown key is an **error**, not a warning. A typo in `mxu_backend` that
/// was ignored would leave the run using the default while the file says
/// otherwise, and the resulting numbers would be untraceable to their
/// configuration.
///
/// Integer values accept decimal, `0x` hex, and the suffixes `K`/`KiB`,
/// `M`/`MiB`, `G`/`GiB` (binary multipliers).
///
/// Throws `std::runtime_error` for I/O and syntax problems, and
/// `std::invalid_argument` for a value the architecture refuses. The returned
/// configuration is **not** validated; the caller applies command-line
/// overrides first and validates once, so that an override can fix a file.
components::tpu_v3::tpu_soc_config load_config_file(const std::string& path);

/// Result of parsing the command line.
struct cli_options {
    std::string config_path;
    bool print_address_map = false;
    bool show_help = false;
    bool show_version = false;
    /// Simulated time limit; 0 means "run until nothing is left to do".
    double time_limit_ns = 0.0;
};

/// Parse `argv`, applying overrides on top of `config`.
///
/// Overrides are applied in command-line order and after the file is read, so
/// `--config a.yaml --chips 4` means what it looks like it means.
///
/// Throws `std::runtime_error` on an unknown or malformed option.
cli_options parse_command_line(int argc, char** argv,
                               components::tpu_v3::tpu_soc_config& config);

/// Usage text, including every accepted configuration key.
std::string usage_text(const std::string& program);

} // namespace cdc::platforms::tpu_v3_soc
