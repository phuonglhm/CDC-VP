// SPDX-License-Identifier: Apache-2.0
//
// The one and only definition of the TPU_V3 physical address map.
//
// Plan §12 rule: "No component may carry an independent copy of base
// addresses." That is the whole point of this header. A magic constant in a
// component, a platform, a test or a firmware header is a defect even if it
// currently holds the right value, because the copy is what drifts.
//
// `docs/ADDRESS_MAP.md` explains the layout and the reasoning; this header
// defines it and `tests/test_address_map.cpp` proves the properties both
// claim. Everything here is `constexpr` and free of SystemC.
//
// Rebaselined in Phase 3 to the D14/D15 contract: `CORE_SRAM` replaces `SVM`,
// and the two MXU control windows become `SA_CONTROL`, `DMA_CONTROL` and
// `TRANSFORM_CONTROL` for the one matrix engine, the one independent DMA and
// the one ImageTransform engine a NEO-CORE owns. The top-level, chip and core
// aperture bases and strides did not move.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3::address_map {

/// Whether widened reads are safe at a region, mirroring
/// `noc_interconnect::target_kind`. It is duplicated as a plain enum rather
/// than included from the NoC so that the map stays usable — and testable —
/// without SystemC; `region_kind_is_memory()` is what a platform translates.
enum class region_kind {
    /// Register file: 4-byte aligned accesses only, no widened reads.
    mmio,
    /// Byte-addressable storage: any width, arbitrary byte enables.
    memory,
};

// ── Top level ────────────────────────────────────────────────────────────────

inline constexpr std::uint64_t boot_rom_base = 0x0000'0000ull;
inline constexpr std::uint64_t boot_rom_size = 0x0001'0000ull;      // 64 KiB

inline constexpr std::uint64_t global_control_base = 0x0001'0000ull;
inline constexpr std::uint64_t global_control_size = 0x0001'0000ull; // 64 KiB

inline constexpr std::uint64_t global_ram_base = 0x8000'0000ull;
inline constexpr std::uint64_t global_ram_window = 0x4000'0000ull;   // 1 GiB

// ── Chip aperture ────────────────────────────────────────────────────────────

inline constexpr std::uint64_t chip_aperture_base = 0xC000'0000ull;
inline constexpr std::uint64_t chip_aperture_stride = 0x0800'0000ull; // 128 MiB

/// Eight chips fill the aperture exactly: 0xC000'0000 + 8 * 0x0800'0000 lands
/// on 0x1'0000'0000, the top of the RV32 space.
static_assert(chip_aperture_base + max_chips * chip_aperture_stride
                  == 0x1'0000'0000ull,
              "chip apertures must end exactly at the 4 GiB RV32 boundary");

inline constexpr std::uint64_t chip_control_offset = 0x0400'0000ull;
inline constexpr std::uint64_t chip_control_size = 0x0001'0000ull;    // 64 KiB

inline constexpr std::uint64_t chip_counters_offset = 0x0401'0000ull;
inline constexpr std::uint64_t chip_counters_size = 0x0001'0000ull;   // 64 KiB

// ── Core aperture ────────────────────────────────────────────────────────────

inline constexpr std::uint64_t core_aperture_stride = 0x0200'0000ull; // 32 MiB

inline constexpr std::uint64_t core_sram_offset = 0x0000'0000ull;
/// Window, not capacity.
///
/// The whole window always decodes to the core SRAM target, whatever capacity
/// is instantiated. An access above the instantiated capacity is the SRAM's
/// error to report, not a hole in the map — see `ADDRESS_MAP.md` §5 and
/// decision record D6. Mapping only the capacity would make the same address
/// unmapped in one configuration and valid in another, so a firmware pointer
/// bug would change symptom with the SRAM size.
inline constexpr std::uint64_t core_sram_window = 0x0100'0000ull;     // 16 MiB

/// Core-local MMIO, all reached through the 32-bit AXI4-Lite control plane
/// (D15). One 64 KiB register file each; the engine geometry or SRAM capacity
/// may change without any of these bases moving.
inline constexpr std::uint64_t core_control_offset = 0x0100'0000ull;
inline constexpr std::uint64_t core_control_size = 0x0001'0000ull;    // 64 KiB

