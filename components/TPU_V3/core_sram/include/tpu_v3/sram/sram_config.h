// SPDX-License-Identifier: Apache-2.0
//
// Construction-time configuration of one NEO-CORE's shared SRAM.

#pragma once

#include <cstdint>
#include <string>

#include "tpu_v3/address_map.h"

namespace cdc::components::tpu_v3::sram {

/// Where the SRAM sits and how much of it exists.
///
/// `window_bytes` and `capacity_bytes` are separate fields because they are
/// separate properties (`ADDRESS_MAP.md` §5, decision record D6). The window
/// always decodes; the capacity is what is backed. Collapsing them would make
/// an address just past the storage *unmapped* in a bring-up configuration and
/// *valid* in the reference one, so the same firmware pointer bug would give a
/// decode error on one run and silent corruption on another.
struct core_sram_config {
    /// Absolute base in the platform map, i.e.
    /// `address_map::core_sram_base(chip, core)`. There is no relative or
    /// "local alias" address for core SRAM (`ADDRESS_MAP.md` §4).
    std::uint64_t base_address = 0;

    /// Decoded extent. Frozen at 16 MiB by the map; a field rather than a
    /// constant so a test can build a small SRAM without editing the map.
    std::uint64_t window_bytes = address_map::core_sram_window;

    /// Backed extent, `<= window_bytes`. Sparsely page-backed (D6), so this
    /// is address space that *may* be written, not host memory committed.
    std::uint64_t capacity_bytes = address_map::core_sram_default_capacity;

    /// Throws `std::invalid_argument` naming the field. Called by the
    /// constructor before anything is allocated, so a bad configuration fails
    /// during elaboration rather than on the first transaction.
    void validate(const std::string& context) const;
};

} // namespace cdc::components::tpu_v3::sram
