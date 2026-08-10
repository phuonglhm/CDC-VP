// SPDX-License-Identifier: Apache-2.0
//
// TPU_V3 SoC top level.
//
// ## What this is at Phase 1
//
// A validated configuration, a SystemC elaboration, and a report. It
// instantiates **no** chips, cores, MXUs, SVMs or NoC yet, and it deliberately
// says so in its own output rather than printing a hierarchy it did not build.
// Its purpose is the Phase 1 gate: prove that the platform configures, that
// the executable really loads the SystemC runtime from beside itself, and that
// the packaged bundle runs with no access to the source or build tree.
//
// It is a real `sc_module` running a real (empty) elaboration rather than a
// plain printer, because a binary that never touches `libsystemc.so` would
// prove nothing about the portable package that is the whole point of the
// gate.
//
// ## What arrives later
//
//   Phase 5   one `tpu_core` (hart + SVM + two MXUs + core-local fabric)
//   Phase 6   two cores behind a chip-local fabric
//   Phase 7   the NoC, global RAM and the boot ROM
//   Phase 8   several chips
//
// Composition belongs here. Component behaviour does not — see plan §11.9.

#pragma once

#include <string>

#include <systemc>

#include "tpu_v3/architecture_config.h"

namespace cdc::platforms::tpu_v3_soc {

class tpu_v3_soc_top : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(tpu_v3_soc_top);

    /// `config` must already be validated. The constructor validates it again
    /// anyway: elaboration is the last point at which a bad value can be
    /// reported as a configuration error instead of as a simulation failure,
    /// and the cost is one call.
    tpu_v3_soc_top(sc_core::sc_module_name name,
                   components::tpu_v3::tpu_soc_config config);

    const components::tpu_v3::tpu_soc_config& config() const noexcept
    {
        return config_;
    }

    /// Human-readable report of what was built and what was not.
    std::string report() const;

    /// True once the SystemC kernel has run `start_of_simulation` and
    /// `end_of_simulation` on this module.
    ///
    /// The Phase 1 gate asserts this. Printing a report is something a program
    /// that never linked SystemC could also do; having the kernel call back
    /// into the module is what actually demonstrates that the runtime beside
    /// the packaged binary was loaded and executed.
    bool simulation_ran() const noexcept { return started_ && finished_; }

private:
    void start_of_simulation() override;
    void end_of_simulation() override;

    components::tpu_v3::tpu_soc_config config_;
    bool started_ = false;
    bool finished_ = false;
};

} // namespace cdc::platforms::tpu_v3_soc
