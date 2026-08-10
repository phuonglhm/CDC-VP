// SPDX-License-Identifier: Apache-2.0
//
// `tpu_v3_soc` entry point.
//
// Phase 1 behaviour: read a configuration, validate it, elaborate the SystemC
// top level, run an empty simulation, and report. See `tpu_v3_soc_top.h` for
// what is deliberately not built yet.

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include <systemc>

#include "config_loader.h"
#include "tpu_v3_soc_top.h"

#include "tpu_v3/architecture_config.h"

namespace {

// Recorded by CMake so a result can always be traced to the build that made
// it. A latency figure from a Debug build means something different from the
// same figure in Release, and "which revision was that?" is not a question to
// answer from memory.
#ifndef TPU_V3_SOC_BUILD_TYPE
#define TPU_V3_SOC_BUILD_TYPE "unspecified"
#endif
#ifndef TPU_V3_SOC_GIT_REVISION
#define TPU_V3_SOC_GIT_REVISION "unknown"
#endif
#ifndef TPU_V3_SOC_COMPILER
#define TPU_V3_SOC_COMPILER "unknown"
#endif
#ifndef TPU_V3_SOC_MXU_BACKEND
#define TPU_V3_SOC_MXU_BACKEND "fast"
#endif

void print_version()
{
    std::cout << "tpu_v3_soc (CDC-VP TPU_V3 platform)\n"
              << "  phase           : 1 (skeleton and packaging)\n"
              << "  build type      : " << TPU_V3_SOC_BUILD_TYPE << '\n'
              << "  CDC-VP revision : " << TPU_V3_SOC_GIT_REVISION << '\n'
              << "  compiler        : " << TPU_V3_SOC_COMPILER << '\n'
              << "  MXU backend     : " << TPU_V3_SOC_MXU_BACKEND << '\n'
              << "  SystemC         : " << sc_core::sc_version() << '\n';
}

/// The MXU backend is chosen when the platform is *compiled*
/// (`-DTPU_V3_MXU_BACKEND=...`), not when it is run. A configuration file
/// asking for a different one must fail rather than run the compiled backend
/// under the other name: the package manifest records the compiled value, so
/// silently honouring the file would make the manifest describe a run it did
/// not describe.
bool backend_matches_this_build(cdc::components::tpu_v3::mxu_backend requested)
{
    return std::string(cdc::components::tpu_v3::to_string(requested))
        == std::string(TPU_V3_SOC_MXU_BACKEND);
}

} // namespace

int sc_main(int argc, char* argv[])
{
    namespace platform = cdc::platforms::tpu_v3_soc;
    namespace tpu = cdc::components::tpu_v3;

    const std::string program = argc > 0 ? argv[0] : "tpu_v3_soc";

    tpu::tpu_soc_config config;
    platform::cli_options options;

    // Configuration errors are reported as messages and a non-zero exit, not
    // as an SC_REPORT_FATAL: they are the user's problem to fix, and a SystemC
    // stack trace obscures rather than explains them.
    try {
        options = platform::parse_command_line(argc, argv, config);
    } catch (const std::exception& error) {
        std::cerr << "tpu_v3_soc: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    if (options.show_help) {
        std::cout << platform::usage_text(program);
        return EXIT_SUCCESS;
    }
    if (options.show_version) {
        print_version();
        return EXIT_SUCCESS;
    }

    // Checked before `validate()`, and per core rather than through core 0:
    // this is the platform's own rule and it must not depend on the component
    // validator having run first.
    for (const auto& core : config.chip.core) {
        if (backend_matches_this_build(core.mxu.backend)) {
            continue;
        }
        std::cerr << "tpu_v3_soc: the configuration selects MXU backend '"
                  << tpu::to_string(core.mxu.backend)
                  << "', but this executable was built with "
                     "TPU_V3_MXU_BACKEND=" TPU_V3_SOC_MXU_BACKEND
                     ". Reconfigure the build, or select '"
                     TPU_V3_SOC_MXU_BACKEND "' in the configuration.\n";
        return EXIT_FAILURE;
    }

    try {
        config.validate();
    } catch (const std::exception& error) {
        std::cerr << "tpu_v3_soc: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    if (options.print_address_map) {
        std::cout << tpu::describe_address_map(config);
        return EXIT_SUCCESS;
    }

    try {
        platform::tpu_v3_soc_top top("tpu_v3_soc", config);

        std::cout << top.report();

        if (options.time_limit_ns > 0.0) {
            sc_core::sc_start(
                sc_core::sc_time(options.time_limit_ns, sc_core::SC_NS));
        } else {
            sc_core::sc_start();
        }

        if (!sc_core::sc_end_of_simulation_invoked()) {
            sc_core::sc_stop();
        }

        if (!top.simulation_ran()) {
            std::cerr << "tpu_v3_soc: the SystemC kernel did not run the "
                         "elaboration callbacks\n";
            return EXIT_FAILURE;
        }

        std::cout << "\nsimulated time : " << sc_core::sc_time_stamp() << '\n'
                  << "status         : OK\n";
    } catch (const std::exception& error) {
        std::cerr << "tpu_v3_soc: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
