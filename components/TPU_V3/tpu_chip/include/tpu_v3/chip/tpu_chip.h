// SPDX-License-Identifier: Apache-2.0
//
// One TPU chip: two NEO-COREs, the chip-local fabric, the chip's own register
// windows, and exactly one boundary to the mesh (plan §11.9,
// `ARCHITECTURE.md` §1).
//
//   tpu_chip
//   ├── tpu_core[0] ──┐                        ┌── chip_control
//   ├── tpu_core[1] ──┼── chip_local_fabric ───┼── chip_counters
//   └── shared bus lock                        └── external  <- the one NoC
//                                                              manager
//
// Phase 7 built a NEO-CORE that works on its own. What this adds is everything
// that only exists once there are two of them, and each piece is here because
// the single-core case could not have shown it:
//
// **Unique hart ids.** `mhartid = chip * 2 + core` (plan §11.8). Firmware reads
// it to decide what it is; two harts reporting the same value would make every
// workload-partitioning decision wrong in a way that looks like a firmware bug.
//
// **One aggregated NoC identity.** Plan §4.4 forbids exposing the harts, DMAs
// and engines as unrelated NoC managers. The cores' external ports terminate on
// the chip fabric and only the fabric's `external` leaves — which is also what
// keeps the system inside the 8-initiator FlooNoC limit (D2).
//
// **Local containment across the chip.** A core addressing its sibling is
// answered by the fabric and never offered to the mesh. That is required, not
// an optimisation: `noc_interconnect` runs with `NoLoopback = 1` and refuses a
// target on a node that hosts an upstream port (D1).
//
// **One bus lock.** `lr`/`sc` and AMO exclude harts through a lock the CPU
// backend holds, and a per-hart lock excludes nobody. The chip creates one and
// attaches it to both harts, which is what closes the multi-hart atomicity
// precondition decision record D8 attached to upstream `52d376d4`. The evidence
// is `test_bus_lock_atomicity`, whose unshared control loses exactly half its
// updates.
//
// ## What this component does not do
//
// No chip-level interrupt controller, no chip-level DMA, no cross-chip
// coherence. Each core aggregates its own engines' interrupts and delivers them
// to its own hart (`ARCHITECTURE.md` §5); the chip does not see them. Revision 1
// has no cross-chip atomics either, so the bus lock's scope is the chip because
// that is the extent of the shared address space, not because a chip is a
// natural unit for one.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <systemc>
#include <tlm>

#include "riscv_vp_plusplus_wrapper.h"

#include "tpu_v3/address_map.h"
#include "tpu_v3/chip/chip_local_fabric.h"
#include "tpu_v3/core/core_registers.h"
#include "tpu_v3/core/tpu_core.h"
#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3::chip {

struct tpu_chip_config {
    /// Position in the SoC. This decides every absolute address in the chip
    /// and, with the core index, every hart id.
    chip_id_t chip = 0;

    /// Frozen at two (plan §4.1, D14). Present so a configuration that asks
    /// for a different number is **refused** rather than silently ignored: a
    /// field that is validated is a field somebody can be wrong about, and a
    /// count that is merely assumed is one nobody can be told about.
    unsigned cores = cores_per_chip;

    /// Per-core settings.
    ///
    /// `tpu_core_config::chip` and `::core` are **assigned by the chip** and
    /// must be left at zero here. Identity has one source — there is no second
    /// field, because two sources of the same fact can disagree, and a chip
    /// whose `core[1]` announced itself as core 0 would produce two harts with
    /// the same `mhartid` and an address map that overlaps itself.
    std::array<core::tpu_core_config, cores_per_chip> core;

    /// D16 applies here for the same reason it applies inside a core: a
    /// blocking fabric on the path from the detailed NoC stalls the one
    /// process that advances the mesh.
    chip_fabric_timing timing = chip_fabric_timing::annotated;

    /// The chip fabric's clock period. Separate from the core clock: the cores
    /// are their own domains (`ARCHITECTURE.md` §6) and the fabric between them
    /// need not run at either one's rate.
    sc_core::sc_time cycle{10, sc_core::SC_NS};

    /// Throws `std::invalid_argument` naming the field, the value and the
    /// accepted range. Called before any socket is bound.
    void validate(const std::string& context) const;
};

class tpu_chip : public sc_core::sc_module {
public:
    tpu_chip(sc_core::sc_module_name name, tpu_chip_config config);

    // ── the mesh-facing boundary ─────────────────────────────────────────────
    //
    // Exactly two sockets for the whole chip, and that is the point.

    /// Outbound: this chip towards global memory or the mesh.
    tlm_utils::simple_initiator_socket<chip_local_fabric>& external() noexcept
    {
        return fabric_.external;
    }
    /// Inbound: a remote chip or the host loader.
    tlm_utils::simple_target_socket_tagged<chip_local_fabric>&
    inbound() noexcept
    {
        return fabric_.from[static_cast<unsigned>(
            chip_initiator::external_inbound)];
    }

    // ── identity ─────────────────────────────────────────────────────────────

    chip_id_t chip_id() const noexcept { return config_.chip; }
    std::uint64_t chip_base() const noexcept
    {
        return address_map::chip_base(config_.chip);
    }
    /// `chip * 2 + core`, as firmware reads it from `mhartid`.
    hart_id_t hart_id(unsigned core) const;

    const tpu_chip_config& config() const noexcept { return config_; }

    // ── the parts, for tests and platform reporting ──────────────────────────

    core::tpu_core& core(unsigned index);
    const core::tpu_core& core(unsigned index) const;
    chip_local_fabric& fabric() noexcept { return fabric_; }
    core::mmio_register_file& control_registers() noexcept
    {
        return control_regs_;
    }
    core::mmio_register_file& counter_registers() noexcept
    {
        return counter_regs_;
    }

    /// Harts sharing this chip's LR/SC and AMO lock. Two, always — exposed so
    /// a gate can state that it is testing one lock rather than two that never
    /// met.
    unsigned bus_lock_sharers() const;

    /// Hierarchical reset: chip → core → component (`ARCHITECTURE.md` §6).
    ///
    /// Consumes **no simulated time**, for the reason `tpu_core::reset()`
    /// documents at length: a reset that yields lets something observe a
    /// half-reset chip or repopulate one before the call returns. Both cores go
    /// through their own reset, then the fabric between them and the chip's
    /// register windows.
    ///
    /// One limitation, recorded rather than hidden. A hart parked in the shared
    /// bus lock's wait — because the *other* hart holds it — resumes there when
    /// the lock frees and finishes the instruction it was in the middle of,
    /// after the reset. It can write one register in doing so. This is the same
    /// root cause as the D19 `wfi` limitation: a reset cannot unwind a SystemC
    /// process's C++ stack, and the hart is suspended inside one. Resetting the
    /// holder is what frees the lock, so the waiter does not hang.
    void reset();

    std::string report() const;

private:
    tpu_chip_config config_;

    // Declaration order is construction order.
    chip_local_fabric fabric_;
    core::mmio_register_file control_regs_;
    core::mmio_register_file counter_regs_;

    /// One lock for both harts, created before the cores so it exists when
    /// they are attached to it.
    std::shared_ptr<cdc::cpu::shared_bus_lock> bus_lock_;

    /// `tpu_core` takes its configuration by value and is not default
    /// constructible, so the cores are created in the constructor body rather
    /// than as plain members. They are still children of this module: SystemC
    /// hierarchy follows the construction context, which is this constructor.
    std::unique_ptr<core::tpu_core> cores_[cores_per_chip];
};

} // namespace cdc::components::tpu_v3::chip