inline constexpr std::uint64_t sa_control_offset = 0x0101'0000ull;
inline constexpr std::uint64_t sa_control_size = 0x0001'0000ull;      // 64 KiB

inline constexpr std::uint64_t dma_control_offset = 0x0102'0000ull;
inline constexpr std::uint64_t dma_control_size = 0x0001'0000ull;     // 64 KiB

inline constexpr std::uint64_t transform_control_offset = 0x0103'0000ull;
inline constexpr std::uint64_t transform_control_size = 0x0001'0000ull; // 64 KiB

inline constexpr std::uint64_t core_counters_offset = 0x0104'0000ull;
inline constexpr std::uint64_t core_counters_size = 0x0001'0000ull;   // 64 KiB

/// Core SRAM capacity bounds.
///
/// The reference configuration instantiates the **full 16 MiB window** per
/// core (decision record D6, superseding the temporary 4 MiB of Phase 0
/// decision P0-6). Smaller capacities stay available for explicitly labelled
/// bring-up or stress configurations; they are not the TPU_V3 reference
/// result.
inline constexpr std::uint64_t core_sram_min_capacity = 0x0000'1000ull; // 4 KiB
inline constexpr std::uint64_t core_sram_max_capacity = core_sram_window;
inline constexpr std::uint64_t core_sram_default_capacity = core_sram_window;

/// Global RAM capacity bounds. 256 MiB is a configurable bring-up default
/// (decision record D6); it is simulated backing memory and is not a model of
/// TPU v3 HBM capacity or bandwidth.
inline constexpr std::uint64_t global_ram_min_capacity = 0x0010'0000ull;  // 1 MiB
inline constexpr std::uint64_t global_ram_max_capacity = global_ram_window;
inline constexpr std::uint64_t global_ram_default_capacity = 0x1000'0000ull; // 256 MiB

// ── Derived addresses ────────────────────────────────────────────────────────
//
// Every one of these is a plain multiply-add on `std::uint64_t`, so it cannot
// wrap for any in-range argument; the range itself is what must be checked,
// and `valid_chip()` / `valid_core()` are how a caller does that. Debug builds
// assert; release builds compute a well-defined but meaningless address rather
// than invoking undefined behaviour, and the caller's own validation is the
// real guard.

constexpr bool valid_chip(chip_id_t chip) noexcept
{
    return chip < max_chips;
}

constexpr bool valid_core(core_id_t core) noexcept
{
    return core < cores_per_chip;
}

constexpr std::uint64_t chip_base(chip_id_t chip) noexcept
{
    return chip_aperture_base + std::uint64_t{chip} * chip_aperture_stride;
}

constexpr std::uint64_t chip_control(chip_id_t chip) noexcept
{
    return chip_base(chip) + chip_control_offset;
}

constexpr std::uint64_t chip_counters(chip_id_t chip) noexcept
{
    return chip_base(chip) + chip_counters_offset;
}

constexpr std::uint64_t core_base(chip_id_t chip, core_id_t core) noexcept
{
    return chip_base(chip) + std::uint64_t{core} * core_aperture_stride;
}

constexpr std::uint64_t core_sram_base(chip_id_t chip, core_id_t core) noexcept
{
    return core_base(chip, core) + core_sram_offset;
}

constexpr std::uint64_t core_control(chip_id_t chip, core_id_t core) noexcept
{
    return core_base(chip, core) + core_control_offset;
}

constexpr std::uint64_t sa_control(chip_id_t chip, core_id_t core) noexcept
{
    return core_base(chip, core) + sa_control_offset;
}

constexpr std::uint64_t dma_control(chip_id_t chip, core_id_t core) noexcept
{
    return core_base(chip, core) + dma_control_offset;
}

constexpr std::uint64_t transform_control(chip_id_t chip,
                                          core_id_t core) noexcept
{
    return core_base(chip, core) + transform_control_offset;
}

constexpr std::uint64_t core_counters(chip_id_t chip, core_id_t core) noexcept
{
    return core_base(chip, core) + core_counters_offset;
}

