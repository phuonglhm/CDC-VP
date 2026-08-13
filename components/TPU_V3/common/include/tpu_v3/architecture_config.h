// SPDX-License-Identifier: Apache-2.0
//
// Strongly typed TPU_V3 configuration (plan §11.1).
//
// Three rules shape this file:
//
//  * **No YAML, no CLI, no SystemC here.** Low-level components take a
//    validated object; parsing belongs to the platform. That is what keeps a
//    component unit-testable without a config file and keeps a config format
//    change out of the components.
//  * **Frozen values are rejected, not adapted.** `validate()` throws on
//    `cores != 2`, an SA count other than one, an unpromoted 128x128 geometry
//    and the rest, with a message naming the field and the accepted value. A
//    model that quietly ran a 64x64 array under the name 128x128 would produce
//    numbers indistinguishable from real ones.
//  * **Open physical values have no default.** The local-SRAM datapath width,
//    bank count and pipeline depth are pending SRAM-macro, frequency and PD
//    inputs (decision record D15), so the schema leaves them at zero and the
//    validator refuses zero. `provisional_local_sram_fabric()` is the one
//    place a concrete set of numbers exists, it is named for what it is, and
//    every report that uses it prints the values and the word "provisional".

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

/// The one Sauria matrix engine a NEO-CORE owns (decision records D14, D6).
struct sauria_matrix_config {
    /// 64x64 is the verified v4.2 bring-up array. 128x128 is the architectural
    /// destination and is accepted only after the NPU team's promotion gate,
    /// which has not run — asking for it now is a configuration error, not a
    /// silent downgrade.
    unsigned rows = sa_bringup_rows;
    unsigned columns = sa_bringup_columns;
    unsigned count_per_core = sa_per_core;

    /// D6's reference numeric contract. A bring-up run on another supported
    /// datatype must say so here, and the report and manifest repeat it.
    matrix_datatype datatype = matrix_datatype::bf16_fp32;

    /// Immutable NPU-team source revision the engine was extracted from.
    /// Empty until Phase 5 integrates one; reports print "not integrated"
    /// rather than imply a verified extraction that has not happened.
    std::string source_revision;

    /// `"64x64"` / `"128x128"`, as printed in reports and the manifest.
    std::string geometry() const;

    bool is_target_geometry() const noexcept
    {
        return rows == sa_target_rows && columns == sa_target_columns;
    }

    void validate(const std::string& context) const;
};

/// The independent TPU_V3 DMA (decision record D14). It is owned here and has
/// nothing to do with Sauria's DMA; implementation lands in Phase 4.
struct dma_config {
    unsigned count_per_core = dma_per_core;

    /// Largest chunk the DMA may hand to the external path in one transfer.
    /// The NoC refuses more than 2048 bytes at bus alignment (plan §9.3), so
    /// this is bounded by the smallest downstream limit, not by preference.
    std::uint64_t max_burst_bytes = 2048;

    void validate(const std::string& context) const;
};

/// The one ImageTransform engine (Im2Col + Col2Im).
///
/// Both operations default to unavailable, which is the honest state: the
/// standalone NPU-team Transform block has not been located, and D14 forbids
/// inferring Col2Im from PSM write ordering. An unavailable operation must be
/// refused explicitly at `start`; it must never report a fake success.
struct image_transform_config {
    unsigned count_per_core = transform_per_core;
    bool im2col_available = false;
    bool col2im_available = false;
    std::string source_revision;

    void validate(const std::string& context) const;
};

/// Physical parameters of the native local-data plane (decision record D15).
///
/// None of the three physical values has an architectural default. D15 is
/// explicit that the number of banks, the datapath width and the pipeline
/// depth follow from the target SRAM macro, the clock target and PD
/// constraints, and that an illustrative 256-bit datapath must not become a
/// constant by way of a default initializer. Zero therefore means "not
/// stated", and `validate()` refuses it.
struct local_sram_fabric_config {
    /// Width of one physical bank access, in bits. Power of two, byte
    /// multiple. Power of two because low-order interleaving indexes banks by
    /// dividing the address by this width, and a non-power-of-two divisor is
    /// not a bank decoder anyone would build.
    unsigned data_width_bits = 0;

    /// Number of independent physical banks. Power of two, for the same
    /// reason.
    unsigned bank_count = 0;

    bank_mapping mapping = bank_mapping::low_order_interleaved;

    /// Register stages between a bank and the fabric boundary. At least one:
    /// a combinational SRAM read is not a configuration that gets built, and
    /// zero is also the "not stated" value.
    unsigned pipeline_stages = 0;

    /// Revision 1 is strictly in order with one request in flight per
    /// requester (D15). Anything else needs a reordering and ownership
    /// contract that does not exist yet.
    unsigned max_outstanding_per_requester = 1;

    arbitration_policy arbitration = arbitration_policy::round_robin;

    unsigned bytes_per_beat() const noexcept { return data_width_bits / 8; }

    /// Bytes covered by one pass over every bank.
    std::uint64_t stripe_bytes() const noexcept
    {
        return std::uint64_t{bytes_per_beat()} * bank_count;
    }

    void validate(const std::string& context) const;
};

/// The concrete physical values the shipped reference configurations use.
///
/// **Provisional, not architectural.** They exist so the platform can run and
/// so contention has something to be measured against; they are placeholders
/// for the SRAM macro, clock target and PD constraints that will eventually
/// decide them. Every report that uses them prints both the values and the
/// word "provisional", and `TPU_V3_DECISION_RECORD.md` D15 is what has to
/// change before any of them may be described as frozen.
local_sram_fabric_config provisional_local_sram_fabric() noexcept;

struct tpu_core_config {
    rvv_config rvv{};
    sauria_matrix_config sa{};
    dma_config dma{};
    image_transform_config transform{};
    local_sram_fabric_config local_sram_fabric{};

    /// Instantiated core-SRAM capacity. The 16 MiB window decodes whatever
    /// this says (D6); this is only how much of it is backed.
    std::uint64_t sram_size_bytes = address_map::core_sram_default_capacity;

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

    /// One matrix engine, one DMA and one ImageTransform engine per core.
    unsigned matrix_engines() const noexcept
    {
        return chips * cores_per_chip * sa_per_core;
    }
    unsigned dma_engines() const noexcept
    {
        return chips * cores_per_chip * dma_per_core;
    }
    unsigned transform_engines() const noexcept
    {
        return chips * cores_per_chip * transform_per_core;
    }

    /// Logical storage the configuration describes: every core SRAM plus
    /// global RAM. Phase 3 backs it sparsely (D6), so this is address space,
    /// not a host-memory commitment — `mesh_4x4` is 1.25 GiB logical and a few
    /// megabytes resident until firmware touches pages.
    std::uint64_t logical_memory_bytes() const noexcept
    {
        return std::uint64_t{chips} * cores_per_chip
                * chip.core[0].sram_size_bytes
            + global_ram_size_bytes;
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
/// engines, fabric parameters and placement. This is what `tpu_v3_soc` prints.
std::string describe(const tpu_soc_config& config);

/// One line per mapped region, as `enumerate_regions()` produces them.
std::string describe_address_map(const tpu_soc_config& config);

} // namespace cdc::components::tpu_v3
