// SPDX-License-Identifier: Apache-2.0
//
// Strongly typed TPU_V3 configuration (plan §11.1).
//
// Two rules shape this file:
//
//  * **No YAML, no CLI, no SystemC here.** Low-level components take a
//    validated object; parsing belongs to the platform. That is what keeps a
//    component unit-testable without a config file and keeps a config format
//    change out of the components.
//  * **Frozen values are rejected, not adapted.** `validate()` throws on
//    `cores != 2`, `rows != 128` and the rest, with a message naming the field
//    and the accepted value. A model that quietly ran with a 64x64 MXU because
//    someone typed 64 would produce numbers indistinguishable from real ones.

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "tpu_v3/address_map.h"
#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3 {

/// RISC-V Vector configuration. Every field here is frozen by plan §4.2; they
/// are fields rather than constants so that a wrong value can be *reported*
/// instead of being impossible to express, which is what makes the negative
/// tests meaningful.
struct rvv_config {
    unsigned xlen = 32;
    unsigned vlen = 512;
    unsigned elen = 64;
    std::string version = "1.0";

    /// `vlenb` as firmware must read it: VLEN in bytes, i.e. 64.
    constexpr unsigned vlenb() const noexcept { return vlen / 8; }

    void validate(const std::string& context) const;
};

struct mxu_config {
    unsigned rows = mxu_rows;
    unsigned columns = mxu_columns;
    unsigned count_per_core = mxus_per_core;
    mxu_backend backend = mxu_backend::fast;
    /// Decision record D6. Named in the configuration because it changes the
    /// results, so a report that does not state it is not interpretable.
    mxu_arithmetic arithmetic = mxu_arithmetic::bf16_fp32;

    void validate(const std::string& context) const;
};

struct tpu_core_config {
    rvv_config rvv{};
    mxu_config mxu{};
    std::uint64_t svm_size_bytes = address_map::svm_default_capacity;

    void validate(const std::string& context) const;
};

struct tpu_chip_config {
    unsigned cores = cores_per_chip;
    std::array<tpu_core_config, cores_per_chip> core{};

    void validate(const std::string& context) const;
};

/// Where a chip or a global target sits on the mesh.
struct mesh_node {
    unsigned x = 0;
    unsigned y = 0;
};

struct tpu_soc_config {
    /// Human-readable configuration name, used in reports and in the package
    /// manifest so a result can be traced to the file that produced it.
    std::string name = "unnamed";

    unsigned mesh_x = 2;
    unsigned mesh_y = 2;

    /// Number of TPU chips instantiated. `1 .. max_chips`.
    unsigned chips = 1;

    tpu_chip_config chip{};

    std::uint64_t global_ram_size_bytes =
        address_map::global_ram_default_capacity;

    noc_timing timing = noc_timing::fast;

    unsigned mesh_nodes() const noexcept { return mesh_x * mesh_y; }

    /// Total harts: two per chip.
    unsigned harts() const noexcept { return chips * cores_per_chip; }

    /// Total MXUs: four per chip.
    unsigned mxus() const noexcept
    {
        return chips * cores_per_chip * mxus_per_core;
    }

    /// Chip `id`'s mesh node under the default row-major placement.
    ///
    /// Chips take the low node indices and the global targets take the last
    /// node; §validate() is what proves the two do not collide.
    mesh_node chip_node(chip_id_t id) const noexcept
    {
        return mesh_node{id % mesh_x, id / mesh_x};
    }

    /// The node hosting boot ROM, global control and global RAM.
    ///
    /// It is the *last* node, and it must host no chip: `noc_interconnect`
    /// refuses a target on a node that hosts an upstream port, so the global
    /// targets need a node of their own.
    mesh_node global_node() const noexcept
    {
        const unsigned last = mesh_nodes() - 1;
        return mesh_node{last % mesh_x, last / mesh_x};
    }

    void validate() const;
};

/// The mesh sizes `noc_interconnect` actually instantiates.
///
/// `floo_mesh` takes its dimensions as template parameters and is RTL-signed,
/// so `make_noc()` in `src/noc_interconnect.cpp` dispatches over a fixed list.
/// Refusing an unsupported size here, at configuration time, turns a
/// `std::runtime_error` thrown from deep inside the NoC constructor into a
/// message that names the file to edit.
bool mesh_size_supported(unsigned mesh_x, unsigned mesh_y) noexcept;

/// Multi-line human-readable description: hierarchy, counts, capacities,
/// backends and placement. This is what `tpu_v3_soc` prints, and what the
/// Phase 1 gate inspects.
std::string describe(const tpu_soc_config& config);

/// One line per mapped region, as `enumerate_regions()` produces them.
std::string describe_address_map(const tpu_soc_config& config);

} // namespace cdc::components::tpu_v3