// ── Range helpers ────────────────────────────────────────────────────────────

/// True when `[address, address + length)` lies inside `[base, base + size)`.
///
/// The length is folded into the comparison rather than added to the address,
/// so a caller-supplied length near 2^64 cannot wrap the check into a false
/// "inside" (plan §12 rule 5). This is the reason not to write the obvious
/// `address + length <= base + size`.
constexpr bool contains(std::uint64_t base, std::uint64_t size,
                        std::uint64_t address, std::uint64_t length) noexcept
{
    if (address < base) {
        return false;
    }
    const std::uint64_t offset = address - base;
    if (offset >= size) {
        return false;
    }
    return length <= size - offset;
}

/// A named region, as enumerated for validation, reporting and NoC mapping.
struct region {
    std::uint64_t base = 0;

    /// Decoded extent. This is what gets mapped — into the address decoder,
    /// and into `noc_interconnect::add_target` — and what the non-overlap
    /// proof operates on.
    std::uint64_t size = 0;

    /// Bytes actually backed by storage, `<= size`.
    ///
    /// Equal to `size` for every region except a memory window whose
    /// instantiated capacity is smaller: core SRAM below 16 MiB, and global
    /// RAM below 1 GiB. The window still decodes; the target owns the refusal
    /// above `capacity` and must never alias the access into valid storage.
    ///
    /// Keeping the two separate is what stops the same address being unmapped
    /// in one configuration and valid in another, which would make a firmware
    /// pointer bug change symptom with the memory size.
    ///
    /// It is also not the *allocated* backing: Phase 3 storage is sparsely
    /// page-backed (D6), so a 16 MiB capacity commits 16 MiB of address space
    /// and only the touched 4 KiB pages of host memory.
    std::uint64_t capacity = 0;

    region_kind kind = region_kind::mmio;

    /// Stable identifier, e.g. `chip0.core1.sa_control`. Owned by the returned
    /// object, so it stays valid independently of the enumeration.
    std::string name;
    /// `max_chips` when the region is global.
    chip_id_t chip = max_chips;
    /// `cores_per_chip` when the region is not core-local.
    core_id_t core = cores_per_chip;

    /// True when the window is larger than the storage behind it.
    bool is_partially_backed() const noexcept { return capacity < size; }
};

/// Number of regions a core aperture contributes: SRAM plus the five
/// AXI4-Lite register files (core, SA, DMA, Transform, counters).
inline constexpr std::size_t regions_per_core = 6;
/// Chip control plus chip counters.
inline constexpr std::size_t regions_per_chip_level = 2;
/// Boot ROM, global control, global RAM.
inline constexpr std::size_t global_regions = 3;

/// Every region a system with `chips` chips exposes, global regions first,
/// then chips in ascending order.
///
/// The capacities set each region's `capacity`; they never change a `base` or
/// a `size`, so the decoded map is identical for every legal configuration.
/// Throws `std::invalid_argument` if `chips` exceeds `max_chips` or a capacity
/// is out of range or not a power of two.
std::vector<region> enumerate_regions(unsigned chips,
                                      std::uint64_t core_sram_capacity,
                                      std::uint64_t global_ram_capacity);

/// Find the region whose **decoded extent** contains `[address, length)`, or
/// `nullptr`.
///
/// An address inside a window but above its `capacity` returns that region: it
/// decodes, and the target is what refuses it. Callers that need to know
/// whether storage exists must compare against `capacity` themselves —
/// `backs()` does it.
///
/// Linear over the enumeration. It exists for tests and for reporting an
/// address in an error message; a fabric decodes with its own comparisons and
/// must not call this on a per-transaction path.
const region* find_region(const std::vector<region>& regions,
                          std::uint64_t address, std::uint64_t length) noexcept;

/// True when `[address, length)` is inside `entry` **and** backed by storage.
bool backs(const region& entry, std::uint64_t address,
           std::uint64_t length) noexcept;

constexpr bool region_kind_is_memory(region_kind kind) noexcept
{
    return kind == region_kind::memory;
}

} // namespace cdc::components::tpu_v3::address_map
