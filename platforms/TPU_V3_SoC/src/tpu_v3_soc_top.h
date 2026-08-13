// SPDX-License-Identifier: Apache-2.0
//
// TPU_V3 SoC top level.
//
// ## What this is at Phase 3
//
// A validated configuration, a SystemC elaboration, one **core SRAM per
// NEO-CORE**, the global RAM store, and a report. It instantiates no hart, no
// matrix engine, no DMA, no ImageTransform engine and no NoC yet, and it says
// so in its own output rather than printing a hierarchy it did not build.
//
// The memories are here and the fabrics are not, and that split is deliberate.
// Phase 3's gate asks for one platform-level property — "`mesh_4x4` logical
// memory elaborates without eager host commitment" — and that is a property of
// the *storage*: sixteen 16 MiB core SRAMs plus a 1 GiB global RAM is 1.25 GiB
// of address space that must cost a few megabytes of host memory until
// firmware touches it (decision record D6). Instantiating the fabrics too
// would mean binding every socket to a stub that no phase asked for; the
// fabrics are proved in `components/TPU_V3/tpu_core/tests` and composed into a
// `tpu_core` in Phase 7, which is where they acquire a hart to be driven by.
//
// It is a real `sc_module` running a real (empty) elaboration rather than a
// plain printer, because a binary that never touches `libsystemc.so` would
// prove nothing about the portable package that is half the point of the
// packaging gate.
//
// ## What arrives later
//
//   Phase 4   the independent NEO DMA
//   Phase 5   the Sauria 64x64 matrix engine
//   Phase 6   the ImageTransform engine
//   Phase 7   one `tpu_core`: hart + SRAM + engines behind the D15 fabrics
//   Phase 8   two cores per chip, several chips
//   Phase 9   the NoC, global RAM as a target, and the boot ROM
//
// Composition belongs here. Component behaviour does not — see plan §11.11.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>

#include "tpu_v3/architecture_config.h"
#include "tpu_v3/sparse_memory.h"
#include "tpu_v3/sram/core_sram.h"

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

    /// Address space the configuration describes: every core SRAM plus global
    /// RAM.
    std::uint64_t logical_memory_bytes() const noexcept;

    /// Host memory actually committed to backing it right now. The Phase 3
    /// gate is the gap between this and the number above.
    std::uint64_t allocated_backing_bytes() const noexcept;

    std::size_t allocated_page_count() const noexcept;

    /// Core SRAM for one NEO-CORE. `chip` and `core` must be in range.
    components::tpu_v3::sram::core_sram& core_sram(
        components::tpu_v3::chip_id_t chip,
        components::tpu_v3::core_id_t core);

    /// True once the SystemC kernel has run `start_of_simulation` and
    /// `end_of_simulation` on this module.
    ///
    /// The packaging gate asserts this. Printing a report is something a
    /// program that never linked SystemC could also do; having the kernel call
    /// back into the module is what actually demonstrates that the runtime
    /// beside the packaged binary was loaded and executed.
    bool simulation_ran() const noexcept { return started_ && finished_; }

private:
    void start_of_simulation() override;
    void end_of_simulation() override;

    components::tpu_v3::tpu_soc_config config_;

    /// One per NEO-CORE, in `chip * cores_per_chip + core` order. Held by
    /// pointer because an `sc_module` is neither copyable nor movable and the
    /// count is a runtime value.
    std::vector<std::unique_ptr<components::tpu_v3::sram::core_sram>> core_srams_;

    /// Global RAM backing.
    ///
    /// Storage without a socket, because nothing is connected to it before
    /// Phase 9 and a target whose sockets dangle does not elaborate. What it
    /// is here for is the D6 property: a 1 GiB window has to cost its page
    /// index and nothing else until something writes to it.
    components::tpu_v3::sparse_memory global_ram_;

    bool started_ = false;
    bool finished_ = false;
};

} // namespace cdc::platforms::tpu_v3_soc
